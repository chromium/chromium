/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_INVALID_PASSWORDS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_INVALID_PASSWORDS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@class PasswordImportItem;

/// View controller listing password errors encountered during data import.
@interface DataImportInvalidPasswordsViewController : ChromeTableViewController

- (instancetype)initWithInvalidPasswords:
    (NSArray<PasswordImportItem*>*)passwords NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_INVALID_PASSWORDS_VIEW_CONTROLLER_H_
