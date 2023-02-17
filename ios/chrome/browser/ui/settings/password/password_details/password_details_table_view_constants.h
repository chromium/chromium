// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/list_model/list_model.h"

// The accessibility identifier of the password details table view.
extern NSString* const kPasswordDetailsViewControllerId;

// UI items for password details
typedef NS_ENUM(NSInteger, PasswordDetailsItemType) {
  PasswordDetailsItemTypeWebsite = kItemTypeEnumZero,
  PasswordDetailsItemTypeUsername,
  PasswordDetailsItemTypePassword,
  PasswordDetailsItemTypeNote,
  PasswordDetailsItemTypeFederation,
  PasswordDetailsItemTypeChangePasswordButton,
  PasswordDetailsItemTypeChangePasswordRecommendation,
  PasswordDetailsItemTypeDeleteButton,
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONSTANTS_H_
