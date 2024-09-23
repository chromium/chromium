// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

// The accessibility identifier of the Password Manager table view.
extern NSString* const kPasswordsTableViewID;
extern NSString* const kPasswordsSearchBarID;
extern NSString* const kPasswordsScrimViewID;

// The accessibility identifier of the password details table view.
extern NSString* const kPasswordDetailsTableViewID;
extern NSString* const kPasswordDetailsDeletionAlertViewID;
extern NSString* const kPasswordsAddPasswordSaveButtonID;
extern NSString* const kPasswordsAddPasswordCancelButtonID;

// The accessibility identifier of the password issues table view.
extern NSString* const kPasswordIssuesTableViewID;

// The accessibility identifier of the "Dismissed Warnings" cell in the password
// issues table view.
extern NSString* const kDismissedWarningsCellID;

// The accessibility identifier of the large "Add Password..." button when
// displayed in the table.
extern NSString* const kAddPasswordButtonID;

// Username text field accessibility identifier for Password Details.
extern NSString* const kUsernameTextfieldForPasswordDetailsID;

// User Display Name text field accessibility identifier for Password Details.
extern NSString* const kUserDisplayNameTextfieldForPasswordDetailsID;

// Creation Date text field accessibility identifier for Password Details.
extern NSString* const kCreationDateTextfieldForPasswordDetailsID;

// Password text field accessibility identifier for Password Details.
extern NSString* const kPasswordTextfieldForPasswordDetailsID;

// Delete button accessibility identifier for Password Details.
extern NSString* const kDeleteButtonForPasswordDetailsID;

// The accessibility identifier of the icon that informs the user a password is
// only stored locally and not backed up to any account.
extern NSString* const kLocalOnlyPasswordIconID;

// Returns the name of the image shown in the Password Manager widget
// promo that's presented in the Password Manager.
NSString* WidgetPromoImageName();

// Returns the name of the image shown in the Password Manager widget
// promo that's presented in the Password Manager when the promo cell
// is disabled.
NSString* WidgetPromoDisabledImageName();

// Accessibility identifier for the Password Manager widget promo.
extern NSString* const kWidgetPromoID;

// Accessibility identifier for the Password Manager widget promo's close
// button.
extern NSString* const kWidgetPromoCloseButtonID;

// Accessibility identifier for the Password Manager widget promo's image.
extern NSString* const kWidgetPromoImageID;

// Name of histogram tracking actions taken on the Password Manager widget
// promo.
extern const char kPasswordManagerWidgetPromoActionHistogram[];

// Enum for the IOS.PasswordManager.WidgetPromo.Action histogram. Keep in sync
// with the "PromoWithInstructionsAction" enum.
// LINT.IfChange
enum class PasswordManagerWidgetPromoAction {
  kClose = 0,             // The user closed the promo.
  kOpenInstructions = 1,  // The user opened the instruction view.
  kMaxValue = kOpenInstructions,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:PromoWithInstructionsAction)

// Sections of the password settings
typedef NS_ENUM(NSInteger, PasswordSectionIdentifier) {
  SectionIdentifierSavedPasswords = kSectionIdentifierEnumZero,
  SectionIdentifierBlocked,
  SectionIdentifierPasswordCheck,
  SectionIdentifierAddPasswordButton,
  SectionIdentifierManageAccountHeader,
  SectionIdentifierWidgetPromo,
};

// Enum with all possible UI states for the Password Manager's Password Checkup
// cell.
typedef NS_ENUM(NSInteger, PasswordCheckUIState) {
  // When no insecure passwords were detected.
  PasswordCheckStateSafe,
  // When user has unmuted compromised passwords.
  PasswordCheckStateUnmutedCompromisedPasswords,
  // When user has reused passwords.
  PasswordCheckStateReusedPasswords,
  // When user has weak passwords.
  PasswordCheckStateWeakPasswords,
  // When user has dismissed warnings.
  PasswordCheckStateDismissedWarnings,
  // When check was not perfect and state is unclear.
  PasswordCheckStateDefault,
  // When password check is running.
  PasswordCheckStateRunning,
  // When user has no passwords and check can't be performed.
  PasswordCheckStateDisabled,
  // When password check failed due to network issues, quota limit or others.
  PasswordCheckStateError,
  // When password check failed due to user being signed out.
  PasswordCheckStateSignedOut,
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONSTANTS_H_
