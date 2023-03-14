// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_BRING_ANDROID_TABS_BRING_ANDROID_TABS_UTIL_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_BRING_ANDROID_TABS_BRING_ANDROID_TABS_UTIL_H_

#include <vector>

#include "ios/chrome/browser/ui/recent_tabs/synced_sessions.h"

// Returns a list of the user's recent tabs if they meet all the prerequisites
// to be shown the Bring Android Tabs prompt. Returns an empty list in all other
// cases.
std::vector<std::unique_ptr<synced_sessions::DistantTab>>
PromptTabsForAndroidSwitcher();

// Called when the Bring Android Tabs Prompt has been displayed.
void OnPromptDisplayed();

// Called when the user interacts with the Bring Android Tabs prompt.
void OnUserInteractWithPrompt();

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_BRING_ANDROID_TABS_BRING_ANDROID_TABS_UTIL_H_
