// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BRING_ANDROID_TABS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BRING_ANDROID_TABS_COMMANDS_H_

// Protocol for commands that manages the presentation cycle of the Bring
// Android Tabs, both the prompt and the review tabs list.
@protocol BringAndroidTabsCommands

// Display the Bring Android Tabs prompt in the UI hierarchy.
- (void)displayBringAndroidTabsPrompt;

// Remove the Bring Android Tabs prompt from the UI hierarchy and bring up the
// modal used to review the list of tabs.
- (void)reviewAllBringAndroidTabs;

// Remove the Bring Android Tabs prompt from the UI hierarchy and end the user
// journey for Bring Android Tabs.
- (void)dismissBringAndroidTabsPrompt;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BRING_ANDROID_TABS_COMMANDS_H_
