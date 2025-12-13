// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_CVC_STORAGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_CVC_STORAGE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_cvc_storage_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_edit_table_view_controller.h"

@class AutofillCvcStorageViewController;

// Delegate for AutofillCvcStorageViewController.
@protocol AutofillCvcStorageViewControllerDelegate
// Informs the delegate that the CVC storage switch has changed value.
- (void)viewController:(AutofillCvcStorageViewController*)controller
    didChangeCvcStorageSwitchTo:(BOOL)isOn;
// Informs the delegate that the user wants to delete all saved CVCs.
- (void)deleteAllSavedCvcsForViewController:
    (AutofillCvcStorageViewController*)controller;
@end

// This class is responsible for displaying CVC storage settings.
@interface AutofillCvcStorageViewController
    : AutofillEditTableViewController <AutofillCvcStorageConsumer>

// The delegate for this view controller.
@property(nonatomic, weak) id<AutofillCvcStorageViewControllerDelegate>
    delegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_CVC_STORAGE_VIEW_CONTROLLER_H_
