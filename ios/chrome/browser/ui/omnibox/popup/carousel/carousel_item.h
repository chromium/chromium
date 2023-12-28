// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CAROUSEL_CAROUSEL_ITEM_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CAROUSEL_CAROUSEL_ITEM_H_

#import <UIKit/UIKit.h>

@class CarouselItem;
@class CrURL;
@class FaviconAttributes;

/// An abstract consumer of carousel items.
@protocol CarouselItemConsumer <NSObject>

/// Removes `carouselItem`.
- (void)deleteCarouselItem:(CarouselItem*)carouselItem;

@end

/// Represent an carousel item in UI.
@interface CarouselItem : NSObject

/// Title of the suggestion.
@property(nonatomic, strong) NSString* title;
/// URL of the suggestion.
@property(nonatomic, strong) CrURL* URL;
/// Attributes used to display a `FaviconView` in the item.
@property(nonatomic, strong) FaviconAttributes* faviconAttributes;
/// IndexPath of the item in the omnibox popup.
@property(nonatomic, strong) NSIndexPath* indexPath;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CAROUSEL_CAROUSEL_ITEM_H_
