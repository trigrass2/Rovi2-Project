#include "PathPlanner_ALTO.hpp"
#include "Testbench.hpp"
#define EPSILON_BINARY 0.01
#define EPSILON_RRT_C 0.5
#define RRT_MAX_TIME 10

PathPlanner_ALTO::PathPlanner_ALTO(const string wcFile, const string deviceName):
   wcell(WorkCellLoader::Factory::load(wcFile)),
   device(wcell->findDevice(deviceName)),
   _state(wcell->getDefaultState()),
   detector(rw::common::ownedPtr(new CollisionDetector(wcell, ProximityStrategyFactory::makeDefaultCollisionStrategy()))),
   constraint(PlannerConstraint::make(detector,device,_state)),
   sampler(QSampler::makeConstrained(QSampler::makeUniform(device),constraint.getQConstraintPtr())),
   metric(MetricFactory::makeEuclidean<Q>()),
   obstacle((MovableFrame*) wcell->findFrame("Obstacle"))
{
/*
    wcell = WorkCellLoader::Factory::load(wcFile);
    device = wcell->findDevice(deviceName);
    obstacle = (MovableFrame*) wcell->findFrame("Obstacle");

    if (device == NULL) {
        cerr << "Device: " << deviceName << " not found!" << endl;
    }
    if (device == NULL) {
        cerr << "Obstacle not found!" << endl;
    }

    _state = wcell->getDefaultState();   // Get default state
    detector = new CollisionDetector(wcell, ProximityStrategyFactory::makeDefaultCollisionStrategy());  // Create detector for collision detection
    constraint = PlannerConstraint::make(detector,device,_state);
    sampler = QSampler::makeConstrained(QSampler::makeUniform(device),constraint.getQConstraintPtr());
    metric = MetricFactory::makeEuclidean<Q>();
*/
    //rrtPlanner = new RRT(constraint, sampler, metric, 0.5);

    rw::math::Math::seed();

    // Load obstacle motion
    motionCounter = 0;
    moveObstacle(-0.595,0.000,1.717);   // Start ball position in scene
}

QPath PathPlanner_ALTO::getMainPath(){
   return mainPath;
}

void PathPlanner_ALTO::moveObstacle(double x, double y, double z) {
    double roll = 0.000;
    double pitch = 0.000;
    double yaw = 0.000;

    RPY<double> rpy(roll,pitch,yaw);   // Create RPY matrix
    Vector3D<double> xyz(x,y,z);   // Create translation vector
    Transform3D<double> t_matrix(xyz, rpy.toRotation3D() ); // Create a transformation matrix from the RPY and XYZ

    obstacle = (MovableFrame*) wcell->findFrame("Obstacle");
    obstacle->moveTo(t_matrix,_state);
}

QPath PathPlanner_ALTO::getPath(rw::math::Q from, rw::math::Q to, double extend, int maxtime){


    //device->setQ(from,_state);

    PlannerConstraint constraint = PlannerConstraint::make(detector,device,_state);

    QToQPlanner::Ptr planner;
    Timer t;

    if (checkCollisions(device, _state, from))
        return 0;
    if (checkCollisions(device, _state, to))
        return 0;

    /*
    QPath path;

    planner = RRTPlanner::makeQToQPlanner(constraint, sampler, metric, extend, RRTPlanner::RRTConnect);
    //planner = Z3Planner::makeQToQPlanner(constraint,device);
    //planner = ARWPlanner::makeQToQPlanner(constraint,device,0,-1,-1);

    t.resetAndResume();
    planner->query(from,to,path,maxtime);
    t.pause();

    if (t.getTime() >= maxtime) {
       cout << "Notice: max time of " << maxtime << " seconds reached." << endl;
   }*/



    rrtPlanner = new RRT(constraint, sampler, metric, EPSILON_BINARY);
    QPath path = rrtPlanner->rrtConnectPlanner(from, to, extend, maxtime);

    if(path.size() == 0)
    {
        path.push_back(from);
        path.push_back(to);
        cout << "RRT planner: could not find a path!" << endl;
    }
    //cout << "Path1" << endl;
    //printPath(path);
    //cout << "\n\nPath2" << endl;
    //printPath(path2);

    return path;
}

void PathPlanner_ALTO::printPath(QPath path){
    cout << "size: " << path.size() << endl;
    for(uint i = 0; i<path.size(); i++)
        cout << path[i] << endl;
}

bool PathPlanner_ALTO::checkCollisions(Device::Ptr device, const State &state, /*const CollisionDetector &detector,*/ const Q &q) {
    State testState;
    CollisionDetector::QueryResult data;
    bool colFrom;

    testState = state;
    device->setQ(q,testState);
    colFrom = detector->inCollision(testState,&data);
    if (colFrom) {
        //cerr << "Configuration in collision: " << q << endl;
        //cerr << "Colliding frames: " << endl;
        FramePairSet fps = data.collidingFrames;
        for (FramePairSet::iterator it = fps.begin(); it != fps.end(); it++) {
            //cerr << (*it).first->getName() << " " << (*it).second->getName() << endl;
        }
        return true;
    }
    return false;
}

QPath PathPlanner_ALTO::correctionPlanner(uint limit, int minimumThreshold)
{
    QPath newPath;

    CollisionDetector detector(wcell, ProximityStrategyFactory::makeDefaultCollisionStrategy());

    uint i = limit + 1;  // Index of first collision

    while(checkCollisions(device, _state, /*detector,*/ workingPath[i]) && i < workingPath.size())
    {
        if(i + 1 >= workingPath.size())
            break;
        else
            i++;
    }

    QPath bypass = getPath(workingPath[limit], workingPath[i], EPSILON_RRT_C, RRT_MAX_TIME);  // Vi skal have indstillet epislon og max time

    // push first collision free part of workingPath into tempPath
    for(uint h = 0; h < limit; h++){
        newPath.push_back(workingPath[h]);
        //cout << workingPath[h] << "   gammel" << endl;
    }

    for(uint k = 0; k < bypass.size(); k++){
        newPath.push_back(bypass[k]);
        //cout << bypass[k] << "   ny" << endl;
    }

    // push remaining part of workingPath into tempPath
    for(uint j = i+1; j < workingPath.size(); j++){
        newPath.push_back(workingPath[j]);
        //cout << workingPath[j] << "   gammel" << endl;
    }
    workingPath = newPath;
    return workingPath;
}


void PathPlanner_ALTO::writeMainPathToFile(std::string filepath)
{
    ofstream myfile;
    myfile.open(filepath);
    for (QPath::iterator it = mainPath.begin(); it < mainPath.end(); it++) {
        myfile << (*it)[0] << "," << (*it)[1] << "," << (*it)[2] << "," << (*it)[3] << "," << (*it)[4] << "," << (*it)[5] << "\n";
    }
    myfile.close();

}

QPath PathPlanner_ALTO::readMainPathFromFile(std::string filepath) {

    vector<double> state_vec;
    std::string line, token;
    std::string::size_type sz;
    std::ifstream myfile;

    mainPath.clear();

    myfile.open(filepath);
    if (myfile) {
        while (getline( myfile, line )) {
            state_vec.clear();
            std::istringstream ss(line);
            while(getline(ss, token, ',')) {
                state_vec.push_back( std::stod(token,&sz));
            }
        mainPath.push_back(Q(6, state_vec[0],state_vec[1],state_vec[2],state_vec[3],state_vec[4],state_vec[5]));
        //cout << temp << endl;
        }
        myfile.close();
    }
    else
        cout << "Could not read file! " << endl;


    workingPath = mainPath;
    return workingPath;
    /*
    for (QPath::iterator it = mainPath.begin(); it < mainPath.end(); it++) {
        cout << *it << endl;
    }*/
}

QPath PathPlanner_ALTO::readBallPathFromFile(std::string filepath)
{
    vector<double> state_vec;
    std::string line, token;
    std::string::size_type sz;
    std::ifstream myfile;
    QPath ballPath;

    myfile.open(filepath);
    if (myfile) {
        while (getline( myfile, line )) {
            state_vec.clear();
            std::istringstream ss(line);
            while(getline(ss, token, ',')) {
                state_vec.push_back( std::stod(token,&sz));
            }
        ballPath.push_back(Q(3, state_vec[0],state_vec[1],state_vec[2]));

        }
        myfile.close();
    }
    else
        cout << "Could not read file! " << endl;

    return ballPath;
}


void PathPlanner_ALTO::writePathToFile(QPath &path, std::string filepath)
{
    ofstream myfile;
    myfile.open(filepath);
    for (QPath::iterator it = path.begin(); it < path.end(); it++) {
        myfile << (*it)[0] << "," << (*it)[1] << "," << (*it)[2] << "," << (*it)[3] << "," << (*it)[4] << "," << (*it)[5] << "\n";
    }
    myfile.close();

}

void PathPlanner_ALTO::setMainPath(QPath path)
{
    this->mainPath = path;
}

void PathPlanner_ALTO::setWorkingPath(QPath path)
{
    this->workingPath = path;
}


int PathPlanner_ALTO::preChecker(Q ballPosition, int presentIndex){
    EdgeCollisionDetectors _edgeCollisionDetect(constraint, EPSILON_BINARY);

    for(uint i = presentIndex; i < workingPath.size(); i++) {
        if(checkCollisions(device, _state, /*detector,*/ workingPath[i])) {
            return i - 1;
        }

        // Binary egde collision checker
        if(i > presentIndex)
        {
            if(_edgeCollisionDetect.inCollisionBinary(workingPath[i - 1], workingPath[i]))
                return i - 1;
        }
    }
    return -1;
}

QPath PathPlanner_ALTO::getInterpolatedPath(Q from, Q to, double epsilon, int maxtime, double minPieces)
{
    QPath path = getPath(from, to, epsilon, maxtime);
    QPath newPath;
    //printPath(path);

    for(uint i = 0; i<path.size()-1; i++)
    {
        Q deltaQ = path[i+1]-path[i];
        double length = deltaQ.norm2();
        int n = ceil( length/minPieces );   // Number of pieces to split the path into
        Q unitVec = deltaQ/length;

        // Loop number of pieces
        for(int j = 1; j<=n; j++)
        {
            if(j == 1)
                newPath.push_back(path[i]);
            else if(j == n)
                newPath.push_back(path[i+1]);
            else
                newPath.push_back(path[i] + unitVec*minPieces);
        }
    }
    //cout << endl;
    //printPath(newPath);
    return newPath;
}
