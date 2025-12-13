// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_UI_TIPS_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_UI_TIPS_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

// A ButtonStackViewController subclass which displays a Lottie animation above
// a title and subtitle.
// The `actionDelegate` property from the superclass should be used to handle
// button actions.
@interface TipsPromoViewController : ButtonStackViewController

// The title of the promo.
@property(nonatomic, copy) NSString* titleText;
// The subtitle of the promo.
@property(nonatomic, copy) NSString* subtitleText;

// The name of the animation resource to be used in light mode.
@property(nonatomic, copy) NSString* animationName;

// The name of the animation resource to be used in dark mode. Must only be used
// if the color providers are nil.
@property(nonatomic, copy) NSString* animationNameDarkMode;

// A dictionary that allows l18n of text within the animations.
@property(nonatomic, copy)
    NSDictionary<NSString*, NSString*>* animationTextProvider;

// A dictionary that associate a keypath with a color, for the light/dark mode.
@property(nonatomic, copy)
    NSDictionary<NSString*, UIColor*>* lightModeColorProvider;
@property(nonatomic, copy)
    NSDictionary<NSString*, UIColor*>* darkModeColorProvider;

@end

#endif  // IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_UI_TIPS_PROMO_VIEW_CONTROLLER_H_
