// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ANIMATED_PROMO_ANIMATED_PROMO_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ANIMATED_PROMO_ANIMATED_PROMO_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@class AnimationBrickConfiguration;
@class ButtonStackConfiguration;
@class TitleBrickConfiguration;

// Configuration object for the `AnimatedPromoViewController`.
@interface AnimatedPromoConfiguration : NSObject

// The configuration for the animated view.
@property(nonatomic, readonly)
    AnimationBrickConfiguration* animationBrickConfiguration;
// The configuration for the button stack view.
@property(nonatomic, readonly)
    ButtonStackConfiguration* buttonStackConfiguration;
// The configuration for the title view.
@property(nonatomic, readonly) TitleBrickConfiguration* titleBrickConfiguration;
// The view displayed under titles and subtitles. Nil if not needed.
@property(nonatomic, strong, readonly) UIView* underTitleView;

- (instancetype)initWithAnimationBrickConfiguration:
                    (AnimationBrickConfiguration*)animationBrickConfiguration
                           buttonStackConfiguration:(ButtonStackConfiguration*)
                                                        buttonStackConfiguration
                            titleBrickConfiguration:(TitleBrickConfiguration*)
                                                        titleBrickConfiguration
                                     underTitleView:(UIView*)underTitleView
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ANIMATED_PROMO_ANIMATED_PROMO_CONFIGURATION_H_
