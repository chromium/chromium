// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_LINK_HEADER_FOOTER_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_LINK_HEADER_FOOTER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

@class CrURL;
@class TableViewLinkHeaderFooterView;

@protocol TableViewLinkHeaderFooterItemDelegate <NSObject>

// Notifies the delegate that the link corresponding to `URL` was tapped in
// `view`.
- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL;

@end

// TableViewLinkHeaderFooterItem is the model class corresponding to
// TableViewLinkHeaderFooterView.
@interface TableViewLinkHeaderFooterItem : TableViewHeaderFooterItem

// The list of URLs used to open when a text with a link attribute is tapped.
// Asserts that the number of urls given corresponds to the link attributes in
// the text.
@property(nonatomic, strong) NSArray<CrURL*>* urls;

// The main text string.
@property(nonatomic, copy) NSString* text;

// UIColor for the text displayed in text view. Default is
// [UIColor colorNamed:kTextSecondaryColor].
@property(nonatomic, strong) UIColor* textColor;

// If true, aligns the header/footer with the section items by ensure that there
// are leading/trailing margins. By default, this is false.
@property(nonatomic, assign) BOOL forceIndents;

@end

// UITableViewHeaderFooterView subclass containing a single UITextView. The text
// view is laid to fill the full width of the cell and it is wrapped as needed
// to fit in the cell. If it contains a link, the link is correctly displayed as
// link and the delegate is notified if it is tapped.
@interface TableViewLinkHeaderFooterView : UITableViewHeaderFooterView

// Delegate to notify when the link is tapped.
@property(nonatomic, weak) id<TableViewLinkHeaderFooterItemDelegate> delegate;

// The URLs to open when text with a link attribute is tapped.
@property(nonatomic, strong) NSArray<CrURL*>* urls;

// Sets the `text` displayed by this cell. If the `text` contains a link, the
// link is appropriately colored. The text is coolored with `color`. If specific
// `textAlignment` is provided, apply that as well, otherwise it is
// NSTextAlignmentNatural.
- (void)setText:(NSString*)text withColor:(UIColor*)color;
- (void)setText:(NSString*)text
        withColor:(UIColor*)color
    textAlignment:(NSTextAlignment)textAlignment;

// If forceIndents is YES, activates the constraint to align the header/footer
// with section items.
- (void)setForceIndents:(BOOL)forceIndents;

// Updates the status of the link based on whether the link should be `enabled`.
- (void)setLinkEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_LINK_HEADER_FOOTER_ITEM_H_
