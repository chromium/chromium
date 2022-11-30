// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_CAROUSEL_CELL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_CAROUSEL_CELL_H_

#import <UIKit/UIKit.h>

@protocol AutocompleteSuggestionGroup;
@class CarouselItem;
@protocol CarouselItemMenuProvider;

namespace {

NSString* OmniboxPopupCarouselCellReuseIdentifier = @"OmniboxPopupCarouselCell";

}  // namespace

// Delegate for actions happening in OmniboxPopupCarouselCell.
@protocol OmniboxPopupCarouselCellDelegate <NSObject>

// User tapped on `carouselItem`.
- (void)didTapCarouselItem:(CarouselItem*)carouselItem;

@end

// Cell used in omnibox popup table view to display suggestions in a carousel
// (horizontal scrolling list).
@interface OmniboxPopupCarouselCell : UITableViewCell

// Fill the carousel with `carouselItems`.
- (void)setupWithCarouselItems:(NSArray<CarouselItem*>*)carouselItems;

// Update the UI of `carouselItem` if it still exist.
- (void)updateCarouselItem:(CarouselItem*)carouselItem;

@property(nonatomic, weak) id<OmniboxPopupCarouselCellDelegate> delegate;
// Context menu provider for the carousel items.
@property(nonatomic, weak) id<CarouselItemMenuProvider> menuProvider;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_CAROUSEL_CELL_H_
