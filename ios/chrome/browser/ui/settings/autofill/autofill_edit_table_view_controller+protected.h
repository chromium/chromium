// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_EDIT_TABLE_VIEW_CONTROLLER_PROTECTED_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_EDIT_TABLE_VIEW_CONTROLLER_PROTECTED_H_

#import "ios/chrome/browser/ui/settings/autofill/autofill_edit_table_view_controller.h"

// The table view for an Autofill edit entry menu.
@interface AutofillEditTableViewController (Protected)

// Returns the indexPath for the currently focused text field when in edit mode.
- (NSIndexPath*)indexPathForCurrentTextField;

// Returns whether cell at indexPath is editCell.
- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_EDIT_TABLE_VIEW_CONTROLLER_PROTECTED_H_
