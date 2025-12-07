// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_UI_PRICE_TRACKING_PROMO_MODULE_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_UI_PRICE_TRACKING_PROMO_MODULE_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/ui/price_tracking_promo_favicon_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_updating.h"

@protocol PriceTrackingPromoCommands;
@class PriceTrackingPromoItem;

// View for the Price Tracking Promo module.
@interface PriceTrackingPromoModuleView
    : UIView <NewTabPageColorUpdating, PriceTrackingPromoFaviconConsumer>

// Configures this view with `config`.
- (void)configureView:(PriceTrackingPromoItem*)config;

// Command handler for user events.
@property(nonatomic, weak) id<PriceTrackingPromoCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_UI_PRICE_TRACKING_PROMO_MODULE_VIEW_H_
