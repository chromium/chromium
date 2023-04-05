// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONSTANTS_H_

#import <Foundation/Foundation.h>
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

// The accessibility identifier of the password details table view.
extern NSString* const kPasswordsTableViewId;
extern NSString* const kPasswordsSearchBarId;
extern NSString* const kPasswordsScrimViewId;

// The accessibility identifier of the password details table view.
extern NSString* const kPasswordDetailsTableViewId;
extern NSString* const kPasswordDetailsDeletionAlertViewId;
extern NSString* const kPasswordsAddPasswordSaveButtonId;
extern NSString* const kPasswordsAddPasswordCancelButtonId;

// The accessibility identifier of the password issues table view.
extern NSString* const kPasswordIssuesTableViewId;

// The accessibility identifier of the large "Add Password..." button when
// displayed in the table.
extern NSString* const kAddPasswordButtonId;

// Delete button accessibility identifier for Password Details.
extern NSString* const kDeleteButtonForPasswordDetailsId;

// The accessibility identifier of the icon that informs the user a password is
// only stored locally and not backed up to any account.
extern NSString* const kLocalOnlyPasswordIconId;

// Sections of the password settings
typedef NS_ENUM(NSInteger, PasswordSectionIdentifier) {
  SectionIdentifierSavedPasswords = kSectionIdentifierEnumZero,
  SectionIdentifierBlocked,
  SectionIdentifierPasswordCheck,
  SectionIdentifierAddPasswordButton,
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONSTANTS_H_
