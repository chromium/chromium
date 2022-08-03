// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_HEADER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_HEADER_ITEM_H_

#import <MaterialComponents/MaterialCollectionCells.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"

// Item to display a view cell.
@interface ContentSuggestionsHeaderItem : CollectionViewItem

// The view to be displayed.
@property(nonatomic, strong) UIView* view;

// Accessibility identifier of the ContentSuggestionsHeaderCell.
+ (NSString*)accessibilityIdentifier;

@end

// Corresponding cell.
@interface ContentSuggestionsHeaderCell : MDCCollectionViewCell

// The header view to be displayed.
@property(nonatomic, strong) UIView* headerView;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_HEADER_ITEM_H_
