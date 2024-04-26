// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/app_launch_manager.h"

#import <XCTest/XCTest.h>

#import "base/command_line.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/testing/earl_grey/app_launch_argument_generator.h"
#import "ios/testing/earl_grey/app_launch_manager_app_interface.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case_app_interface.h"
#import "ios/testing/earl_grey/coverage_utils.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/third_party/edo/src/Service/Sources/EDOServiceException.h"

namespace {
// Returns the list of extra app launch args from test command line args.
NSArray<NSString*>* ExtraAppArgsFromTestSwitch() {
  if (!base::CommandLine::InitializedForCurrentProcess()) {
    return [NSArray array];
  }
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Multiple extra app launch arguments can be passed through this switch. The
  // args should be in raw format, separated by commas if more than one.
  const char kExtraAppArgsSwitch[] = "extra-app-args";
  if (!command_line->HasSwitch(kExtraAppArgsSwitch)) {
    return [NSArray array];
  }

  return [base::SysUTF8ToNSString(command_line->GetSwitchValueASCII(
      kExtraAppArgsSwitch)) componentsSeparatedByString:@","];
}

// Checks if two pairs of launch arguments are equivalent.
bool LaunchArgumentsAreEqual(NSArray<NSString*>* args1,
                             NSArray<NSString*>* args2) {
  // isEqualToArray will only return true if both arrays are non-nil,
  // so first check if both arrays are empty or nil
  if (!args1.count && !args2.count) {
    return true;
  }

  return [args1 isEqualToArray:args2];
}
}  // namespace

@interface AppLaunchManager ()
// Similar to EG's -backgroundApplication, but with a longer 20 second wait and
// faster 0.5 second poll interval.
- (BOOL)backgroundApplication;
// List of observers to be notified of actions performed by the app launch
// manager.
@property(nonatomic, strong)
    CRBProtocolObservers<AppLaunchManagerObserver>* observers;
@property(nonatomic) XCUIApplication* runningApplication;
@property(nonatomic) int runningApplicationProcessIdentifier;
@property(nonatomic) NSArray<NSString*>* currentLaunchArgs;
@end

@implementation AppLaunchManager

+ (AppLaunchManager*)sharedManager {
  static AppLaunchManager* instance = nil;
  static dispatch_once_t guard;
  dispatch_once(&guard, ^{
    instance = [[AppLaunchManager alloc] initPrivate];
  });
  return instance;
}

- (instancetype)initPrivate {
  self = [super init];

  Protocol* protocol = @protocol(AppLaunchManagerObserver);
  _observers = (id)[CRBProtocolObservers observersWithProtocol:protocol];
  return self;
}

- (BOOL)appIsLaunched {
  return (self.runningApplication != nil) &&
         (self.runningApplication.state != XCUIApplicationStateNotRunning) &&
         (self.runningApplication.state != XCUIApplicationStateUnknown);
}

- (BOOL)appIsRunning {
  return
      [self appIsLaunched] && (self.runningApplication.state !=
                               XCUIApplicationStateRunningBackgroundSuspended);
}

// Makes sure the app has been started with the appropriate |arguments|.
// In EG2, will launch the app if any of the following conditions are met:
// * The app is not running
// * The app is currently running with different arguments.
// * |forceRestart| is YES
// Otherwise, the app will be activated instead of (re)launched.
// Will wait until app is activated or launched, and fail the test if it
// fails to do so.
// In EG1, this method is a no-op.
- (void)ensureAppLaunchedWithArgs:(NSArray<NSString*>*)arguments
                   relaunchPolicy:(RelaunchPolicy)relaunchPolicy {
  BOOL forceRestart = (relaunchPolicy == ForceRelaunchByKilling) ||
                      (relaunchPolicy == ForceRelaunchByCleanShutdown);
  BOOL gracefullyKill = (relaunchPolicy == ForceRelaunchByCleanShutdown);
  BOOL runResets = (relaunchPolicy == NoForceRelaunchAndResetState);

  // If app has crashed it should be relaunched with the proper resets.
  BOOL appIsRunning = [self appIsRunning];

  // App PID change means an unknown relaunch not from AppLaunchManager, so it
  // needs a correct relaunch for setups.
  BOOL appPIDChanged = YES;
  if (appIsRunning) {
    @try {
      appPIDChanged = (self.runningApplicationProcessIdentifier !=
                       [AppLaunchManagerAppInterface processIdentifier]);
    } @catch (NSException* exception) {
      GREYAssertEqual(
          EDOServiceGenericException, exception.name,
          @"Unknown excption caught when communicating to host app: %@",
          exception.reason);
      // An EDOServiceGenericException here comes from the communication between
      // test and app process, which means there should be issues in host app,
      // but it wasn't reflected in XCUIApplicationState.
      // TODO(crbug.com/40687845): Investigate why the exception is thrown.
      appIsRunning = NO;
    }
  }

  // Extend extra app launch args from test switch to arguments.
  arguments =
      [arguments arrayByAddingObjectsFromArray:ExtraAppArgsFromTestSwitch()];

  bool appNeedsLaunching =
      forceRestart || !appIsRunning || appPIDChanged ||
      !LaunchArgumentsAreEqual(arguments, self.currentLaunchArgs);
  if (!appNeedsLaunching) {
    XCTAssertTrue(self.runningApplication.state ==
                  XCUIApplicationStateRunningForeground);
    return;
  }

  if (appIsRunning) {
    if (gracefullyKill) {
      GREYAssertTrue([self backgroundApplication],
                     @"Failed to background application.");

      if (self.runningApplication.state ==
          XCUIApplicationStateRunningBackgroundSuspended) {
        [self.runningApplication terminate];
      } else {
        [BaseEarlGreyTestCaseAppInterface gracefulTerminate];
        if (![self.runningApplication
                waitForState:XCUIApplicationStateNotRunning
                     timeout:5]) {
          [self.runningApplication terminate];
        }
      }
    }

    // No-op if already terminated above.
    [self.runningApplication terminate];

    // Can't use EG conditionals here since the app is terminated.
    XCTAssertTrue([self.runningApplication
        waitForState:XCUIApplicationStateNotRunning
             timeout:15]);
    XCTAssertTrue(self.runningApplication.state ==
                  XCUIApplicationStateNotRunning);
  }

  XCUIApplication* application = [[XCUIApplication alloc] init];
  application.launchArguments = arguments;

  // Instruct EG to not DYLD_INSERT_LIBRARIES, which can interfere with
  // Chromium's framework setup.
  NSMutableDictionary<NSString*, NSString*>* mutableEnv =
      [application.launchEnvironment mutableCopy];
  mutableEnv[@"EG_SKIP_INSERT_LIBRARIES"] = @"YES";
  application.launchEnvironment = [mutableEnv copy];

  @try {
    [application launch];
  } @catch (id exception) {
    XCTAssertFalse(GREYTestApplicationDistantObject.sharedInstance
                       .hostActiveWithAppComponent);
  }

  if (!GREYTestApplicationDistantObject.sharedInstance
           .hostActiveWithAppComponent) {
    NSLog(@"App has crashed on startup");
    self.runningApplication = nil;
    self.runningApplicationProcessIdentifier = -1;
    self.currentLaunchArgs = nil;
    XCTAssertFalse([self appIsLaunched]);
    return;
  }

  [CoverageUtils configureCoverageReportPath];

  if (self.runningApplication) {
    [self.observers appLaunchManagerDidRelaunchApp:self runResets:runResets];
  }

  self.runningApplication = application;
  self.runningApplicationProcessIdentifier =
      [AppLaunchManagerAppInterface processIdentifier];
  self.currentLaunchArgs = arguments;
}

- (void)ensureAppLaunchedWithConfiguration:
    (AppLaunchConfiguration)configuration {
  NSArray<NSString*>* arguments = ArgumentsFromConfiguration(configuration);

  [self ensureAppLaunchedWithArgs:arguments
                   relaunchPolicy:configuration.relaunch_policy];

  if ([self appIsLaunched]) {
    [BaseEarlGreyTestCaseAppInterface enableFastAnimation];

#if !TARGET_IPHONE_SIMULATOR
    if (@available(iOS 17, *)) {
      [BaseEarlGreyTestCaseAppInterface swizzleKeyboardOOP];
    }
#endif

    // Wait for application to settle before continuing on with test.
    GREYWaitForAppToIdle(@"App failed to idle BEFORE test body started.\n\n"
                         @"**** Check that the prior test left the app in a"
                         @"clean state. ****");
  }
}

- (void)ensureAppLaunchedWithFeaturesEnabled:
            (std::vector<base::test::FeatureRef>)featuresEnabled
                                    disabled:
                                        (std::vector<base::test::FeatureRef>)
                                            featuresDisabled
                              relaunchPolicy:(RelaunchPolicy)relaunchPolicy {
  AppLaunchConfiguration config;
  config.features_enabled = std::move(featuresEnabled);
  config.features_disabled = std::move(featuresDisabled);
  config.relaunch_policy = relaunchPolicy;
  [self ensureAppLaunchedWithConfiguration:config];
}

- (void)backgroundAndForegroundApp {
  GREYAssertTrue([self backgroundApplication],
                 @"Failed to background application.");
  [self.runningApplication activate];
}

- (BOOL)backgroundApplication {
  XCUIApplication* currentApplication = [[XCUIApplication alloc] init];
  // Tell the system to background the app.
  [[XCUIDevice sharedDevice] pressButton:XCUIDeviceButtonHome];
  BOOL (^conditionBlock)(void) = ^BOOL {
    return currentApplication.state == XCUIApplicationStateRunningBackground ||
           currentApplication.state ==
               XCUIApplicationStateRunningBackgroundSuspended;
  };
  GREYCondition* condition =
      [GREYCondition conditionWithName:@"check if backgrounded"
                                 block:conditionBlock];
  return [condition waitWithTimeout:20.0 pollInterval:0.5];
}

- (void)addObserver:(id<AppLaunchManagerObserver>)observer {
  [self.observers addObserver:observer];
}

- (void)removeObserver:(id<AppLaunchManagerObserver>)observer {
  [self.observers removeObserver:observer];
}

@end
