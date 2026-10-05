// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"

#import <limits>

#import "base/check.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/crash/core/common/crash_key.h"
#import "components/previous_session_info/previous_session_info.h"

namespace crash_keys {

namespace {

// Helper to record a boolean crash key.
class BooleanCrashKey {
 public:
  constexpr explicit BooleanCrashKey(const char name[])
      : name_(name), key_(name) {}

  void Update(bool value) {
    if (value) {
      key_.Set("yes");
      [[PreviousSessionInfo sharedInstance]
          setReportParameterValue:@"yes"
                           forKey:base::SysUTF8ToNSString(name_)];
    } else {
      key_.Clear();
      [[PreviousSessionInfo sharedInstance]
          removeReportParameterForKey:base::SysUTF8ToNSString(name_)];
    }
  }

 private:
  const char* const name_;
  crash_reporter::CrashKeyString<4> key_;
};

// Helper to record an integer crash key.
template <const bool clear_on_zero = false>
class IntegerCrashKey {
 public:
  constexpr explicit IntegerCrashKey(const char name[])
      : name_(name), key_(name) {}

  void Update(int value) {
    if constexpr (clear_on_zero) {
      if (value == 0) {
        key_.Clear();
        [[PreviousSessionInfo sharedInstance]
            removeReportParameterForKey:base::SysUTF8ToNSString(name_)];
        return;
      }
    }

    const std::string value_as_string = base::NumberToString(value);
    key_.Set(value_as_string);
    [[PreviousSessionInfo sharedInstance]
        setReportParameterValue:base::SysUTF8ToNSString(value_as_string)
                         forKey:base::SysUTF8ToNSString(name_)];
  }

 private:
  const char* const name_;
  crash_reporter::CrashKeyString<16> key_;
};

// Helper to record an integer crash key computed from local true/false events.
template <const bool clear_on_zero = false>
class CounterCrashKey {
 public:
  constexpr explicit CounterCrashKey(const char name[])
      : key_(name), count_(0) {}

  void Update(bool value) {
    if (value) {
      if (count_ < std::numeric_limits<int>::max()) {
        ++count_;
      }
    } else {
      if (count_ > 0) {
        --count_;
      }
    }

    key_.Update(count_);
  }

 private:
  IntegerCrashKey<clear_on_zero> key_;
  int count_ = 0;
};

char const kBookmarkNodesCount[] = "bookmarks";
char const kConnectedScenesCount[] = "connected_scenes";
char const kForegroundScenesCount[] = "foreground_scenes";
char const kHorizontalSizeClass[] = "sizeclass";
char const kInactiveTabsCount[] = "inactive_tabs";
char const kIncognitoTabsCount[] = "incognito_tabs";
char const kIsReaderModeActive[] = "reader_mode";
char const kOrientationState[] = "orient";
char const kRecreatingIncognitoProfile[] = "recreating_incognito_profile";
char const kRegularTabsCount[] = "regular_tabs";
char const kSignedIn[] = "signed_in";
char const kTabsShowingPDFsCount[] = "tabs_showing_pdfs";
char const kUserInterfaceStyle[] = "user_interface_style";
char const kVoiceOverRunning[] = "voice_over";
const char kCrashedAfterAppWillTerminate[] = "crashed_after_app_will_terminate";
const char kCrashedInBackground[] = "crashed_in_background";
const char kFreeMemoryInKB[] = "free_memory_in_kb";
const char kMemoryLimitBytesRemainingInKB[] =
    "memory_limit_bytes_remaining_in_kb";
const char kMemoryWarningCount[] = "memory_warning_count";
const char kMemoryWarningInProgress[] = "memory_warning_in_progress";

}  // namespace

void SetCurrentlyInBackground(bool background) {
  static BooleanCrashKey key(kCrashedInBackground);
  key.Update(background);
}

void SetMemoryWarningCount(int count) {
  static IntegerCrashKey</*clear_on_zero=*/true> key(kMemoryWarningCount);
  key.Update(count);
}

void SetMemoryWarningInProgress(bool value) {
  static BooleanCrashKey key(kMemoryWarningInProgress);
  key.Update(value);
}

void SetCrashedAfterAppWillTerminate() {
  static BooleanCrashKey key(kCrashedAfterAppWillTerminate);
  key.Update(true);
}

void SetCurrentFreeMemoryInKB(int value) {
  static IntegerCrashKey<> key(kFreeMemoryInKB);
  key.Update(value);
}

void SetCurrentMemoryLimitBytesRemainingInKB(int value) {
  static IntegerCrashKey<> key(kMemoryLimitBytesRemainingInKB);
  key.Update(value);
}

void SetCurrentTabIsPDF(bool value) {
  static CounterCrashKey</*clear_on_zero=*/true> key(kTabsShowingPDFsCount);
  key.Update(value);
}

void SetCurrentOrientation(int statusBarOrientation, int deviceOrientation) {
  static IntegerCrashKey<> key(kOrientationState);
  DCHECK((statusBarOrientation < 10) && (deviceOrientation < 10));
  int deviceAndUIOrientation = 10 * statusBarOrientation + deviceOrientation;
  key.Update(deviceAndUIOrientation);
}

void SetCurrentHorizontalSizeClass(int horizontalSizeClass) {
  static IntegerCrashKey<> key(kHorizontalSizeClass);
  key.Update(horizontalSizeClass);
}

void SetCurrentUserInterfaceStyle(int userInterfaceStyle) {
  static IntegerCrashKey<> key(kUserInterfaceStyle);
  key.Update(userInterfaceStyle);
}

void SetCurrentlySignedIn(bool signedIn) {
  static BooleanCrashKey key(kSignedIn);
  key.Update(signedIn);
}

void SetConnectedScenesCount(int connectedScenes) {
  static IntegerCrashKey<> key(kConnectedScenesCount);
  key.Update(connectedScenes);
}

void SetForegroundScenesCount(int foregroundScenes) {
  static IntegerCrashKey<> key(kForegroundScenesCount);
  key.Update(foregroundScenes);
}

void SetRegularTabCount(int tabCount) {
  static IntegerCrashKey<> key(kRegularTabsCount);
  key.Update(tabCount);
  [[PreviousSessionInfo sharedInstance] updateCurrentSessionTabCount:tabCount];
}

void SetInactiveTabCount(int tabCount) {
  static IntegerCrashKey<> key(kInactiveTabsCount);
  key.Update(tabCount);
  [[PreviousSessionInfo sharedInstance]
      updateCurrentSessionInactiveTabCount:tabCount];
}

void SetIncognitoTabCount(int tabCount) {
  static IntegerCrashKey<> key(kIncognitoTabsCount);
  key.Update(tabCount);
  [[PreviousSessionInfo sharedInstance]
      updateCurrentSessionOTRTabCount:tabCount];
}

void SetDestroyingAndRebuildingIncognitoBrowserState(bool in_progress) {
  static BooleanCrashKey key(kRecreatingIncognitoProfile);
  key.Update(in_progress);
}

void SetBookmarkNodesCount(int bookmarks_count, ProfileIOS* profile) {
  static IntegerCrashKey<> key(kBookmarkNodesCount);
  key.Update(bookmarks_count);
}

void SetVoiceOverRunning(bool running) {
  static BooleanCrashKey key(kVoiceOverRunning);
  key.Update(running);
}

void SetCurrentlyInReaderMode(bool is_reader_mode_active) {
  static BooleanCrashKey key(kIsReaderModeActive);
  key.Update(is_reader_mode_active);
}

}  // namespace crash_keys
