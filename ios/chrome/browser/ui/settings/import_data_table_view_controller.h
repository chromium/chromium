// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_IMPORT_DATA_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_IMPORT_DATA_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@class ImportDataTableViewController;

// The accessibility identifier of the Import Data cell.
extern NSString* const kImportDataImportCellId;

// The accessibility identifier of the Keep Data Separate cell.
extern NSString* const kImportDataKeepSeparateCellId;

// The accessibility identifier of the Continue navigation button.
extern NSString* const kImportDataContinueButtonId;

// Notifies of the user action on the corresponding
// ImportDataTableViewController.
@protocol ImportDataControllerDelegate

// Indicates that the user chose the clear data policy to be `shouldClearData`
// when presented with `controller`.
- (void)didChooseClearDataPolicy:(ImportDataTableViewController*)controller
                 shouldClearData:(ShouldClearData)shouldClearData;

@end

// Table View that handles how to import data during account switching.
@interface ImportDataTableViewController : SettingsRootTableViewController

// `fromEmail` is the email of the previously signed in account.
// `toIdentity` is the email of the account switched to.
//
// `fromEmail` and `toEmail` must not be NULL.
- (instancetype)initWithDelegate:(id<ImportDataControllerDelegate>)delegate
                       fromEmail:(NSString*)fromEmail
                         toEmail:(NSString*)toEmail NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_IMPORT_DATA_TABLE_VIEW_CONTROLLER_H_
