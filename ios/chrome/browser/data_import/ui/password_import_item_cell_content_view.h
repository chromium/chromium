// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_UI_PASSWORD_IMPORT_ITEM_CELL_CONTENT_VIEW_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_UI_PASSWORD_IMPORT_ITEM_CELL_CONTENT_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/data_import/ui/password_import_item_cell_content_configuration.h"

// A view that displays the content of a password import item cell.
@interface PasswordImportItemCellContentView : UIView <UIContentView>

// The configuration of the view.
@property(nonatomic, copy)
    PasswordImportItemCellContentConfiguration* configuration;

// Initializes the view with the given configuration.
- (instancetype)initWithConfiguration:
    (PasswordImportItemCellContentConfiguration*)configuration;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_UI_PASSWORD_IMPORT_ITEM_CELL_CONTENT_VIEW_H_
