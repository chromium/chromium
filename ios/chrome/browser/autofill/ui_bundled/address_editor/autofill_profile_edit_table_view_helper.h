// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_HELPER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_HELPER_H_

#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_handler.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

@protocol AutofillProfileEditTableViewHelperDelegate;

// The table view shared between the settings and messages UI for the edit
// functionality.
@interface AutofillProfileEditTableViewHelper
    : NSObject <AutofillProfileEditConsumer,
                AutofillProfileEditHandler,
                TableViewTextEditItemDelegate>

// Initializes a AutofillProfileEditTableViewHelper with passed `delegate`
// and `userEmail`.
- (instancetype)initWithDelegate:
                    (id<AutofillProfileEditTableViewHelperDelegate>)delegate
                       userEmail:(NSString*)userEmail
                      controller:(LegacyChromeTableViewController*)controller
                  addressContext:(SaveAddressContext)addressContext
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_HELPER_H_
