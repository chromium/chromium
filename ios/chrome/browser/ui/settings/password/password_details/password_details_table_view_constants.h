// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

// The accessibility identifier of the password details table view.
extern NSString* const kPasswordDetailsViewControllerID;

// The accessibility identifier of the button to move local passwords to the
// account.
extern NSString* const kMovePasswordToAccountButtonID;

// The accessibility identifier of the compromised warning.
extern NSString* const kCompromisedWarningID;

// The accessibility identifier of the password sharing button.
extern NSString* const kPasswordShareButtonID;

// UI items for password details
typedef NS_ENUM(NSInteger, PasswordDetailsItemType) {
  PasswordDetailsItemTypeWebsite = kItemTypeEnumZero,
  PasswordDetailsItemTypeUsername,
  PasswordDetailsItemTypePassword,
  PasswordDetailsItemTypeNote,
  PasswordDetailsItemTypeNoteFooter,
  PasswordDetailsItemTypeFederation,
  PasswordDetailsItemTypeChangePasswordButton,
  PasswordDetailsItemTypeChangePasswordRecommendation,
  PasswordDetailsItemTypeDismissWarningButton,
  PasswordDetailsItemTypeRestoreWarningButton,
  PasswordDetailsItemTypeDeleteButton,
  PasswordDetailsItemTypeMoveToAccountButton,
  PasswordDetailsItemTypeMoveToAccountRecommendation,
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONSTANTS_H_
