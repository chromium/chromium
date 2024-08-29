// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"

#import "base/check.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/crash/core/common/crash_key.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/crash_report/model/crash_report_user_application_state.h"

namespace crash_keys {

namespace {

const char kCrashedInBackground[] = "crashed_in_background";
const char kFreeDiskInKB[] = "free_disk_in_kb";
const char kFreeMemoryInKB[] = "free_memory_in_kb";
const char kMemoryWarningInProgress[] = "memory_warning_in_progress";
const char kMemoryWarningCount[] = "memory_warning_count";
const char kGridToVisibleTabAnimation[] = "grid_to_visible_tab_animation";
static crash_reporter::CrashKeyString<1028> kRemoveGridToVisibleTabAnimationKey(
    kGridToVisibleTabAnimation);
const char kCrashedAfterAppWillTerminate[] = "crashed_after_app_will_terminate";

// Multiple state information are combined into one CrashReportMultiParameter
// to save limited and finite number of ReportParameters.
// These are the values grouped in the user_application_state parameter.
char const kOrientationState[] = "orient";
char const kHorizontalSizeClass[] = "sizeclass";
char const kUserInterfaceStyle[] = "user_interface_style";
char const kSignedIn[] = "signIn";
char const kIsShowingPDF[] = "pdf";
char const kVideoPlaying[] = "avplay";
char const kIncognitoTabCount[] = "OTRTabs";
char const kRegularTabCount[] = "regTabs";
char const kInactiveTabCount[] = "inactiveTabs";
char const kConnectedScenes[] = "scenes";
char const kForegroundScenes[] = "fgScenes";
char const kDestroyingAndRebuildingIncognitoBrowserState[] =
    "destroyingAndRebuildingOTR";
char const kVoiceOverRunning[] = "voiceOver";

}  // namespace

void SetCurrentlyInBackground(bool background) {
  static crash_reporter::CrashKeyString<4> key(kCrashedInBackground);
  if (background) {
    key.Set("yes");
    [[PreviousSessionInfo sharedInstance]
        setReportParameterValue:@"yes"
                         forKey:base::SysUTF8ToNSString(kCrashedInBackground)];
  } else {
    key.Clear();
    [[PreviousSessionInfo sharedInstance]
        removeReportParameterForKey:base::SysUTF8ToNSString(
                                        kCrashedInBackground)];
  }
}

void SetMemoryWarningCount(int count) {
  static crash_reporter::CrashKeyString<16> key(kMemoryWarningCount);
  if (count) {
    key.Set(base::NumberToString(count));
    [[PreviousSessionInfo sharedInstance]
        setReportParameterValue:base::SysUTF8ToNSString(
                                    base::NumberToString(count))
                         forKey:base::SysUTF8ToNSString(kMemoryWarningCount)];
  } else {
    key.Clear();
    [[PreviousSessionInfo sharedInstance]
        removeReportParameterForKey:base::SysUTF8ToNSString(
                                        kMemoryWarningCount)];
  }
}

void SetMemoryWarningInProgress(bool value) {
  static crash_reporter::CrashKeyString<4> key(kMemoryWarningInProgress);
  if (value) {
    key.Set("yes");
    [[PreviousSessionInfo sharedInstance]
        setReportParameterValue:@"yes"
                         forKey:base::SysUTF8ToNSString(
                                    kMemoryWarningInProgress)];

  } else {
    key.Clear();
    [[PreviousSessionInfo sharedInstance]
        removeReportParameterForKey:base::SysUTF8ToNSString(
                                        kMemoryWarningInProgress)];
  }
}

void SetCrashedAfterAppWillTerminate() {
  static crash_reporter::CrashKeyString<4> key(kCrashedAfterAppWillTerminate);
  key.Set("yes");
  [[PreviousSessionInfo sharedInstance]
      setReportParameterValue:@"yes"
                       forKey:base::SysUTF8ToNSString(
                                  kCrashedAfterAppWillTerminate)];
}

void SetCurrentFreeMemoryInKB(int value) {
  static crash_reporter::CrashKeyString<16> key(kFreeMemoryInKB);
  key.Set(base::NumberToString(value));
  [[PreviousSessionInfo sharedInstance]
      setReportParameterValue:base::SysUTF8ToNSString(
                                  base::NumberToString(value))
                       forKey:base::SysUTF8ToNSString(kFreeMemoryInKB)];
}

void SetCurrentFreeDiskInKB(int value) {
  static crash_reporter::CrashKeyString<16> key(kFreeDiskInKB);
  key.Set(base::NumberToString(value));
  [[PreviousSessionInfo sharedInstance]
      setReportParameterValue:base::SysUTF8ToNSString(
                                  base::NumberToString(value))
                       forKey:base::SysUTF8ToNSString(kFreeDiskInKB)];
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

void SetConnectedScenesCount(int connectedScenes) {
  if (connectedScenes > 1) {
    [[CrashReportUserApplicationState sharedInstance] setValue:kConnectedScenes
                                                     withValue:connectedScenes];
  } else {
    [[CrashReportUserApplicationState sharedInstance]
        removeValue:kConnectedScenes];
  }
}

void SetForegroundScenesCount(int foregroundScenes) {
  if (foregroundScenes > 1) {
    [[CrashReportUserApplicationState sharedInstance]
         setValue:kForegroundScenes
        withValue:foregroundScenes];
  } else {
    [[CrashReportUserApplicationState sharedInstance]
        removeValue:kForegroundScenes];
  }
}

void SetRegularTabCount(int tabCount) {
  [[CrashReportUserApplicationState sharedInstance] setValue:kRegularTabCount
                                                   withValue:tabCount];
  [[PreviousSessionInfo sharedInstance] updateCurrentSessionTabCount:tabCount];
}

void SetInactiveTabCount(int tabCount) {
  [[CrashReportUserApplicationState sharedInstance] setValue:kInactiveTabCount
                                                   withValue:tabCount];
  [[PreviousSessionInfo sharedInstance]
      updateCurrentSessionInactiveTabCount:tabCount];
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
  kRemoveGridToVisibleTabAnimationKey.Set(
      base::SysNSStringToUTF8(formatted_value));
}

void RemoveGridToVisibleTabAnimation() {
  kRemoveGridToVisibleTabAnimationKey.Clear();
}

void MediaStreamPlaybackDidStart() {
  [[CrashReportUserApplicationState sharedInstance]
      incrementValue:kVideoPlaying];
}

void MediaStreamPlaybackDidStop() {
  [[CrashReportUserApplicationState sharedInstance]
      decrementValue:kVideoPlaying];
}

void SetVoiceOverRunning(bool running) {
  if (running) {
    [[CrashReportUserApplicationState sharedInstance] setValue:kVoiceOverRunning
                                                     withValue:1];
  } else {
    [[CrashReportUserApplicationState sharedInstance]
        removeValue:kVoiceOverRunning];
  }
}

}  // namespace crash_keys
