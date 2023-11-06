// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MENU_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MENU_ITEM_H_

#import <UIKit/UIKit.h>

// TODO(crbug.com/1489457): Delete this file when cleaning up deprecation for
// iOS 15.
#if !defined(__IPHONE_16_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_16_0
// Menu item which holds item type. Possible types `website`, `username` or
// `password`.
@interface PasswordDetailsMenuItem : UIMenuItem

@property(nonatomic, assign) NSInteger itemType;

@end

#endif

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MENU_ITEM_H_
