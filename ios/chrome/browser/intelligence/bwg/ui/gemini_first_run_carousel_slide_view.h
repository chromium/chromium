// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_CAROUSEL_SLIDE_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_CAROUSEL_SLIDE_VIEW_H_

#import <UIKit/UIKit.h>

// Represents a single slide in the Gemini FRE carousel.
@interface GeminiFirstRunCarouselSlide : NSObject

// The name of the Lottie animation resource file for light mode (without .json
// extension).
@property(nonatomic, copy, readonly) NSString* animationName;

// The name of the Lottie animation resource file for dark mode (without .json
// extension).
@property(nonatomic, copy, readonly) NSString* darkAnimationName;

// The name of the Lottie animation resource file for light mode RTL (without
// .json extension).
@property(nonatomic, copy, readonly) NSString* animationNameRTL;

// The name of the Lottie animation resource file for dark mode RTL (without
// .json extension).
@property(nonatomic, copy, readonly) NSString* darkAnimationNameRTL;

// The title text displayed below the animation.
@property(nonatomic, copy, readonly) NSString* title;

// The accessibility label for the Lottie animation artwork.
@property(nonatomic, copy, readonly) NSString* animationAccessibilityLabel;

// Initializer with the Lottie animations for light and dark mode, in both LTR
// and RTL layout directions, along with the title and accessibility label for
// the animation artwork.
- (instancetype)initWithAnimationName:(NSString*)animationName
                    darkAnimationName:(NSString*)darkAnimationName
                     animationNameRTL:(NSString*)animationNameRTL
                 darkAnimationNameRTL:(NSString*)darkAnimationNameRTL
                                title:(NSString*)title
          animationAccessibilityLabel:(NSString*)animationAccessibilityLabel
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

// View representing an individual slide inside the Gemini FRE carousel.
// Manages light and dark Lottie animations, branded gradient title styling,
// Dynamic Type text scaling, and landscape/compact height collapsing.
@interface GeminiFirstRunCarouselSlideView : UIView

// Initializes the slide view with `slide` model.
- (instancetype)initWithSlide:(GeminiFirstRunCarouselSlide*)slide
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Plays the Lottie animation for the current user interface style.
- (void)playAnimation;

// Stops the Lottie animation.
- (void)stopAnimation;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_CAROUSEL_SLIDE_VIEW_H_
