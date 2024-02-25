// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_TESTING_H_

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"

// Testing category to expose private methods of
// PasswordDetailsTableViewController for testing.
@interface PasswordDetailsTableViewController (Testing)
#if !defined(__IPHONE_16_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_16_0
- (void)copyPasswordDetails:(id)sender;
#endif

- (void)copyPasswordDetailsHelper:(NSInteger)itemType;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_TESTING_H_
