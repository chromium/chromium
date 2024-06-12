// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_TABLE_VIEW_CONTROLLER_ACTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_TABLE_VIEW_CONTROLLER_ACTION_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AccountPickerSelectionScreenTableViewController;

// Delegate protocol for AccountPickerSelectionScreenTableViewController.
@protocol
    AccountPickerSelectionScreenTableViewControllerActionDelegate <NSObject>

// Invoked when the user selects an identity.
- (void)accountPickerListTableViewController:
            (AccountPickerSelectionScreenTableViewController*)viewController
                 didSelectIdentityWithGaiaID:(NSString*)gaiaID;
// Invoked when the user taps on "Add account".
- (void)accountPickerListTableViewControllerDidTapOnAddAccount:
    (AccountPickerSelectionScreenTableViewController*)viewController;

// Show management help page.
- (void)showManagementHelpPage;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_TABLE_VIEW_CONTROLLER_ACTION_DELEGATE_H_
