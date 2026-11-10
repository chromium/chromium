// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_settings_consumer.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_settings_mutator.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@class AutofillSettingsTableViewController;

// Delegate for presentation events related to
// AutofillSettingsTableViewController.
@protocol AutofillSettingsTableViewControllerDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)autofillSettingsTableViewControllerDidRemove:
    (AutofillSettingsTableViewController*)controller;

// Called when the user taps on the Google Wallet promo button.
- (void)autofillSettingsTableViewControllerDidTapWalletPromoCard:
    (AutofillSettingsTableViewController*)controller;

@end

// The TableView for Autofill settings page.
@interface AutofillSettingsTableViewController
    : SettingsRootTableViewController <AutofillSettingsConsumer,
                                       SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak) id<AutofillSettingsTableViewControllerDelegate>
    delegate;

// Mutator for this view controller.
@property(nonatomic, weak) id<AutofillSettingsMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_SETTINGS_TABLE_VIEW_CONTROLLER_H_
