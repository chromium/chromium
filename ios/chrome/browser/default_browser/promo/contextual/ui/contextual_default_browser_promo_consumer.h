// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_UI_CONTEXTUAL_DEFAULT_BROWSER_PROMO_CONSUMER_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_UI_CONTEXTUAL_DEFAULT_BROWSER_PROMO_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer for the contextual default browser promo UI.
@protocol ContextualDefaultBrowserPromoConsumer <NSObject>

// Sets the title text of the promo.
- (void)setPromoTitle:(NSString*)promoTitle;

// Sets the subtitle / description text of the promo.
- (void)setPromoSubtitle:(NSString*)promoSubtitle;

// Sets the animation asset name.
- (void)setAnimationAssetName:(NSString*)animationAssetName;

// Sets the light and dark mode color provider dictionaries for the Lottie
// animation.
- (void)setLightModeColorProvider:
            (NSDictionary<NSString*, UIColor*>*)lightModeColorProvider
            darkModeColorProvider:
                (NSDictionary<NSString*, UIColor*>*)darkModeColorProvider;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_UI_CONTEXTUAL_DEFAULT_BROWSER_PROMO_CONSUMER_H_
