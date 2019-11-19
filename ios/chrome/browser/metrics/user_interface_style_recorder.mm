// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/user_interface_style_recorder.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Interface Style enum to report UMA metrics. Must be in sync with
// iOSInterfaceStyleForReporting in tools/metrics/histograms/enums.xml.
enum class InterfaceStyleForReporting {
  kUnspecified,
  kLight,
  kDark,
  kMaxValue = kDark
};

// Converts a UIKit interface style to a interface style for reporting.
InterfaceStyleForReporting InterfaceStyleForReportingForUIUserInterfaceStyle(
    UIUserInterfaceStyle userInterfaceStyle) {
  switch (userInterfaceStyle) {
    case UIUserInterfaceStyleUnspecified:
      return InterfaceStyleForReporting::kUnspecified;
    case UIUserInterfaceStyleLight:
      return InterfaceStyleForReporting::kLight;
    case UIUserInterfaceStyleDark:
      return InterfaceStyleForReporting::kDark;
  }
}

// Reports the currently used interface style.
void ReportUserInterfaceStyleUsed(UIUserInterfaceStyle userInterfaceStyle) {
  InterfaceStyleForReporting userInterfaceStyleForReporting =
      InterfaceStyleForReportingForUIUserInterfaceStyle(userInterfaceStyle);
  base::UmaHistogramEnumeration("UserInterfaceStyle.CurrentlyUsed",
                                userInterfaceStyleForReporting);
}

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

- (void)userInterfaceStyleDidChange:
    (UIUserInterfaceStyle)newUserInterfaceStyle {
  // When an app goes to the background iOS toggles the user interface 2 times.
  // This is probably to take screenshots of the screen for multitask. After
  // this if the interface style changes, the app is not notified until it comes
  // to the foreground. We only care if changed was registered while in
  // foreground.
  if (!self.applicationInBackground) {
    ReportUserInterfaceStyleUsed(newUserInterfaceStyle);
  }
}

#pragma mark - Application state notifications handlers

- (void)applicationDidBecomeActive:(NSNotification*)notification {
  // This is only needed to report the initial user interface. Deregister from
  // this notification on the first time it is received.
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidBecomeActiveNotification
              object:nil];
  ReportUserInterfaceStyleUsed(self.initialUserInterfaceStyle);
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
