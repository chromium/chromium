// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_ROW_CELL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_ROW_CELL_H_

#import <UIKit/UIKit.h>

@protocol AutocompleteSuggestion;
@protocol FaviconRetriever;
@protocol ImageRetriever;
@class OmniboxIconView;
@class OmniboxPopupRowCell;

namespace {
NSString* OmniboxPopupRowCellReuseIdentifier = @"OmniboxPopupRowCell";
// This mimimum height causes most of the rows to be the same height. Some have
// multiline answers, so those heights may be taller than this minimum.
const CGFloat kOmniboxPopupCellMinimumHeight = 58;
}  // namespace

// Protocol for informing delegate that the trailing button for this cell
// was tapped
@protocol OmniboxPopupRowCellDelegate

// The trailing button was tapped.
- (void)trailingButtonTappedForCell:(OmniboxPopupRowCell*)cell;

@end

// Table view cell to display an autocomplete suggestion in the omnibox popup.
// It handles all the layout logic internally.
@interface OmniboxPopupRowCell : UITableViewCell

@property(nonatomic, weak) id<OmniboxPopupRowCellDelegate> delegate;
// Used to fetch favicons.
@property(nonatomic, weak) id<FaviconRetriever> faviconRetriever;

@property(nonatomic, weak) id<ImageRetriever> imageRetriever;

// The semanticContentAttribute determined by the text in the omnibox. The
// views in this cell should be updated to match this.
@property(nonatomic, assign)
    UISemanticContentAttribute omniboxSemanticContentAttribute;

// Whether the table row separator appears. This can't use the default
// UITableView separators because the leading edge of the separator must be
// aligned with the text, which is positioned using a layout guide, so the
// tableView's separatorInsets can't be calculated.
@property(nonatomic, assign) BOOL showsSeparator;

// Image view for the leading image.
@property(nonatomic, strong, readonly) OmniboxIconView* leadingIconView;

// Layout this cell with the given data before displaying.
- (void)setupWithAutocompleteSuggestion:(id<AutocompleteSuggestion>)suggestion
                              incognito:(BOOL)incognito;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_ROW_CELL_H_
