// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_NAVIGATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_NAVIGATION_COMMANDS_H_

// Commands related to the privacy navigation inside the privacy view
// controller.
@protocol PrivacyNavigationCommands

// Shows Handoff screen.
- (void)showHandoff;

// Shows ClearBrowsingData screen.
- (void)showClearBrowsingData;

// Shows Safe Browsing screen.
- (void)showSafeBrowsing;

// Shows Lockdown Mode screen.
- (void)showLockdownMode;

// Show Privacy Guide screen.
- (void)showPrivacyGuide;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_NAVIGATION_COMMANDS_H_
