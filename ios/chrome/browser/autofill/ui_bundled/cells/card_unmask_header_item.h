// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_CARD_UNMASK_HEADER_ITEM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_CARD_UNMASK_HEADER_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

// Item to represent and configure a CardUnmaskHeaderCell.
@interface CardUnmaskHeaderItem : TableViewHeaderFooterItem

// The title text to display.
@property(nonatomic, copy) NSString* titleText;

// The instructions text to display.
@property(nonatomic, copy) NSString* instructionsText;

- (instancetype)initWithType:(NSInteger)type
                   titleText:(NSString*)titleText
            instructionsText:(NSString*)instructionsText
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

// Returns the accessibility labels of the CardUnmaskHeaderItem.
- (NSString*)accessibilityLabels;

@end

// Header view of a card unmask prompt.
@interface CardUnmaskHeaderView : UITableViewHeaderFooterView

// The label displaying the title.
@property(nonatomic, readonly, strong) UILabel* titleLabel;

// The label displaying instructions.
@property(nonatomic, readonly, strong) UILabel* instructionsLabel;

@end
#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_CARD_UNMASK_HEADER_ITEM_H_
