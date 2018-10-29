// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/size_class_recorder.h"
#import "ios/chrome/browser/metrics/size_class_recorder_private.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Converts a UIKit size class to a size class for reporting.
SizeClassForReporting SizeClassForReportingForUIUserInterfaceSizeClass(
    UIUserInterfaceSizeClass sizeClass) {
  switch (sizeClass) {
    case UIUserInterfaceSizeClassUnspecified:
      return SizeClassForReporting::UNSPECIFIED;
    case UIUserInterfaceSizeClassCompact:
      return SizeClassForReporting::COMPACT;
    case UIUserInterfaceSizeClassRegular:
      return SizeClassForReporting::REGULAR;
  }
}

namespace {

// Reports the currently used horizontal size class.
void ReportHorizontalSizeClassUsed(UIUserInterfaceSizeClass sizeClass) {
  SizeClassForReporting sizeClassForReporting =
      SizeClassForReportingForUIUserInterfaceSizeClass(sizeClass);
  UMA_HISTOGRAM_ENUMERATION("Tab.HorizontalSizeClassUsed",
                            sizeClassForReporting,
                            SizeClassForReporting::COUNT);
}

}  // namespace

@interface SizeClassRecorder ()
@property(nonatomic, assign) BOOL applicationInBackground;
@property(nonatomic, assign) UIUserInterfaceSizeClass initialSizeClass;
@end

@implementation SizeClassRecorder

@synthesize applicationInBackground = _applicationInBackground;
@synthesize initialSizeClass = _initialSizeClass;

- (instancetype)initWithHorizontalSizeClass:
    (UIUserInterfaceSizeClass)sizeClass {
  // Size classes change tracking is only available on iPad devices.
  DCHECK(IsIPadIdiom());
  self = [super init];
  if (self) {
    // Store the initial size class for reporting after the application did
    // become active. Otherwise, if this initializer is called before the
    // metrics service is started, reporting metrics will fail for the entire
    // session.
    _initialSizeClass = sizeClass;
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

- (void)horizontalSizeClassDidChange:(UIUserInterfaceSizeClass)newSizeClass {
  // iOS sometimes changes from Compact to Regular to Compact again when putting
  // the application in the background, to update the application screenshot.
  // Ignore those changes.
  if (self.applicationInBackground)
    return;
  ReportHorizontalSizeClassUsed(newSizeClass);
}

+ (void)pageLoadedWithHorizontalSizeClass:(UIUserInterfaceSizeClass)sizeClass {
  SizeClassForReporting sizeClassForReporting =
      SizeClassForReporting(sizeClass);
  UMA_HISTOGRAM_ENUMERATION("Tab.PageLoadInHorizontalSizeClass",
                            sizeClassForReporting,
                            SizeClassForReporting::COUNT);
}

#pragma mark - Application state notifications handlers

- (void)applicationDidBecomeActive:(NSNotification*)notification {
  // This is only needed to report the initial size class. Deregister from this
  // notification on the first time it is received.
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidBecomeActiveNotification
              object:nil];
  ReportHorizontalSizeClassUsed(self.initialSizeClass);
  // For good measure, set the initial size class to unspecified.
  self.initialSizeClass = UIUserInterfaceSizeClassUnspecified;
}

- (void)applicationDidEnterBackground:(NSNotification*)notification {
  self.applicationInBackground = YES;
}

- (void)applicationWillEnterForeground:(NSNotification*)notification {
  self.applicationInBackground = NO;
}

@end
