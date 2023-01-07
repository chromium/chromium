// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_CAROUSEL_CONTROL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_CAROUSEL_CONTROL_H_

#import <UIKit/UIKit.h>

@protocol AutocompleteSuggestion;
@class CarouselItem;
@protocol CarouselItemMenuProvider;

// View inside the OmniboxCarouselCell that displays the icon and text of
// `CarouselItem`.
@interface OmniboxPopupCarouselControl
    : UIControl <UIContextMenuInteractionDelegate>

// Updates the View with `item`'s icon and text.
- (void)setupWithCarouselItem:(CarouselItem*)carouselItem;

// Underlying CarouselItem.
- (CarouselItem*)carouselItem;

// Context menu provider for the carousel items.
@property(nonatomic, weak) id<CarouselItemMenuProvider> menuProvider;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_CAROUSEL_CONTROL_H_
