// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_UI_CREDENTIAL_IMPORT_ITEM_CELL_CONTENT_VIEW_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_UI_CREDENTIAL_IMPORT_ITEM_CELL_CONTENT_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/data_import/ui/credential_import_item_cell_content_configuration.h"

// A view that displays the content of a credential import item cell.
@interface CredentialImportItemCellContentView : UIView <UIContentView>

// The configuration of the view.
@property(nonatomic, copy)
    CredentialImportItemCellContentConfiguration* configuration;

// Initializes the view with the given configuration.
- (instancetype)initWithConfiguration:
    (CredentialImportItemCellContentConfiguration*)configuration;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_UI_CREDENTIAL_IMPORT_ITEM_CELL_CONTENT_VIEW_H_
