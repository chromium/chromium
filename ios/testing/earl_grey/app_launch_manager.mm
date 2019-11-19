// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/app_launch_manager.h"

#import <XCTest/XCTest.h>

#include "base/feature_list.h"
#import "base/ios/crb_protocol_observers.h"
#include "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)  // avoid unused function warning in EG1
namespace {
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
#endif

@interface AppLaunchManager ()
// List of observers to be notified of actions performed by the app launch
// manager.
@property(nonatomic, strong)
    CRBProtocolObservers<AppLaunchManagerObserver>* observers;
@property(nonatomic) XCUIApplication* runningApplication;
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
                     forceRestart:(BOOL)forceRestart {
#if defined(CHROME_EARL_GREY_2)
  bool appNeedsLaunching =
      forceRestart || !self.runningApplication ||
      !LaunchArgumentsAreEqual(arguments, self.currentLaunchArgs);

  if (!appNeedsLaunching) {
    [self.runningApplication activate];
    return;
  }

  XCUIApplication* application = [[XCUIApplication alloc] init];
  application.launchArguments = arguments;

  [application launch];
  if (self.runningApplication) {
    [self.observers appLaunchManagerDidRelaunchApp:self];
  }
  self.runningApplication = application;
  self.currentLaunchArgs = arguments;
#endif
}

- (void)ensureAppLaunchedWithFeaturesEnabled:
            (const std::vector<base::Feature>&)featuresEnabled
                                    disabled:(const std::vector<base::Feature>&)
                                                 featuresDisabled
                                forceRestart:(BOOL)forceRestart {
  NSMutableArray<NSString*>* namesToEnable =
      [NSMutableArray arrayWithCapacity:featuresEnabled.size()];
  NSMutableArray<NSString*>* namesToDisable =
      [NSMutableArray arrayWithCapacity:featuresDisabled.size()];

  for (const base::Feature& feature : featuresEnabled) {
    [namesToEnable addObject:base::SysUTF8ToNSString(feature.name)];
  }

  for (const base::Feature& feature : featuresDisabled) {
    [namesToDisable addObject:base::SysUTF8ToNSString(feature.name)];
  }

  NSString* enabledString = @"";
  NSString* disabledString = @"";
  if ([namesToEnable count] > 0) {
    enabledString = [NSString
        stringWithFormat:@"--enable-features=%@",
                         [namesToEnable componentsJoinedByString:@","]];
  }
  if ([namesToDisable count] > 0) {
    disabledString = [NSString
        stringWithFormat:@"--disable-features=%@",
                         [namesToDisable componentsJoinedByString:@","]];
  }

  NSArray<NSString*>* arguments = @[ enabledString, disabledString ];
  [self ensureAppLaunchedWithArgs:arguments forceRestart:forceRestart];
}

- (void)addObserver:(id<AppLaunchManagerObserver>)observer {
  [self.observers addObserver:observer];
}

- (void)removeObserver:(id<AppLaunchManagerObserver>)observer {
  [self.observers removeObserver:observer];
}

@end
