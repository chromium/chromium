// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_HEADER_FOOTER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_HEADER_FOOTER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_header_footer_item.h"

@class CrURL;
@class TableViewTextHeaderFooterView;

// The protocol that invokes actions for the associated links embedded in text.
@protocol TableViewTextHeaderFooterItemDelegate <NSObject>

// Notifies the delegate that the link corresponding to `URL` was tapped in
// `view`.
- (void)view:(TableViewTextHeaderFooterView*)view didTapLinkURL:(CrURL*)URL;

@end

// TableViewTextHeaderFooterItem is the model class corresponding to
// TableViewTextHeaderFooterView.
@interface TableViewTextHeaderFooterItem : TableViewHeaderFooterItem

// The text that represents the section's headline.
@property(nonatomic, readwrite, strong) NSString* text;

// The list of URLs used to open when text with a link attribute is tapped.
// Asserts that the number of urls given corresponds to the link attributes in
// the text. These urls are only relevant to the `subtitle` since the only the
// `subtitle` can contain links.
@property(nonatomic, strong) NSArray<CrURL*>* URLs;

// The subtitle text string.
@property(nonatomic, copy) NSString* subtitle;

@end

// TableViewTextHeaderFooterView is a clone of the
// TableViewLinkHeaderFooterView with a UILabel, representing the header's
// title, above the link text.
@interface TableViewTextHeaderFooterView : UITableViewHeaderFooterView

// The UILabel containing the text stored in `text`.
//@property(nonatomic, readonly, strong) UILabel* textLabel;

// UITextView corresponding to `subtitle` from the item.
@property(nonatomic, readonly, strong) UITextView* subtitleLabel;

// Delegate to notify when the link is tapped.
@property(nonatomic, weak) id<TableViewTextHeaderFooterItemDelegate> delegate;

// The URLs to open when text with a link attribute is tapped.
@property(nonatomic, strong) NSArray<CrURL*>* URLs;

// Sets the `text` displayed by this cell. If the `text` contains a link, the
// link is appropriately colored.
- (void)setSubtitle:(NSString*)subtitle;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_HEADER_FOOTER_ITEM_H_
