// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONSTANTS_H_

#import <Foundation/Foundation.h>
#import "ios/chrome/browser/ui/list_model/list_model.h"

// The accessibility identifier of the password details table view.
extern NSString* const kPasswordsTableViewId;
extern NSString* const kPasswordsExportConfirmViewId;
extern NSString* const kPasswordsSearchBarId;
extern NSString* const kPasswordsScrimViewId;

// The accessibility identifier of on device encryption.
extern NSString* const kOnDeviceEncryptionOptInId;
extern NSString* const kOnDeviceEncryptionLearnMoreId;
extern NSString* const kOnDeviceEncryptionOptedInTextId;
extern NSString* const kOnDeviceEncryptionSetUpId;

// The accessibility identifier of the password details table view.
extern NSString* const kPasswordDetailsTableViewId;
extern NSString* const kPasswordDetailsDeletionAlertViewId;
extern NSString* const kPasswordsAddPasswordSaveButtonId;
extern NSString* const kPasswordsAddPasswordCancelButtonId;

// The accessibility identifier of the save password item.
extern NSString* const kSavePasswordSwitchTableViewId;
extern NSString* const kSavePasswordManagedTableViewId;

// The accessibility identifier of the password in other apps item.
extern NSString* const kSettingsPasswordsInOtherAppsCellId;

// The accessibility identifier of the password issues table view.
extern NSString* const kPasswordIssuesTableViewId;

// The accessibility identifier of the large "Add Password..." button when
// displayed in the table.
extern NSString* const kAddPasswordButtonId;

// Sections of the password settings
typedef NS_ENUM(NSInteger, PasswordSectionIdentifier) {
  SectionIdentifierSavePasswordsSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierSavedPasswords,
  SectionIdentifierPasswordsInOtherApps,
  SectionIdentifierBlocked,
  SectionIdentifierExportPasswordsButton,
  SectionIdentifierPasswordCheck,
  SectionIdentifierOnDeviceEncryption,
  SectionIdentifierAddPasswordButton,
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONSTANTS_H_
