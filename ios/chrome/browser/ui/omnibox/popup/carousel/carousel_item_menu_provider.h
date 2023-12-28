// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CAROUSEL_CAROUSEL_ITEM_MENU_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CAROUSEL_CAROUSEL_ITEM_MENU_PROVIDER_H_

@class CarouselItem;

@protocol CarouselItemMenuProvider <NSObject>

/// Creates a context menu configuration instance for the given `carouselItem`,
/// which is represented on the UI by `view`.
- (UIContextMenuConfiguration*)
    contextMenuConfigurationForCarouselItem:(CarouselItem*)carouselItem
                                   fromView:(UIView*)view;

- (NSArray<UIAccessibilityCustomAction*>*)
    accessibilityActionsForCarouselItem:(CarouselItem*)carouselItem
                               fromView:(UIView*)view;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CAROUSEL_CAROUSEL_ITEM_MENU_PROVIDER_H_
