// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_handler.h"

@protocol AutofillProfileEditTableViewControllerDelegate;

// The table view shared between the settings and messages UI for the edit
// functionality.
@interface AutofillProfileEditTableViewController
    : NSObject <AutofillProfileEditConsumer,
                AutofillProfileEditHandler,
                TableViewTextEditItemDelegate>

// Initializes a AutofillProfileEditTableViewController with passed `delegate`
// and `userEmail`.
- (instancetype)initWithDelegate:
                    (id<AutofillProfileEditTableViewControllerDelegate>)delegate
                       userEmail:(NSString*)userEmail
                      controller:(LegacyChromeTableViewController*)controller
                    settingsView:(BOOL)settingsView NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_H_
