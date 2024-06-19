// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ACTION_CELL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ACTION_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Wrapper to use Action Cells in Chrome Table Views.
@interface ManualFillActionItem : TableViewItem

- (instancetype)initWithTitle:(NSString*)title
                       action:(void (^)(void))action NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

// A table view cell which contains a button and holds an action block, which
// is called when the button is touched.
@interface ManualFillActionCell : TableViewCell
// Updates the cell with the passed title and action block.
- (void)setUpWithTitle:(NSString*)title
       accessibilityID:(NSString*)accessibilityID
                action:(void (^)(void))action;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ACTION_CELL_H_
