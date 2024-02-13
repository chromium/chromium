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
      "test run. All other tests will be excluded from this run. This is "
      "incompatible with -i.\n"
      "  -c  Specifies command line flags to pass to application.\n"
      "  -p  Print the device's home directory, does not run a test.\n"
      "  -s  Specifies the SDK version to use (e.g '9.3'). Will use system "
      "default if not specified.\n"
      "  -v  Be more verbose, showing all the xcrun commands we call\n"
      "  -k  When to kill the iOS Simulator : before, after, both, never "
      "(default: both)\n"
      "  -i  Use iossim instead of xcodebuild (disables all xctest "
      "features). This is incompatible with -t.\n");
}

// Exit status codes.
const int kExitSuccess = EXIT_SUCCESS;
const int kExitInvalidArguments = 2;

void LogError(NSString* format, ...) {
  va_list list;
  va_start(list, format);

  NSString* message = [[NSString alloc] initWithFormat:format arguments:list];

  NSLog(@"ERROR: %@", message);

  va_end(list);
}

}

typedef enum {
  KILL_NEVER = 0,
  KILL_BEFORE = 1 << 0,
  KILL_AFTER = 1 << 1,
  KILL_BOTH = KILL_BEFORE | KILL_AFTER,
} SimulatorKill;

// See https://stackoverflow.com/a/51895129 and
// https://github.com/facebook/xctool/pull/159/files.
@interface NSTask (PrivateAPI)
- (void)setStartsNewProcessGroup:(BOOL)startsNewProcessGroup;
@end

// Wrap boiler plate calls to xcrun NSTasks.
@interface XCRunTask : NSObject
- (instancetype)initWithArguments:(NSArray*)arguments;
- (void)run:(bool)verbose;
- (void)launch:(bool)verbose;
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

- (void)run:(bool)verbose {
  if (verbose) {
    NSLog(@"Running xcrun %@", [_task.arguments componentsJoinedByString:@" "]);
  }
  [_task launch];
  [_task waitUntilExit];
}

- (void)launch:(bool)verbose {
  if (verbose) {
    NSLog(@"Running xcrun %@", [_task.arguments componentsJoinedByString:@" "]);
  }
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
NSDictionary* GetSimulatorList(bool verbose) {
  XCRunTask* task =
      [[XCRunTask alloc] initWithArguments:@[ @"simctl", @"list", @"-j" ]];
  NSPipe* out = [NSPipe pipe];
  task.standardOutput = out;

  // In the rest of the this file we read from the pipe after -waitUntilExit
  // (We normally wrap -launch and -waitUntilExit in one -run method).  However,
  // on some swarming slaves this led to a hang on simctl's pipe.  Since the
  // output of simctl is so instant, reading it before exit seems to work, and
  // seems to avoid the hang.
  [task launch:verbose];
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
NSString* CreateDeviceBySDKAndName(NSString* device,
                                   NSString* sdk_version,
                                   bool verbose) {
  NSString* sdk = [@"iOS" stringByAppendingString:sdk_version];
  XCRunTask* create = [[XCRunTask alloc]
      initWithArguments:@[ @"simctl", @"create", device, device, sdk ]];
  [create run:verbose];

  NSDictionary* simctl_list = GetSimulatorList(verbose);
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
void PrintDeviceHome(NSString* udid, bool verbose) {
  XCRunTask* task = [[XCRunTask alloc]
      initWithArguments:@[ @"simctl", @"getenv", udid, @"HOME" ]];
  [task run:verbose];
}

// Erase a device, used by the bots before a clean test run.
void WipeDevice(NSString* udid, bool verbose) {
  XCRunTask* shutdown =
      [[XCRunTask alloc] initWithArguments:@[ @"simctl", @"shutdown", udid ]];
  shutdown.standardOutput = nil;
  shutdown.standardError = nil;
  [shutdown run:verbose];

  XCRunTask* erase =
      [[XCRunTask alloc] initWithArguments:@[ @"simctl", @"erase", udid ]];
  [erase run:verbose];
}

void KillSimulator(bool verbose) {
  XCRunTask* task =
      [[XCRunTask alloc] initWithArguments:@[ @"killall", @"Simulator" ]];
  task.standardOutput = nil;
  task.standardError = nil;
  [task run:verbose];
}

NSString* GetBundleIdentifierFromPath(NSString* app_path) {
  NSFileManager* file_manager = [NSFileManager defaultManager];
  NSString* info_plist_path =
      [app_path stringByAppendingPathComponent:@"Info.plist"];
  if (![file_manager fileExistsAtPath:info_plist_path]) {
    return nil;
  }

  NSDictionary* info_dictionary =
      [NSDictionary dictionaryWithContentsOfFile:info_plist_path];
  NSString* bundle_identifier = info_dictionary[@"CFBundleIdentifier"];
  return bundle_identifier;
}

int RunSimCtl(NSArray* arguments, bool verbose) {
  XCRunTask* task = [[XCRunTask alloc]
      initWithArguments:[@[ @"simctl" ]
                            arrayByAddingObjectsFromArray:arguments]];
  [task run:verbose];
  int ret = [task terminationStatus];
  if (ret) {
    NSLog(@"Warning: the following command failed: xcrun simctl %@",
          [arguments componentsJoinedByString:@" "]);
  }
  return ret;
}

void PrepareWebTests(NSString* udid, NSString* app_path, bool verbose) {
  NSString* bundle_identifier = GetBundleIdentifierFromPath(app_path);

  RunSimCtl(@[ @"uninstall", udid, bundle_identifier ], verbose);
  RunSimCtl(@[ @"install", udid, app_path ], verbose);
}

int RunWebTest(NSString* app_path,
               NSString* udid,
               NSMutableArray* cmd_args,
               bool verbose) {
  NSMutableArray* arguments = [NSMutableArray array];
  [arguments addObject:@"simctl"];
  [arguments addObject:@"launch"];
  [arguments addObject:@"--console"];
  [arguments addObject:@"--terminate-running-process"];
  [arguments addObject:udid];
  [arguments addObject:GetBundleIdentifierFromPath(app_path)];
  if (cmd_args.count == 1) {
    for (NSString* arg in [cmd_args[0] componentsSeparatedByString:@" "]) {
      [arguments addObject:arg];
    }
  }
  [arguments addObject:@"-"];
  XCRunTask* task = [[XCRunTask alloc] initWithArguments:arguments];

  // The following stderr message causes a lot of test faiures on the web
  // tests. Strip the message here.
  NSArray* ignore_strings = @[ @"Class SwapLayerEAGL" ];
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
        fprintf(stderr, "%s", log.UTF8String);
      };
  task.standardError = stderr_pipe;

  [task run:verbose];
  return [task terminationStatus];
}

bool isSimDeviceBooted(NSDictionary* simctl_list, NSString* udid) {
  for (NSString* sdk in simctl_list[@"devices"]) {
    for (NSDictionary* device in simctl_list[@"devices"][sdk]) {
      if ([device[@"udid"] isEqualToString:udid]) {
        if ([device[@"state"] isEqualToString:@"Booted"]) {
          return true;
        }
      }
    }
  }
  return false;
}

int SimpleRunApplication(NSString* app_path,
                         NSString* udid,
                         NSMutableArray* cmd_args,
                         bool verbose) {
  NSString* bundle_id = GetBundleIdentifierFromPath(app_path);

  RunSimCtl(@[ @"uninstall", udid, bundle_id ], verbose);
  RunSimCtl(@[ @"install", udid, app_path ], verbose);

  NSArray* command = [@[
    @"launch", @"--console", @"--terminate-running-process", udid, bundle_id
  ] arrayByAddingObjectsFromArray:cmd_args];
  return RunSimCtl(command, verbose);
}

int RunApplication(NSString* app_path,
                   NSString* xctest_path,
                   NSString* udid,
                   NSMutableDictionary* app_env,
                   NSMutableArray* cmd_args,
                   NSMutableArray* tests_filter,
                   bool verbose) {
  NSString* filename =
      [NSUUID.UUID.UUIDString stringByAppendingString:@".xctestrun"];
  NSString* tempFilePath =
      [NSTemporaryDirectory() stringByAppendingPathComponent:filename];
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
  [task run:verbose];
  return [task terminationStatus];
}

int main(int argc, char* const argv[]) {
  NSString* app_path = nil;
  NSString* xctest_path = nil;
  NSString* udid = nil;
  NSString* device_name = @"iPhone 6s";
  bool wants_wipe = false;
  bool wants_print_home = false;
  bool wants_print_supported_devices = false;
  bool run_web_test = false;
  bool prepare_web_test = false;
  NSString* sdk_version = nil;
  NSMutableDictionary* app_env = [NSMutableDictionary dictionary];
  NSMutableArray* cmd_args = [NSMutableArray array];
  NSMutableArray* tests_filter = [NSMutableArray array];
  bool verbose_commands = false;
  SimulatorKill kill_simulator = KILL_BOTH;
  bool wants_simple_iossim = false;

  int c;
  while ((c = getopt(argc, argv, "hs:d:u:t:e:c:pwlvk:i")) != -1) {
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
        wants_print_supported_devices = true;
        break;
      case 'v':
        verbose_commands = true;
        break;
      case 'k': {
        NSString* cmd_arg = @(optarg);
        if ([cmd_arg isEqualToString:@"before"]) {
          kill_simulator = KILL_BEFORE;
        } else if ([cmd_arg isEqualToString:@"after"]) {
          kill_simulator = KILL_AFTER;
        } else if ([cmd_arg isEqualToString:@"both"]) {
          kill_simulator = KILL_BOTH;
        } else if ([cmd_arg isEqualToString:@"never"]) {
          kill_simulator = KILL_NEVER;
        } else {
          PrintUsage();
          exit(kExitInvalidArguments);
        }
      } break;
      case 'i':
        wants_simple_iossim = true;
        break;
      case 'h':
        PrintUsage();
        exit(kExitSuccess);
      default:
        PrintUsage();
        exit(kExitInvalidArguments);
    }
  }

  if (wants_simple_iossim && [tests_filter count]) {
    LogError(@"Cannot specify tests with -t when using -i.");
    exit(kExitInvalidArguments);
  }

  NSDictionary* simctl_list = GetSimulatorList(verbose_commands);

  if (wants_print_supported_devices) {
    PrintSupportedDevices(simctl_list);
    exit(kExitSuccess);
  }

  if (!sdk_version) {
    float sdk = 0;
    for (NSDictionary* runtime in Runtimes(simctl_list)) {
      sdk = fmax(sdk, [runtime[@"version"] floatValue]);
    }
    sdk_version = [NSString stringWithFormat:@"%0.1f", sdk];
  }

  NSRange range;
  for (NSString* cmd_arg in cmd_args) {
    range = [cmd_arg rangeOfString:@"--run-web-tests"];
    if (range.location != NSNotFound) {
      run_web_test = true;
      break;
    }
  }

  for (NSString* cmd_arg in cmd_args) {
    range = [cmd_arg rangeOfString:@"--prepare-web-tests"];
    if (range.location != NSNotFound) {
      prepare_web_test = true;
      break;
    }
  }

  if (udid == nil) {
    udid = GetDeviceBySDKAndName(simctl_list, device_name, sdk_version);
    if (udid == nil) {
      udid =
          CreateDeviceBySDKAndName(device_name, sdk_version, verbose_commands);
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
    PrintDeviceHome(udid, verbose_commands);
    exit(kExitSuccess);
  }

  if (kill_simulator & KILL_BEFORE) {
    KillSimulator(verbose_commands);
  }

  if (wants_wipe) {
    WipeDevice(udid, verbose_commands);
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
      if (wants_simple_iossim) {
        fprintf(stderr, "Warning: xctest_path ignored when using -i");
      } else {
        NSString* unresolved_xctest_path = [NSFileManager.defaultManager
            stringWithFileSystemRepresentation:argv[optind]
                                        length:strlen(argv[optind])];
        xctest_path = ResolvePath(unresolved_xctest_path);
        if (!xctest_path) {
          LogError(@"Unable to resolve xctest_path %@", unresolved_xctest_path);
          exit(kExitInvalidArguments);
        }
      }
    }
  } else {
    LogError(@"Unable to parse command line arguments.");
    PrintUsage();
    exit(kExitInvalidArguments);
  }

  if ((prepare_web_test || run_web_test || wants_simple_iossim) &&
      !isSimDeviceBooted(simctl_list, udid)) {
    RunSimCtl(@[ @"boot", udid ], verbose_commands);
  }

  int return_code = -1;
  if (prepare_web_test) {
    PrepareWebTests(udid, app_path, verbose_commands);
    return_code = kExitSuccess;
  } else if (run_web_test) {
    return_code = RunWebTest(app_path, udid, cmd_args, verbose_commands);
  } else if (wants_simple_iossim) {
    return_code =
        SimpleRunApplication(app_path, udid, cmd_args, verbose_commands);
  } else {
    return_code = RunApplication(app_path, xctest_path, udid, app_env, cmd_args,
                                 tests_filter, verbose_commands);
  }

  if (kill_simulator & KILL_AFTER) {
    KillSimulator(verbose_commands);
  }

  return return_code;
}
