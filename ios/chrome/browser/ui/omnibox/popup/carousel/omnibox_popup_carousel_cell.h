// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CAROUSEL_OMNIBOX_POPUP_CAROUSEL_CELL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CAROUSEL_OMNIBOX_POPUP_CAROUSEL_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/omnibox_keyboard_delegate.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel/carousel_item.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel/omnibox_popup_carousel_control.h"

@class OmniboxPopupCarouselCell;
@protocol AutocompleteSuggestionGroup;
@protocol CarouselItemMenuProvider;

/// Delegate for actions happening in OmniboxPopupCarouselCell.
@protocol OmniboxPopupCarouselCellDelegate <NSObject>

/// User tapped on `carouselItem`.
- (void)carouselCell:(OmniboxPopupCarouselCell*)carouselCell
    didTapCarouselItem:(CarouselItem*)carouselItem;
/// Called when the cell has changed the number of items.
- (void)carouselCellDidChangeItemCount:(OmniboxPopupCarouselCell*)carouselCell;

@end

/// Cell used in omnibox popup table view to display suggestions in a carousel
/// (horizontal scrolling list).
@interface OmniboxPopupCarouselCell
    : UITableViewCell <CarouselItemConsumer,
                       OmniboxKeyboardDelegate,
                       OmniboxPopupCarouselControlDelegate>

/// Fill the carousel with `carouselItems`.
- (void)setupWithCarouselItems:(NSArray<CarouselItem*>*)carouselItems;

/// Update the UI of `carouselItem` if it still exist.
- (void)updateCarouselItem:(CarouselItem*)carouselItem;

@property(nonatomic, weak) id<OmniboxPopupCarouselCellDelegate> delegate;
/// Context menu provider for the carousel items.
@property(nonatomic, weak) id<CarouselItemMenuProvider> menuProvider;
/// Index of the highlighted index, or NSNotFound if no tile is highlighted.
/// The index is given from all the `carouselItems`, not just the ones that
/// aren't hidden.
@property(nonatomic, readonly, assign) NSInteger highlightedTileIndex;

/// Indicates the number of items in the carousel.
/// - Note: Usually this matches the `carouselItems` passed in
/// `setupWithCarouselItems:` method. However, the user may long-press and
/// "delete" a tile. The tile will then be deleted (by `deleteCarouselItem:`
/// call).
@property(nonatomic, readonly, assign) NSUInteger tileCount;
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CAROUSEL_OMNIBOX_POPUP_CAROUSEL_CELL_H_
