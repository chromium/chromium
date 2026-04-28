// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AND_PASSWORDS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AND_PASSWORDS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@class AutofillAndPasswordsTableViewController;

// Delegate for presentation events related to
// AutofillAndPasswordsTableViewController.
@protocol AutofillAndPasswordsTableViewControllerDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)autofillAndPasswordsTableViewControllerDidRemove:
    (AutofillAndPasswordsTableViewController*)controller;

// Called when the user taps on the passwords item.
- (void)autofillAndPasswordsTableViewControllerDidSelectPasswords:
    (AutofillAndPasswordsTableViewController*)controller;

// Called when the user taps on the autofill credit card item.
- (void)autofillAndPasswordsTableViewControllerDidSelectAutofillCreditCard:
    (AutofillAndPasswordsTableViewController*)controller;

// Called when the user taps on the autofill profile item.
- (void)autofillAndPasswordsTableViewControllerDidSelectAutofillProfile:
    (AutofillAndPasswordsTableViewController*)controller;

// Called when the user taps on the identity docs item.
- (void)autofillAndPasswordsTableViewControllerDidSelectIdentityDocs:
    (AutofillAndPasswordsTableViewController*)controller;

@end

// The TableView for Autofill and passwords settings page.
@interface AutofillAndPasswordsTableViewController
    : SettingsRootTableViewController <AutofillAndPasswordsConsumer,
                                       SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak) id<AutofillAndPasswordsTableViewControllerDelegate>
    delegate;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AND_PASSWORDS_TABLE_VIEW_CONTROLLER_H_
