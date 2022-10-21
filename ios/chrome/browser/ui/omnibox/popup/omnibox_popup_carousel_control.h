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

// Context menu provider for the carousel items.
@property(nonatomic, weak) id<CarouselItemMenuProvider> menuProvider;
@property(nonatomic, strong) CarouselItem* carouselItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_CAROUSEL_CONTROL_H_
