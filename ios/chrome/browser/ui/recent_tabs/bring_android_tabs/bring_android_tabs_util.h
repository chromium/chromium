// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_BRING_ANDROID_TABS_BRING_ANDROID_TABS_UTIL_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_BRING_ANDROID_TABS_BRING_ANDROID_TABS_UTIL_H_

#include "ios/chrome/browser/synced_sessions/distant_tab.h"

class ChromeBrowserState;
class PrefService;

// Returns a list of the user's recent tabs if they meet all the prerequisites
// to be shown the Bring Android Tabs prompt. Returns an empty list in all other
// cases.
synced_sessions::DistantTabVector PromptTabsForAndroidSwitcher(
    ChromeBrowserState* browser_state);

// Called when the Bring Android Tabs Prompt has been displayed.
// Takes browser state prefs as input.
void OnBringAndroidTabsPromptDisplayed(PrefService* user_prefs);

// Called when the user interacts with the Bring Android Tabs prompt.
void OnUserInteractWithBringAndroidTabsPrompt();

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_BRING_ANDROID_TABS_BRING_ANDROID_TABS_UTIL_H_
