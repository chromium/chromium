// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AutofillAIEntityCountryItem;
@class AutofillAIEntityEditTableViewController;
@class CrURL;
@class TableViewItem;

// Delegate for the AutofillAIEntityEditTableViewController.
@protocol AutofillAIEntityEditTableViewControllerDelegate

// Notifies the delegate that the user tapped on a country item.
- (void)didTapCountryItem:(AutofillAIEntityCountryItem*)item;

// Notifies the delegate that the user tapped on the close button.
- (void)dismissViewController:
    (AutofillAIEntityEditTableViewController*)viewController;

// Called when the user taps the Edit button on a Server Wallet item.
- (void)didTapEditInWalletButton:
    (AutofillAIEntityEditTableViewController*)viewController;

// Called when the entity is saved locally and an alert needs to be shown.
- (void)showLocalSaveFallbackAlert;

// Called when the user taps on a link in the footer.
- (void)didTapLinkWithURL:(CrURL*)url;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
