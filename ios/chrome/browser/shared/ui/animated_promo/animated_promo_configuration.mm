// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_configuration.h"

#import "ios/chrome/browser/shared/ui/brick_configurations/animation_brick_configuration.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/title_brick_configuration.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"

@implementation AnimatedPromoConfiguration

- (instancetype)initWithAnimationBrickConfiguration:
                    (AnimationBrickConfiguration*)animationBrickConfiguration
                           buttonStackConfiguration:(ButtonStackConfiguration*)
                                                        buttonStackConfiguration
                            titleBrickConfiguration:(TitleBrickConfiguration*)
                                                        titleBrickConfiguration
                                     underTitleView:(UIView*)underTitleView {
  self = [super init];
  if (self) {
    _animationBrickConfiguration = animationBrickConfiguration;
    _buttonStackConfiguration = buttonStackConfiguration;
    _titleBrickConfiguration = titleBrickConfiguration;
    _underTitleView = underTitleView;
  }
  return self;
}

@end
