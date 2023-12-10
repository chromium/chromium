// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_NAVIGATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_NAVIGATION_COMMANDS_H_

// Commands related to the safety check navigation inside the safety check view
// controller.
@protocol SafetyCheckNavigationCommands

// Shows the Password Checkup page.
- (void)showPasswordCheckupPage;

// Opens update page at `location`.
- (void)showUpdateAtLocation:(NSString*)location;

// Shows page with Safe Browsing preference toggle.
- (void)showSafeBrowsingPreferencePage;

// Shows the error popover with the corresponding `text`.
- (void)showErrorInfoFrom:(UIButton*)buttonView
                 withText:(NSAttributedString*)text;

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)showManagedInfoFrom:(UIButton*)buttonView;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_NAVIGATION_COMMANDS_H_
