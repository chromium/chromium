// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DISCLOSURE_HEADER_FOOTER_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DISCLOSURE_HEADER_FOOTER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

// TableViewDisclosureHeaderFooterItem contains the model data for a
// TableViewDisclosureHeaderFooterView.
@interface TableViewDisclosureHeaderFooterItem : TableViewHeaderFooterItem
// Title of Header.
@property(nonatomic, readwrite, strong) NSString* text;
// Header subtitle displayed as a smaller font under title.
@property(nonatomic, readwrite, strong) NSString* subtitleText;
// Determines the direction of the disclosure view.
@property(nonatomic, readwrite, assign) BOOL collapsed;
// Determines if the header is shown has disabled.
@property(nonatomic, readwrite, assign) BOOL disabled;
@end

// UITableViewHeaderFooterView that displays a text label, subtitle, and a
// disclosure accessory view.
@interface TableViewDisclosureHeaderFooterView : UITableViewHeaderFooterView
// Indicates in what direction the disclosure accessory should point.
typedef NS_ENUM(NSInteger, DisclosureDirection) {
  DisclosureDirectionTrailing = 2,
  DisclosureDirectionDown,
};
// Shows the text of the TableViewDisclosureHeaderFooterItem.
@property(nonatomic, readwrite, strong) UILabel* titleLabel;
// Shows the subtitleText of the TableViewDisclosureHeaderFooterItem.
@property(nonatomic, readwrite, strong) UILabel* subtitleLabel;
// Determines if the header is shown has disabled.
@property(nonatomic, readwrite, assign) BOOL disabled;
// Determines if disclosureImageView should be pointing down or to the right.
@property(nonatomic, assign) DisclosureDirection disclosureDirection;
// Sets initial direction of disclosure view.
- (void)setInitialDirection:(DisclosureDirection)direction;
// Rotates the disclosure view if the direction parameter is different from its
// current state.
- (void)rotateToDirection:(DisclosureDirection)direction;
@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DISCLOSURE_HEADER_FOOTER_ITEM_H_
