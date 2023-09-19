// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_IDENTITY_BUTTON_CELL_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_IDENTITY_BUTTON_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"

@class IdentityButtonControl;

// A table view cell containing an IdentityButtonControl.
@interface IdentityButtonCell : TableViewCell

@property(nonatomic, strong, readonly)
    IdentityButtonControl* identityButtonControl;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_IDENTITY_BUTTON_CELL_H_
