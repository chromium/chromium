// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CAROUSEL_OMNIBOX_POPUP_CAROUSEL_CONTROL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CAROUSEL_OMNIBOX_POPUP_CAROUSEL_CONTROL_H_

#import <UIKit/UIKit.h>

@protocol AutocompleteSuggestion;
@class CarouselItem;
@protocol CarouselItemMenuProvider;
@class OmniboxPopupCarouselControl;

/// Delegate for events happening in OmniboxPopupCarouselControl.
@protocol OmniboxPopupCarouselControlDelegate <NSObject>

/// Called when `control` becomes focused with accessibility or keyboard
/// selection.
- (void)carouselControlDidBecomeFocused:(OmniboxPopupCarouselControl*)control;

@end

/// Full width of a OmniboxPopupCarouselControl.
extern const CGFloat kOmniboxPopupCarouselControlWidth;

/// View inside the OmniboxCarouselCell that displays the icon and text of
/// `CarouselItem`.
@interface OmniboxPopupCarouselControl
    : UIControl <UIContextMenuInteractionDelegate>

/// Context menu provider for the carousel item.
@property(nonatomic, weak) id<CarouselItemMenuProvider> menuProvider;
/// Object containing data displayed by the control.
@property(nonatomic, strong) CarouselItem* carouselItem;
/// Delegate for events happening in the control.
@property(nonatomic, weak) id<OmniboxPopupCarouselControlDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CAROUSEL_OMNIBOX_POPUP_CAROUSEL_CONTROL_H_
