// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_PRIVATE_H_

// Class extension exposing private methods of
// PasswordDetailsTableViewController for testing.
@interface PasswordDetailsTableViewController ()
#if !defined(__IPHONE_16_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_16_0
- (void)copyPasswordDetails:(id)sender;
#endif

- (void)copyPasswordDetailsHelper:(NSInteger)itemType;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_PRIVATE_H_
