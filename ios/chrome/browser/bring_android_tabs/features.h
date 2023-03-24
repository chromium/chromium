// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_FEATURES_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to turn on the prompt that brings the user's Android tabs to iOS
// Chrome.
BASE_DECLARE_FEATURE(kBringYourOwnTabsIOS);

// Enum for "Bring Your Own Tabs" experiment groups.
enum class BringYourOwnTabsPromptType {
  // "Bring Your Own Tabs" enabled with half sheet modal prompt.
  kHalfSheet = 0,
  // "Bring Your Own Tabs" enabled with bottom message prompt.
  kBottomMessage,
  // "Bring Your Own Tabs" not enabled.
  kDisabled,
};

// Feature param name that specifies the prompt type.
extern const char kBringYourOwnTabsIOSParam[];

// Returns the current BringYourOwnTabsPromptType according to the feature flag
// and experiment "BringYourOwnTabsIOS".
BringYourOwnTabsPromptType GetBringYourOwnTabsPromptType();

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_FEATURES_H_
