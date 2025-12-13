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
      "Usage: iossim [options] <app_path> <xctest_path>\n"
      "  where <app_path> is the path to the .app directory and <xctest_path> "
      "is the path to an optional xctest bundle. The -i option is disallowed.\n"
      "Simplified usage: iossim -i [options] <app_path>\n"
      "  where <app_path> is the path to the .app directory. The -t option is "
      "disallowed.\n"
      "\n"
      "The -x option allows choosing between iOS and tvOS simulators.\n"
      "\n"
      "Options:\n"
      "  -h  Print this help message and exit.\n"
      "  -l  Print list of supported devices and runtimes and exit.\n"
      "  -p  Print the device's home directory and exit.\n"
      "  -w  Wipe the device's contents and settings and exit.\n"
      "\n"
      "  -v  Be more verbose, showing all the xcrun commands we call.\n"
      "  -i  Use iossim instead of xcodebuild (disables all xctest "
      "features). This is incompatible with -t.\n"
      "  -t  Specifies a test or test suite that should be included in the "
      "test run. All other tests will be excluded from this run. This is "
      "incompatible with -i.\n"
      "  -e  Specifies an environment key=value pair that will be"
      " set in the simulated application's environment.\n"
      "  -c  Specifies command line flags to pass to application.\n"
      "  -k  When to kill the iOS Simulator : before, after, both, never "
      "(default: both)\n"
      "\n"
      "  -u  Specifies the device udid to use. Will use -d, -s values to get "
      "devices if not specified.\n"
      "  -d  Specifies the device (must be one of the values from the iOS "
      "Simulator's Hardware -> Device menu. Defaults to 'iPhone 13'.\n"
      "  -s  Specifies the SDK version to use (e.g '9.3'). Will use system "
      "default if not specified.\n"
      "  -x  Specifies the desired platform for simulator selection: ios or "
      "tvos (default: ios)\n");
}

// Exit status codes.
const int kExitSuccess = EXIT_SUCCESS;
const int kExitInvalidArguments = 2;

// As of Xcode 16.2, passing --console causes `xcode simctl launch` to block
// indefinitely if there is stderr output longer than 8192 bytes (e.g. a long
// stack trace if a (D)CHECK is hit).
// Apple's documentation is very vague about the differences between --console
// and --console-pty, but the latter seems to work fine including in the case
// above.
constexpr NSString* kSimCtlLaunchConsoleArg = @"--console-pty";

void LogError(NSString* format, ...) {
  va_list list;
  va_start(list, format);

  NSString* message = [[NSString alloc] initWithFormat:format arguments:list];

  NSLog(@"ERROR: %@", message);

  va_end(list);
}

}  // namespace

typedef enum {
  PLATFORM_TYPE_IOS,
  PLATFORM_TYPE_TVOS,
} PlatformType;

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

// Return array of available iOS and tvOS runtime dictionaries.  Unavailable
// (old Xcode versions) or other runtimes (e.g. watchOS) are removed.
NSArray* Runtimes(NSDictionary* simctl_list, PlatformType platform_type) {
  NSString* runtime_prefix;
  switch (platform_type) {
    case PLATFORM_TYPE_IOS:
      runtime_prefix = @"com.apple.CoreSimulator.SimRuntime.iOS";
      break;
    case PLATFORM_TYPE_TVOS:
      runtime_prefix = @"com.apple.CoreSimulator.SimRuntime.tvOS";
      break;
  }

  NSMutableArray* runtimes = [simctl_list[@"runtimes"] mutableCopy];
  for (NSDictionary* runtime in simctl_list[@"runtimes"]) {
    BOOL available =
        [runtime[@"availability"] isEqualToString:@"(available)"] ||
        runtime[@"isAvailable"];

    if (![runtime[@"identifier"] hasPrefix:runtime_prefix] || !available) {
      [runtimes removeObject:runtime];
    }
  }
  return runtimes;
}

// Return array of device dictionaries.
NSArray* Devices(NSDictionary* simctl_list, PlatformType platform_type) {
  NSSet* product_families;
  switch (platform_type) {
    case PLATFORM_TYPE_IOS:
      product_families = [NSSet setWithArray:@[ @"iPad", @"iPhone" ]];
      break;
    case PLATFORM_TYPE_TVOS:
      product_families = [NSSet setWithObject:@"Apple TV"];
      break;
  }

  NSMutableArray* devicetypes = [simctl_list[@"devicetypes"] mutableCopy];
  for (NSDictionary* devicetype in simctl_list[@"devicetypes"]) {
    if (![product_families containsObject:devicetype[@"productFamily"]]) {
      [devicetypes removeObject:devicetype];
    }
  }
  return devicetypes;
}

// Get list of devices, runtimes, etc from simctl.
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
void PrintSupportedDevices(NSDictionary* simctl_list,
                           PlatformType platform_type) {
  printf("\ndevices:\n");
  for (NSDictionary* type in Devices(simctl_list, platform_type)) {
    printf("%s\n", [type[@"name"] UTF8String]);
  }
  printf("\nruntimes:\n");
  for (NSDictionary* runtime in Runtimes(simctl_list, platform_type)) {
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
                                NSString* sdk_version,
                                PlatformType platform_type) {
  BOOL found_runtime_with_version = NO;
  // Get runtime identifier based on version property to handle
  // cases when version and identifier are not the same,
  // e.g. below identifer is *13-2 but version is 13.2.2
  // {
  //   "version" : "13.2.2",
  //   "bundlePath" : "path"
  //   "identifier" : "com.apple.CoreSimulator.SimRuntime.iOS-13-2",
  //   "buildversion" : "17K90"
  // }
  for (NSDictionary* runtime in Runtimes(simctl_list, platform_type)) {
    if ([runtime[@"version"] isEqualToString:sdk_version]) {
      found_runtime_with_version = YES;
      NSString* sdk = runtime[@"identifier"];
      NSArray* devices = [simctl_list[@"devices"] objectForKey:sdk];
      for (NSDictionary* device in devices) {
        if ([device[@"name"] isEqualToString:device_name]) {
          return device[@"udid"];
        }
      }
    }
  }
  if (!found_runtime_with_version) {
    printf("\nDid not find runtime with specified version.\n");
    PrintSupportedDevices(simctl_list, platform_type);
    exit(kExitInvalidArguments);
  }
  // In this case we did find a runtime matching `sdk_version`, but no device
  // matching it.
  return nil;
}

// Create and return a device udid of |device| and |sdk_version|.
NSString* CreateDeviceBySDKAndName(NSString* device,
                                   NSString* sdk_version,
                                   PlatformType platform_type,
                                   bool verbose) {
  NSString* sdk;
  switch (platform_type) {
    case PLATFORM_TYPE_IOS:
      sdk = [@"iOS" stringByAppendingString:sdk_version];
      break;
    case PLATFORM_TYPE_TVOS:
      sdk = [@"tvOS" stringByAppendingString:sdk_version];
      break;
  }

  XCRunTask* create = [[XCRunTask alloc]
      initWithArguments:@[ @"simctl", @"create", device, device, sdk ]];
  [create run:verbose];

  NSDictionary* simctl_list = GetSimulatorList(verbose);
  return GetDeviceBySDKAndName(simctl_list, device, sdk_version, platform_type);
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
  [arguments addObject:kSimCtlLaunchConsoleArg];
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
    @"launch", kSimCtlLaunchConsoleArg, @"--terminate-running-process", udid,
    bundle_id
  ] arrayByAddingObjectsFromArray:cmd_args];
  return RunSimCtl(command, verbose);
}

int RunApplication(NSString* app_path,
                   NSString* xctest_path,
                   NSString* udid,
                   PlatformType platform_type,
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

  NSString* out_dir_name;
  NSString* platform_dir_name;
  NSString* simulator_platform_name;
  switch (platform_type) {
    case PLATFORM_TYPE_IOS:
      out_dir_name = @"Debug-iphonesimulator";
      platform_dir_name = @"iPhoneSimulator.platform";
      simulator_platform_name = @"iOS Simulator";
      break;
    case PLATFORM_TYPE_TVOS:
      out_dir_name = @"Debug-appletvsimulator";
      platform_dir_name = @"AppleTVSimulator.platform";
      simulator_platform_name = @"tvOS Simulator";
      break;
  }

  testingEnvironmentVariables[@"DYLD_FRAMEWORK_PATH"] = [NSString
      stringWithFormat:
          @"__TESTROOT__/%@:__PLATFORMS__/%@/Developer/Library/Frameworks",
          out_dir_name, platform_dir_name];
  testingEnvironmentVariables[@"DYLD_LIBRARY_PATH"] = [NSString
      stringWithFormat:@"__TESTROOT__/%@:__PLATFORMS__/%@/Developer/Library",
                       out_dir_name, platform_dir_name];

  if (xctest_path) {
    testTargetName[@"TestBundlePath"] = xctest_path;
    testingEnvironmentVariables[@"DYLD_INSERT_LIBRARIES"] =
        [NSString stringWithFormat:@"__PLATFORMS__/%@/Developer/"
                                   @"usr/lib/libXCTestBundleInject.dylib",
                                   platform_dir_name];
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
    [NSString
        stringWithFormat:@"platform=%@,id=%@", simulator_platform_name, udid],
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
  NSString* device_name = @"iPhone 13";
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
  PlatformType platform_type = PLATFORM_TYPE_IOS;

  int c;
  while ((c = getopt(argc, argv, "hs:d:u:t:e:c:pwlvk:ix:")) != -1) {
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
      case 'x': {
        NSString* cmd_arg = @(optarg);
        if ([cmd_arg isEqualToString:@"ios"]) {
          platform_type = PLATFORM_TYPE_IOS;
        } else if ([cmd_arg isEqualToString:@"tvos"]) {
          platform_type = PLATFORM_TYPE_TVOS;
        } else {
          LogError(@"Invalid value for -x.");
          PrintUsage();
          exit(kExitInvalidArguments);
        }
      } break;
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
    PrintSupportedDevices(simctl_list, platform_type);
    exit(kExitSuccess);
  }

  if (!sdk_version) {
    NSString* highest_sdk = @"0";

    for (NSDictionary* runtime in Runtimes(simctl_list, platform_type)) {
      NSString* runtime_version = runtime[@"version"];
      if (!highest_sdk ||
          [runtime_version compare:highest_sdk
                           options:NSNumericSearch] == NSOrderedDescending) {
        highest_sdk = runtime_version;
      }
    }

    sdk_version = highest_sdk;
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
    udid = GetDeviceBySDKAndName(simctl_list, device_name, sdk_version,
                                 platform_type);
    if (udid == nil) {
      udid = CreateDeviceBySDKAndName(device_name, sdk_version, platform_type,
                                      verbose_commands);
      if (udid == nil) {
        LogError(@"Unable to find a device %@ with SDK %@.", device_name,
                 sdk_version);
        PrintSupportedDevices(simctl_list, platform_type);
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
    return_code =
        RunApplication(app_path, xctest_path, udid, platform_type, app_env,
                       cmd_args, tests_filter, verbose_commands);
  }

  if (kill_simulator & KILL_AFTER) {
    KillSimulator(verbose_commands);
  }

  return return_code;
}
