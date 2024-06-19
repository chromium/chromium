// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_TEXT_CELL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_TEXT_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"

// Wrapper to use a simple text cell with a custom bottom separator for manual
// fill tables.
@interface ManualFillTextItem : TableViewTextItem
// Defaults to NO. If YES, the corresponding cell will show a custom bottom
// separator.
@property(nonatomic, assign) BOOL showSeparator;
@end

// A simple text table view cell with a custom bottom separator for manual fill
// tables.
@interface ManualFillTextCell : TableViewTextCell
// Defaults to NO. If YES, shows a custom bottom separator.
@property(nonatomic, assign) BOOL showSeparator;
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_TEXT_CELL_H_
