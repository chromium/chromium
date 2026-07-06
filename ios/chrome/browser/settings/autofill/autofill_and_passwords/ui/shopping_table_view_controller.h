// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_SHOPPING_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_SHOPPING_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/shopping_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@protocol AutofillAIBaseMutator;

// The TableView for Shopping settings page.
@interface ShoppingTableViewController
    : SettingsRootTableViewController <ShoppingConsumer>

// Mutator for actions in the view.
@property(nonatomic, weak) id<AutofillAIBaseMutator> mutator;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_SHOPPING_TABLE_VIEW_CONTROLLER_H_
