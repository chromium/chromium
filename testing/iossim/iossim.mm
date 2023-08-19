// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#include <getopt.h>

#include <string>

namespace {

void PrintUsage() {
  fprintf(
      stderr,
      "Usage: iossim [-d device] [-s sdk_version] <app_path> <xctest_path>\n"
      "  where <app_path> is the path to the .app directory and <xctest_path> "
      "is the path to an optional xctest bundle.\n"
      "Options:\n"
      "  -u  Specifies the device udid to use. Will use -d, -s values to get "
      "devices if not specified.\n"
      "  -d  Specifies the device (must be one of the values from the iOS "
      "Simulator's Hardware -> Device menu. Defaults to 'iPhone 6s'.\n"
      "  -w  Wipe the device's contents and settings before running the "
      "test.\n"
      "  -e  Specifies an environment key=value pair that will be"
      " set in the simulated application's environment.\n"
      "  -t  Specifies a test or test suite that should be included in the "
      "test run. All other tests will be excluded from this run.\n"
      "  -c  Specifies command line flags to pass to application.\n"
      "  -p  Print the device's home directory, does not run a test.\n"
      "  -s  Specifies the SDK version to use (e.g '9.3'). Will use system "
      "default if not specified.\n");
}

// Exit status codes.
const int kExitSuccess = EXIT_SUCCESS;
const int kExitInvalidArguments = 2;

void LogError(NSString* format, ...) {
  va_list list;
  va_start(list, format);

  NSString* message = [[NSString alloc] initWithFormat:format arguments:list];

  fprintf(stderr, "iossim: ERROR: %s\n", [message UTF8String]);
  fflush(stderr);

  va_end(list);
}

}

// See https://stackoverflow.com/a/51895129 and
// https://github.com/facebook/xctool/pull/159/files.
@interface NSTask (PrivateAPI)
- (void)setStartsNewProcessGroup:(BOOL)startsNewProcessGroup;
@end

// Wrap boiler plate calls to xcrun NSTasks.
@interface XCRunTask : NSObject
- (instancetype)initWithArguments:(NSArray*)arguments;
- (void)run;
- (void)setStandardOutput:(id)output;
- (void)setStandardError:(id)error;
- (int)terminationStatus;
@end

@implementation XCRunTask {
  NSTask* __strong _task;
}

- (instancetype)initWithArguments:(NSArray*)arguments {
  self = [super init];
  if (self) {
    _task = [[NSTask alloc] init];
    [_task setStartsNewProcessGroup:NO];
    _task.launchPath = @"/usr/bin/xcrun";
    _task.arguments = arguments;
  }
  return self;
}

- (void)setStandardOutput:(id)output {
  _task.standardOutput = output;
}

- (void)setStandardError:(id)error {
  _task.standardError = error;
}

- (int)terminationStatus {
  return _task.terminationStatus;
}

- (void)run {
  [_task launch];
  [_task waitUntilExit];
}

- (void)launch {
  [_task launch];
}

- (void)waitUntilExit {
  [_task waitUntilExit];
}

@end

// Return array of available iOS runtime dictionaries.  Unavailable (old Xcode
// versions) or other runtimes (tvOS, watchOS) are removed.
NSArray* Runtimes(NSDictionary* simctl_list) {
  NSMutableArray* runtimes = [simctl_list[@"runtimes"] mutableCopy];
  for (NSDictionary* runtime in simctl_list[@"runtimes"]) {
    BOOL available =
        [runtime[@"availability"] isEqualToString:@"(available)"] ||
        runtime[@"isAvailable"];

    if (![runtime[@"identifier"]
            hasPrefix:@"com.apple.CoreSimulator.SimRuntime.iOS"] ||
        !available) {
      [runtimes removeObject:runtime];
    }
  }
  return runtimes;
}

// Return array of device dictionaries.
NSArray* Devices(NSDictionary* simctl_list) {
  NSMutableArray* devicetypes = [simctl_list[@"devicetypes"] mutableCopy];
  for (NSDictionary* devicetype in simctl_list[@"devicetypes"]) {
    if (![devicetype[@"identifier"]
            hasPrefix:@"com.apple.CoreSimulator.SimDeviceType.iPad"] &&
        ![devicetype[@"identifier"]
            hasPrefix:@"com.apple.CoreSimulator.SimDeviceType.iPhone"]) {
      [devicetypes removeObject:devicetype];
    }
  }
  return devicetypes;
}

// Get list of devices, runtimes, etc from sim_ctl.
NSDictionary* GetSimulatorList() {
  XCRunTask* task =
      [[XCRunTask alloc] initWithArguments:@[ @"simctl", @"list", @"-j" ]];
  NSPipe* out = [NSPipe pipe];
  task.standardOutput = out;

  // In the rest of the this file we read from the pipe after -waitUntilExit
  // (We normally wrap -launch and -waitUntilExit in one -run method).  However,
  // on some swarming slaves this led to a hang on simctl's pipe.  Since the
  // output of simctl is so instant, reading it before exit seems to work, and
  // seems to avoid the hang.
  [task launch];
  NSData* data = [out.fileHandleForReading readDataToEndOfFile];
  [task waitUntilExit];

  NSError* error = nil;
  return [NSJSONSerialization JSONObjectWithData:data
                                         options:kNilOptions
                                           error:&error];
}

// List supported runtimes and devices.
void PrintSupportedDevices(NSDictionary* simctl_list) {
  printf("\niOS devices:\n");
  for (NSDictionary* type in Devices(simctl_list)) {
    printf("%s\n", [type[@"name"] UTF8String]);
  }
  printf("\nruntimes:\n");
  for (NSDictionary* runtime in Runtimes(simctl_list)) {
    printf("%s\n", [runtime[@"version"] UTF8String]);
  }
}

// Expand path to absolute path.
NSString* ResolvePath(NSString* path) {
  path = path.stringByExpandingTildeInPath;
  path = path.stringByStandardizingPath;
  const char* cpath = path.UTF8String;
  char* resolved_name = nullptr;
  char* abs_path = realpath(cpath, resolved_name);
  if (abs_path == nullptr) {
    return nil;
  }
  return @(abs_path);
}

// Search |simctl_list| for a udid matching |device_name| and |sdk_version|.
NSString* GetDeviceBySDKAndName(NSDictionary* simctl_list,
                                NSString* device_name,
                                NSString* sdk_version) {
  NSString* sdk = nil;
  NSString* name = nil;
  // Get runtime identifier based on version property to handle
  // cases when version and identifier are not the same,
  // e.g. below identifer is *13-2 but version is 13.2.2
  // {
  //   "version" : "13.2.2",
  //   "bundlePath" : "path"
  //   "identifier" : "com.apple.CoreSimulator.SimRuntime.iOS-13-2",
  //   "buildversion" : "17K90"
  // }
  for (NSDictionary* runtime in Runtimes(simctl_list)) {
    if ([runtime[@"version"] isEqualToString:sdk_version]) {
      sdk = runtime[@"identifier"];
      name = runtime[@"name"];
      break;
    }
  }
  if (sdk == nil) {
    printf("\nDid not find Runtime with specified version.\n");
    PrintSupportedDevices(simctl_list);
    exit(kExitInvalidArguments);
  }
  NSArray* devices = [simctl_list[@"devices"] objectForKey:sdk];
  if (devices == nil || devices.count == 0) {
    // Specific for XCode 10.1 and lower,
    // where name from 'runtimes' uses as a key in 'devices'.
    devices = [simctl_list[@"devices"] objectForKey:name];
  }
  for (NSDictionary* device in devices) {
    if ([device[@"name"] isEqualToString:device_name]) {
      return device[@"udid"];
    }
  }
  return nil;
}

// Create and return a device udid of |device| and |sdk_version|.
NSString* CreateDeviceBySDKAndName(NSString* device, NSString* sdk_version) {
  NSString* sdk = [@"iOS" stringByAppendingString:sdk_version];
  XCRunTask* create = [[XCRunTask alloc]
      initWithArguments:@[ @"simctl", @"create", device, device, sdk ]];
  [create run];

  NSDictionary* simctl_list = GetSimulatorList();
  return GetDeviceBySDKAndName(simctl_list, device, sdk_version);
}

bool FindDeviceByUDID(NSDictionary* simctl_list, NSString* udid) {
  NSDictionary* devices_table = simctl_list[@"devices"];
  for (id runtimes in devices_table) {
    NSArray* devices = devices_table[runtimes];
    for (NSDictionary* device in devices) {
      if ([device[@"udid"] isEqualToString:udid]) {
        return true;
      }
    }
  }
  return false;
}

// Prints the HOME environment variable for a device.  Used by the bots to
// package up all the test data.
void PrintDeviceHome(NSString* udid) {
  XCRunTask* task = [[XCRunTask alloc]
      initWithArguments:@[ @"simctl", @"getenv", udid, @"HOME" ]];
  [task run];
}

// Erase a device, used by the bots before a clean test run.
void WipeDevice(NSString* udid) {
  XCRunTask* shutdown =
      [[XCRunTask alloc] initWithArguments:@[ @"simctl", @"shutdown", udid ]];
  shutdown.standardOutput = nil;
  shutdown.standardError = nil;
  [shutdown run];

  XCRunTask* erase =
      [[XCRunTask alloc] initWithArguments:@[ @"simctl", @"erase", udid ]];
  [erase run];
}

void KillSimulator() {
  XCRunTask* task =
      [[XCRunTask alloc] initWithArguments:@[ @"killall", @"Simulator" ]];
  task.standardOutput = nil;
  task.standardError = nil;
  [task run];
}

int RunApplication(NSString* app_path,
                   NSString* xctest_path,
                   NSString* udid,
                   NSMutableDictionary* app_env,
                   NSMutableArray* cmd_args,
                   NSMutableArray* tests_filter) {
  NSString* tempFilePath = [NSTemporaryDirectory()
      stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
  [NSFileManager.defaultManager createFileAtPath:tempFilePath
                                        contents:nil
                                      attributes:nil];

  NSMutableDictionary* xctestrun = [NSMutableDictionary dictionary];
  NSMutableDictionary* testTargetName = [NSMutableDictionary dictionary];

  NSMutableDictionary* testingEnvironmentVariables =
      [NSMutableDictionary dictionary];
  testingEnvironmentVariables[@"IDEiPhoneInternalTestBundleName"] =
      app_path.lastPathComponent;

  testingEnvironmentVariables[@"DYLD_FRAMEWORK_PATH"] =
      @"__TESTROOT__/Debug-iphonesimulator:__PLATFORMS__/"
      @"iPhoneSimulator.platform/Developer/Library/Frameworks";
  testingEnvironmentVariables[@"DYLD_LIBRARY_PATH"] =
      @"__TESTROOT__/Debug-iphonesimulator:__PLATFORMS__/"
      @"iPhoneSimulator.platform/Developer/Library";

  if (xctest_path) {
    testTargetName[@"TestBundlePath"] = xctest_path;
    testingEnvironmentVariables[@"DYLD_INSERT_LIBRARIES"] =
        @"__PLATFORMS__/iPhoneSimulator.platform/Developer/"
        @"usr/lib/libXCTestBundleInject.dylib";
    testingEnvironmentVariables[@"XCInjectBundleInto"] =
        [NSString stringWithFormat:@"__TESTHOST__/%@",
                                   app_path.lastPathComponent
                                       .stringByDeletingPathExtension];
  } else {
    testTargetName[@"TestBundlePath"] = app_path;
  }
  testTargetName[@"TestHostPath"] = app_path;

  if (app_env.count) {
    testTargetName[@"EnvironmentVariables"] = app_env;
  }

  if (cmd_args.count > 0) {
    testTargetName[@"CommandLineArguments"] = cmd_args;
  }

  if (tests_filter.count > 0) {
    testTargetName[@"OnlyTestIdentifiers"] = tests_filter;
  }

  testTargetName[@"TestingEnvironmentVariables"] = testingEnvironmentVariables;
  xctestrun[@"TestTargetName"] = testTargetName;

  NSData* data = [NSPropertyListSerialization
      dataWithPropertyList:xctestrun
                    format:NSPropertyListXMLFormat_v1_0
                   options:0
                     error:nil];
  [data writeToFile:tempFilePath atomically:YES];

  XCRunTask* task = [[XCRunTask alloc] initWithArguments:@[
    @"xcodebuild", @"-xctestrun", tempFilePath, @"-destination",
    [@"platform=iOS Simulator,id=" stringByAppendingString:udid],
    @"test-without-building"
  ]];

  if (!xctest_path) {
    // The following stderr messages are meaningless on iossim when not running
    // xctests and can be safely stripped.
    NSArray* ignore_strings = @[
      @"IDETestOperationsObserverErrorDomain", @"** TEST EXECUTE FAILED **"
    ];
    NSPipe* stderr_pipe = [NSPipe pipe];
    stderr_pipe.fileHandleForReading.readabilityHandler =
        ^(NSFileHandle* handle) {
          NSString* log = [[NSString alloc] initWithData:handle.availableData
                                                encoding:NSUTF8StringEncoding];
          for (NSString* ignore_string in ignore_strings) {
            if ([log rangeOfString:ignore_string].location != NSNotFound) {
              return;
            }
          }
          printf("%s", log.UTF8String);
        };
    task.standardError = stderr_pipe;
  }
  [task run];
  return [task terminationStatus];
}

int main(int argc, char* const argv[]) {
  // When the last running simulator is from Xcode 7, an Xcode 8 run will yield
  // a failure to "unload a stale CoreSimulatorService job" message.  Sending a
  // hidden simctl to do something simple (list devices) helpfully works around
  // this issue.
  XCRunTask* workaround_task =
      [[XCRunTask alloc] initWithArguments:@[ @"simctl", @"list", @"-j" ]];
  workaround_task.standardOutput = nil;
  workaround_task.standardError = nil;
  [workaround_task run];

  NSString* app_path = nil;
  NSString* xctest_path = nil;
  NSString* udid = nil;
  NSString* device_name = @"iPhone 6s";
  bool wants_wipe = false;
  bool wants_print_home = false;
  NSDictionary* simctl_list = GetSimulatorList();
  float sdk = 0;
  for (NSDictionary* runtime in Runtimes(simctl_list)) {
    sdk = fmax(sdk, [runtime[@"version"] floatValue]);
  }
  NSString* sdk_version = [NSString stringWithFormat:@"%0.1f", sdk];
  NSMutableDictionary* app_env = [NSMutableDictionary dictionary];
  NSMutableArray* cmd_args = [NSMutableArray array];
  NSMutableArray* tests_filter = [NSMutableArray array];

  int c;
  while ((c = getopt(argc, argv, "hs:d:u:t:e:c:pwl")) != -1) {
    switch (c) {
      case 's':
        sdk_version = @(optarg);
        break;
      case 'd':
        device_name = @(optarg);
        break;
      case 'u':
        udid = @(optarg);
        break;
      case 'w':
        wants_wipe = true;
        break;
      case 'c': {
        NSString* cmd_arg = @(optarg);
        [cmd_args addObject:cmd_arg];
      } break;
      case 't': {
        NSString* test = @(optarg);
        [tests_filter addObject:test];
      } break;
      case 'e': {
        NSString* envLine = @(optarg);
        NSRange range = [envLine rangeOfString:@"="];
        if (range.location == NSNotFound) {
          LogError(@"Invalid key=value argument for -e.");
          PrintUsage();
          exit(kExitInvalidArguments);
        }
        NSString* key = [envLine substringToIndex:range.location];
        NSString* value = [envLine substringFromIndex:(range.location + 1)];
        [app_env setObject:value forKey:key];
      } break;
      case 'p':
        wants_print_home = true;
        break;
      case 'l':
        PrintSupportedDevices(simctl_list);
        exit(kExitSuccess);
      case 'h':
        PrintUsage();
        exit(kExitSuccess);
      default:
        PrintUsage();
        exit(kExitInvalidArguments);
    }
  }

  if (udid == nil) {
    udid = GetDeviceBySDKAndName(simctl_list, device_name, sdk_version);
    if (udid == nil) {
      udid = CreateDeviceBySDKAndName(device_name, sdk_version);
      if (udid == nil) {
        LogError(@"Unable to find a device %@ with SDK %@.", device_name,
                 sdk_version);
        PrintSupportedDevices(simctl_list);
        exit(kExitInvalidArguments);
      }
    }
  } else {
    if (!FindDeviceByUDID(simctl_list, udid)) {
      LogError(
          @"Unable to find a device with udid %@. Use 'xcrun simctl list' to "
          @"see valid device udids.",
          udid);
      exit(kExitInvalidArguments);
    }
  }

  if (wants_print_home) {
    PrintDeviceHome(udid);
    exit(kExitSuccess);
  }

  KillSimulator();
  if (wants_wipe) {
    WipeDevice(udid);
    printf("Device wiped.\n");
    exit(kExitSuccess);
  }

  // There should be at least one arg left, specifying the app path. Any
  // additional args are passed as arguments to the app.
  if (optind < argc) {
    NSString* unresolved_app_path = [NSFileManager.defaultManager
        stringWithFileSystemRepresentation:argv[optind]
                                    length:strlen(argv[optind])];
    app_path = ResolvePath(unresolved_app_path);
    if (!app_path) {
      LogError(@"Unable to resolve app_path %@", unresolved_app_path);
      exit(kExitInvalidArguments);
    }

    if (++optind < argc) {
      NSString* unresolved_xctest_path = [NSFileManager.defaultManager
          stringWithFileSystemRepresentation:argv[optind]
                                      length:strlen(argv[optind])];
      xctest_path = ResolvePath(unresolved_xctest_path);
      if (!xctest_path) {
        LogError(@"Unable to resolve xctest_path %@", unresolved_xctest_path);
        exit(kExitInvalidArguments);
      }
    }
  } else {
    LogError(@"Unable to parse command line arguments.");
    PrintUsage();
    exit(kExitInvalidArguments);
  }

  int return_code = RunApplication(app_path, xctest_path, udid, app_env,
                                   cmd_args, tests_filter);
  KillSimulator();
  return return_code;
}
