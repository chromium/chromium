// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AutofillAIEntityEditTableViewController;
@class TableViewItem;
@class AutofillAIEntityCountryItem;

// Delegate for the AutofillAIEntityEditTableViewController.
@protocol AutofillAIEntityEditTableViewControllerDelegate

// Notifies the delegate that the user tapped on a country item.
- (void)didTapCountryItem:(AutofillAIEntityCountryItem*)item;

// Notifies the delegate that the user tapped on the close button.
- (void)didTapCloseButton:
    (AutofillAIEntityEditTableViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
