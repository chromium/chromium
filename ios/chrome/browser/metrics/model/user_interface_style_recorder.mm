// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/user_interface_style_recorder.h"

#import "base/metrics/histogram_functions.h"

namespace {

// Interface Style enum to report UMA metrics. Must be in sync with
// iOSInterfaceStyleForReporting in tools/metrics/histograms/enums.xml.
enum class InterfaceStyleForReporting {
  kUnspecified,
  kLight,
  kDark,
  kMaxValue = kDark
};

}  // namespace
@interface UserInterfaceStyleRecorder ()
@property(nonatomic, assign) BOOL applicationInBackground;
@property(nonatomic, assign) UIUserInterfaceStyle initialUserInterfaceStyle;
@end

@implementation UserInterfaceStyleRecorder

- (instancetype)initWithUserInterfaceStyle:
    (UIUserInterfaceStyle)userInterfaceStyle {
  self = [super init];
  if (self) {
    // Store the initial user interface for reporting after the application did
    // become active. Otherwise, if this initializer is called before the
    // metrics service is started, reporting metrics will fail for the entire
    // session.
    _initialUserInterfaceStyle = userInterfaceStyle;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationDidBecomeActive:)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationDidEnterBackground:)
               name:UIApplicationDidEnterBackgroundNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationWillEnterForeground:)
               name:UIApplicationWillEnterForegroundNotification
             object:nil];
  }
  return self;
}

#pragma mark - Application state notifications handlers

- (void)applicationDidBecomeActive:(NSNotification*)notification {
  // This is only needed to report the initial user interface. Deregister from
  // this notification on the first time it is received.
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidBecomeActiveNotification
              object:nil];
  // For good measure, set the initial interface to unspecified.
  self.initialUserInterfaceStyle = UIUserInterfaceStyleUnspecified;
}

- (void)applicationDidEnterBackground:(NSNotification*)notification {
  self.applicationInBackground = YES;
}

- (void)applicationWillEnterForeground:(NSNotification*)notification {
  self.applicationInBackground = NO;
}

@end
