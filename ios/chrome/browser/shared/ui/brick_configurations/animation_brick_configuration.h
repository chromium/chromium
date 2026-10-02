// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_BRICK_CONFIGURATIONS_ANIMATION_BRICK_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_BRICK_CONFIGURATIONS_ANIMATION_BRICK_CONFIGURATION_H_

#import <UIKit/UIKit.h>

// Configuration object for the `AnimationBrick` brick.
@interface AnimationBrickConfiguration : NSObject

// The name of the animation resource to be used in light mode.
@property(nonatomic, copy) NSString* animationName;

// The name of the animation resource to be used in dark mode.
@property(nonatomic, copy) NSString* darkAnimationName;

// A dictionary that allows localization of text within the animations.
@property(nonatomic, copy)
    NSDictionary<NSString*, NSString*>* animationTextProvider;

// (Optional) The background color of the containing animation view.
@property(nonatomic, strong) UIColor* animationBackgroundColor;

// A boolean to indicate if the view controller should use the legacy mode for
// dark mode (i.e. finding a json ending with _darkmode), YES by default.
@property(nonatomic, assign) BOOL useLegacyDarkMode;

// A dictionary that associate a keypath with a color, for the light/dark mode.
// `useLegacyDarkMode` should be NO for this to be taken into account.
@property(nonatomic, copy)
    NSDictionary<NSString*, UIColor*>* lightModeColorProvider;
@property(nonatomic, copy)
    NSDictionary<NSString*, UIColor*>* darkModeColorProvider;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_BRICK_CONFIGURATIONS_ANIMATION_BRICK_CONFIGURATION_H_
