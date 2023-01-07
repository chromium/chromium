// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_NAVIGATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_NAVIGATION_COMMANDS_H_

// Commands related to the privacy navigation inside the privacy safe browsing
// view controller.
@protocol PrivacySafeBrowsingNavigationCommands

// Shows Safe Browsing Enhanced Protection screen.
- (void)showSafeBrowsingEnhancedProtection;

// Shows Safe Browsing Standard Protection screen.
- (void)showSafeBrowsingStandardProtection;

// Called when the Safe Browsing No Protection cell is selected
// and triggers a pop up
- (void)showSafeBrowsingNoProtectionPopUp:(TableViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_NAVIGATION_COMMANDS_H_
