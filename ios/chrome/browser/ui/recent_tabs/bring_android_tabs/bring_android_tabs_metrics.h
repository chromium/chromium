// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_BRING_ANDROID_TABS_BRING_ANDROID_TABS_METRICS_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_BRING_ANDROID_TABS_BRING_ANDROID_TABS_METRICS_H_

namespace bring_android_tabs {

// The result of prompting the set of tabs for an Android switcher. This is
// mapped to the IOSPromptTabsForAndroidSwitcherState enum in enums.xml for
// metrics.
enum class IOSPromptTabsForAndroidSwitcherState {
  kSyncDisabled = 0,
  kSegmentationIncomplete = 1,
  kPromptDismissed = 2,
  kTabSyncDisabled = 3,
  kNoActiveTabs = 4,
  kSuccess = 5,
  kMaxValue = kSuccess,
}

// Interactions with the initial Bring Android Tabs prompt. This is mapped to
// the IOSBringAndroidTabsPromptActionType enum in enums.xml for metrics.
enum class IOSBringAndroidTabsPromptActionType {
  kReviewTabs = 0,
  kOpenTabs = 1,
  kCancel = 2,
  kSwipeToDismiss = 3,
  kMaxValue = kSwipeToDismiss,
}

// Interactions with the Bring Android Tabs Tab List view. This is mapped to the
// IOSBringAndroidTabsTabsListActionType enum in enums.xml for metrics.
enum class IOSBringAndroidTabsTabsListActionType {
  kCancel = 0,
  kSwipeDown = 1,
  kOpenTabs = 2,
  kMaxValue = kOpenTabs,
}

}  // namespace bring_android_tabs

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_BRING_ANDROID_TABS_BRING_ANDROID_TABS_METRICS_H_
