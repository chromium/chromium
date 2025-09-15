// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_ANIMATION_CONTEXT_PROVIDER_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_ANIMATION_CONTEXT_PROVIDER_H_

#import <UIKit/UIKit.h>

// Provides the views needed for the custom dismissal animation.
@protocol AIMPrototypeAnimationContextProvider
// The main view of the view controller to be faded out.
@property(nonatomic, readonly) UIView* mainViewForAnimation;
// The input plate view to be animated downwards.
@property(nonatomic, readonly) UIView* inputPlateViewForAnimation;
// The text view to be focused after the presentation animation.
@property(nonatomic, readonly) UITextView* textViewForAnimation;

// Sets whether AI mode is enabled.
- (void)setAIModeEnabled:(BOOL)AIModeEnabled;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_ANIMATION_CONTEXT_PROVIDER_H_
