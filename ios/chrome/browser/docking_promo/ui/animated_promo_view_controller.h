// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOCKING_PROMO_UI_ANIMATED_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DOCKING_PROMO_UI_ANIMATED_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

// TODO(crbug.com/373869553): Explore a better codebase location for
// `AnimatedPromoViewController`.

// Container view controller for a full-screen, animated promo.
@interface AnimatedPromoViewController : UIViewController

// The action handler for interactions in this view controller.
@property(nonatomic, weak) id<ConfirmationAlertActionHandler> actionHandler;

// The headline below the image. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* titleString;

// The subtitle below the title. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* subtitleString;

// The view displayed under titles and subtitles. Nil if not needed.
// If needed, must be set before the view is loaded.
@property(nonatomic, strong) UIView* underTitleView;

// The text for the primary action. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* primaryActionString;

// The text for the secondary action. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* secondaryActionString;

// The name of the animation resource to be used in light mode. Must be set
// before the view is loaded.
@property(nonatomic, copy) NSString* animationName;

// The name of the animation resource to be used in dark mode. Must be set
// before the view is loaded.
@property(nonatomic, copy) NSString* animationNameDarkMode;

// (Optional) The background color of the containing animation view. If set,
// must be set before the view is loaded.
@property(nonatomic, copy) UIColor* animationBackgroundColor;

// A dictionary that allows localization of text within the animations.
@property(nonatomic, copy)
    NSDictionary<NSString*, NSString*>* animationTextProvider;

@end

#endif  // IOS_CHROME_BROWSER_DOCKING_PROMO_UI_ANIMATED_PROMO_VIEW_CONTROLLER_H_
