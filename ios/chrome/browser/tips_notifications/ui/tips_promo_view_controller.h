// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_UI_TIPS_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_UI_TIPS_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// A PromoStyleViewController subclass which displays a Lottie animation above
// a title and subtitle.
@interface TipsPromoViewController : PromoStyleViewController

// The name of the animation resource to be used in light mode.
@property(nonatomic, copy) NSString* animationName;

// The name of the animation resource to be used in dark mode.
@property(nonatomic, copy) NSString* animationNameDarkMode;

// A dictionary that allows l18n of text within the animations.
@property(nonatomic, copy)
    NSDictionary<NSString*, NSString*>* animationTextProvider;

@end

#endif  // IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_UI_TIPS_PROMO_VIEW_CONTROLLER_H_
