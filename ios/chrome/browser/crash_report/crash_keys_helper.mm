// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/crash_keys_helper.h"

#include "base/check.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/crash_report/breakpad_helper.h"
#import "ios/chrome/browser/crash_report/crash_report_user_application_state.h"
#import "ios/chrome/browser/crash_report/main_thread_freeze_detector.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using breakpad_helper::AddReportParameter;
using breakpad_helper::RemoveReportParameter;

namespace crash_keys {

NSString* const kBreadcrumbsProductDataKey = @"breadcrumbs";

namespace {

NSString* const kCrashedInBackground = @"crashed_in_background";
NSString* const kFreeDiskInKB = @"free_disk_in_kb";
NSString* const kFreeMemoryInKB = @"free_memory_in_kb";
NSString* const kMemoryWarningInProgress = @"memory_warning_in_progress";
NSString* const kMemoryWarningCount = @"memory_warning_count";
NSString* const kGridToVisibleTabAnimation = @"grid_to_visible_tab_animation";

// Multiple state information are combined into one CrashReportMultiParameter
// to save limited and finite number of ReportParameters.
// These are the values grouped in the user_application_state parameter.
NSString* const kOrientationState = @"orient";
NSString* const kHorizontalSizeClass = @"sizeclass";
NSString* const kUserInterfaceStyle = @"user_interface_style";
NSString* const kSignedIn = @"signIn";
NSString* const kIsShowingPDF = @"pdf";
NSString* const kVideoPlaying = @"avplay";
NSString* const kIncognitoTabCount = @"OTRTabs";
NSString* const kRegularTabCount = @"regTabs";
NSString* const kDestroyingAndRebuildingIncognitoBrowserState =
    @"destroyingAndRebuildingOTR";

}  // namespace

void SetCurrentlyInBackground(bool background) {
  if (background) {
    AddReportParameter(kCrashedInBackground, @"yes", true);
    [[MainThreadFreezeDetector sharedInstance] stop];
  } else {
    RemoveReportParameter(kCrashedInBackground);
    [[MainThreadFreezeDetector sharedInstance] start];
  }
}

void SetMemoryWarningCount(int count) {
  if (count) {
    AddReportParameter(kMemoryWarningCount,
                       [NSString stringWithFormat:@"%d", count], true);
  } else {
    RemoveReportParameter(kMemoryWarningCount);
  }
}

void SetMemoryWarningInProgress(bool value) {
  if (value)
    AddReportParameter(kMemoryWarningInProgress, @"yes", true);
  else
    RemoveReportParameter(kMemoryWarningInProgress);
}

void SetCurrentFreeMemoryInKB(int value) {
  AddReportParameter(kFreeMemoryInKB, [NSString stringWithFormat:@"%d", value],
                     true);
}

void SetCurrentFreeDiskInKB(int value) {
  AddReportParameter(kFreeDiskInKB, [NSString stringWithFormat:@"%d", value],
                     true);
}

void SetCurrentTabIsPDF(bool value) {
  if (value) {
    [[CrashReportUserApplicationState sharedInstance]
        incrementValue:kIsShowingPDF];
  } else {
    [[CrashReportUserApplicationState sharedInstance]
        decrementValue:kIsShowingPDF];
  }
}

void SetCurrentOrientation(int statusBarOrientation, int deviceOrientation) {
  DCHECK((statusBarOrientation < 10) && (deviceOrientation < 10));
  int deviceAndUIOrientation = 10 * statusBarOrientation + deviceOrientation;
  [[CrashReportUserApplicationState sharedInstance]
       setValue:kOrientationState
      withValue:deviceAndUIOrientation];
}

void SetCurrentHorizontalSizeClass(int horizontalSizeClass) {
  [[CrashReportUserApplicationState sharedInstance]
       setValue:kHorizontalSizeClass
      withValue:horizontalSizeClass];
}

void SetCurrentUserInterfaceStyle(int userInterfaceStyle) {
  [[CrashReportUserApplicationState sharedInstance]
       setValue:kUserInterfaceStyle
      withValue:userInterfaceStyle];
}

void SetCurrentlySignedIn(bool signedIn) {
  if (signedIn) {
    [[CrashReportUserApplicationState sharedInstance] setValue:kSignedIn
                                                     withValue:1];
  } else {
    [[CrashReportUserApplicationState sharedInstance] removeValue:kSignedIn];
  }
}

void SetRegularTabCount(int tabCount) {
  [[CrashReportUserApplicationState sharedInstance] setValue:kRegularTabCount
                                                   withValue:tabCount];
  [[PreviousSessionInfo sharedInstance] updateCurrentSessionTabCount:tabCount];
}

void SetIncognitoTabCount(int tabCount) {
  [[CrashReportUserApplicationState sharedInstance] setValue:kIncognitoTabCount
                                                   withValue:tabCount];
  [[PreviousSessionInfo sharedInstance]
      updateCurrentSessionOTRTabCount:tabCount];
}

void SetDestroyingAndRebuildingIncognitoBrowserState(bool in_progress) {
  if (in_progress) {
    [[CrashReportUserApplicationState sharedInstance]
         setValue:kDestroyingAndRebuildingIncognitoBrowserState
        withValue:1];
  } else {
    [[CrashReportUserApplicationState sharedInstance]
        removeValue:kDestroyingAndRebuildingIncognitoBrowserState];
  }
}

void SetGridToVisibleTabAnimation(NSString* to_view_controller,
                                  NSString* presenting_view_controller,
                                  NSString* presented_view_controller,
                                  NSString* parent_view_controller) {
  NSString* formatted_value =
      [NSString stringWithFormat:
                    @"{toVC:%@, presentingVC:%@, presentedVC:%@, parentVC:%@}",
                    to_view_controller, presenting_view_controller,
                    presented_view_controller, parent_view_controller];
  AddReportParameter(kGridToVisibleTabAnimation, formatted_value, true);
}

void RemoveGridToVisibleTabAnimation() {
  RemoveReportParameter(kGridToVisibleTabAnimation);
}

void SetBreadcrumbEvents(NSString* breadcrumbs) {
  AddReportParameter(kBreadcrumbsProductDataKey, breadcrumbs, /*async=*/true);
}

void MediaStreamPlaybackDidStart() {
  [[CrashReportUserApplicationState sharedInstance]
      incrementValue:kVideoPlaying];
}

void MediaStreamPlaybackDidStop() {
  [[CrashReportUserApplicationState sharedInstance]
      decrementValue:kVideoPlaying];
}

}  // namespace breakpad_helper
