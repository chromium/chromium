// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_URL_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_URL_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@class CrURL;
@class FaviconView;
@class TableViewURLCellFaviconBadgeView;

// TableViewURLItem contains the model data for a TableViewURLCell.
@interface TableViewURLItem : TableViewItem

// The title of the page at `URL`.
@property(nonatomic, readwrite, copy) NSString* title;
// CrURL from which the cell will retrieve a favicon and display the host name.
@property(nonatomic, readwrite, strong) CrURL* URL;
// Supplemental text used to describe the URL.
@property(nonatomic, readwrite, copy) NSString* supplementalURLText;
// Delimiter used to separate the URL hostname and the supplemental text.
@property(nonatomic, readwrite, copy) NSString* supplementalURLTextDelimiter;
// Custom third row text. This is not shown if it is empty or if the second row
// is empty.
@property(nonatomic, readwrite, copy) NSString* thirdRowText;
// Third row text color, if it is shown. If nil, ChromeTableViewStyler's
// `detailTextColor` is used, otherwise a default color is used.
@property(nonatomic, strong) UIColor* thirdRowTextColor;
// Detail text to be displayed instead of the URL.
@property(nonatomic, strong) NSString* detailText;
// Metadata text displayed at the trailing edge of the cell.
@property(nonatomic, readwrite, copy) NSString* metadata;
// Metadata image displayed at the trailing edge of the cell, before the
// metadata text if there's any.
@property(nonatomic, readwrite, copy) UIImage* metadataImage;
// Tint color for metadata image.
@property(nonatomic, readwrite, copy) UIColor* metadataImageColor;
// The image for the badge view added over the favicon.
@property(nonatomic, readwrite, strong) UIImage* badgeImage;
// Identifier to match a URLItem with its URLCell.
@property(nonatomic, readonly) NSString* uniqueIdentifier;

@end

// TableViewURLCell is used in Bookmarks, Reading List, and Recent Tabs.  It
// contains a favicon, a title, a URL, and optionally some metadata such as a
// timestamp or a file size. After configuring the cell, make sure to call
// configureUILayout:.
@interface TableViewURLCell : TableViewCell

// The imageview that is displayed on the leading edge of the cell.  This
// contains a favicon composited on top of an off-white background.
@property(nonatomic, readonly, strong) FaviconView* faviconView;

// The image view used to display the favicon badge.
@property(nonatomic, readonly, strong)
    TableViewURLCellFaviconBadgeView* faviconBadgeView;

// The cell title.
@property(nonatomic, readonly, strong) UILabel* titleLabel;

// The host URL associated with this cell.
@property(nonatomic, readonly, strong) UILabel* URLLabel;

// Optional metadata that is displayed at the trailing edge of the cell.
@property(nonatomic, readonly, strong) UILabel* metadataLabel;

// Optional metadata image that is displayed at the trailing edge of the cell.
@property(nonatomic, readonly, strong) UIImageView* metadataImage;

// Optional third row label. This is never used in place of the second row of
// text.
@property(nonatomic, readonly, strong) UILabel* thirdRowLabel;

// Unique identifier that matches with one URLItem.
@property(nonatomic, strong) NSString* cellUniqueIdentifier;

// Properly configure the subview layouts once all labels' properties have been
// configured. This must be called at the end of configureCell: for all items
// that use TableViewURLCell.
- (void)configureUILayout;

// Starts the animation of the activity indicator replacing the favicon. NO-OP
// if it is already running.
- (void)startAnimatingActivityIndicator;

// Stops the animation of the activity indicator and puts favicon back in place.
// NO-OP if it is already stopped.
- (void)stopAnimatingActivityIndicator;

// Sets the background color for the favicon container view.
- (void)setFaviconContainerBackgroundColor:(UIColor*)backgroundColor;

// Sets the border color for the favicon container view.
- (void)setFaviconContainerBorderColor:(UIColor*)borderColor;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_URL_ITEM_H_
