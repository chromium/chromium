// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_FIRST_RUN_SC_FIRST_RUN_HERO_SCREEN_VIEW_CONTROLLER_H_
#define IOS_SHOWCASE_FIRST_RUN_SC_FIRST_RUN_HERO_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

// Extends the base delegate protocol to handle taps on the custom button.
@protocol HeroScreenDelegate <PromoStyleViewControllerDelegate>

// Invoked when the custom action button is tapped.
- (void)didTapCustomActionButton;

@end

// A view controller to showcase an example hero screen for the first run
// experience.
@interface SCFirstRunHeroScreenViewController : PromoStyleViewController

@property(nonatomic, weak) id<HeroScreenDelegate> delegate;

@end

#endif  // IOS_SHOWCASE_FIRST_RUN_SC_FIRST_RUN_HERO_SCREEN_VIEW_CONTROLLER_H_
