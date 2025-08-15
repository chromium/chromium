// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_CVC_STORAGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_CVC_STORAGE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_edit_table_view_controller.h"

@class AutofillCvcStorageViewController;

// Delegate for AutofillCvcStorageViewController.
@protocol AutofillCvcStorageViewControllerDelegate
@end

// This class is responsible for displaying CVC storage settings.
@interface AutofillCvcStorageViewController : AutofillEditTableViewController

// The delegate for this view controller.
@property(nonatomic, weak) id<AutofillCvcStorageViewControllerDelegate>
    delegate;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_DESIGNATED_INITIALIZER;
- (instancetype)init;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_CVC_STORAGE_VIEW_CONTROLLER_H_
