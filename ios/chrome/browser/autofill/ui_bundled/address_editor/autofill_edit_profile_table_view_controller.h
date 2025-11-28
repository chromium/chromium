// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_EDIT_PROFILE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_EDIT_PROFILE_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_handler.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

@protocol AutofillEditProfileTableViewControllerDelegate

// Invoked when the "Cancel" button is pressed.
- (void)didCancelBottomSheetView;

@end

// The Bottom Sheet TableView for an Autofill save/update address edit menu.
@interface AutofillEditProfileTableViewController
    : LegacyChromeTableViewController

- (instancetype)initWithDelegate:
                    (id<AutofillEditProfileTableViewControllerDelegate>)delegate
                   editSheetMode:(AutofillSaveProfilePromptMode)editSheetMode
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@property(nonatomic, weak) id<AutofillProfileEditHandler> handler;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_EDIT_PROFILE_TABLE_VIEW_CONTROLLER_H_
