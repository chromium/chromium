// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_CONSUMER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer protocol for the ephemeral theme promo UI.
@protocol HomeCustomizationEphemeralThemePromoConsumer <NSObject>

// Sets the Lottie animation asset name and optional bundle to be loaded.
// If `bundle` is nil, the default framework bundle is used.
- (void)setAnimationAssetName:(NSString*)animationAssetName
                       bundle:(NSBundle*)bundle;

// Sets the light and dark mode dynamic color providers for the animation.
- (void)setLightModeColorProvider:
            (NSDictionary<NSString*, UIColor*>*)lightModeColorProvider
            darkModeColorProvider:
                (NSDictionary<NSString*, UIColor*>*)darkModeColorProvider;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_CONSUMER_H_
