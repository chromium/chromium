// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_AUTOFILL_EDIT_PROFILE_BOTTOM_SHEET_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_AUTOFILL_EDIT_PROFILE_BOTTOM_SHEET_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_handler.h"

// The Bottom Sheet TableView for an Autofill save/update address edit menu.
@interface AutofillEditProfileBottomSheetTableViewController
    : LegacyChromeTableViewController

@property(nonatomic, weak) id<AutofillProfileEditHandler> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_AUTOFILL_EDIT_PROFILE_BOTTOM_SHEET_TABLE_VIEW_CONTROLLER_H_
