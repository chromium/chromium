// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_CAROUSEL_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_CAROUSEL_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_carousel_slide_view.h"

// Standalone horizontal paging carousel view for the Visual-Rich Gemini FRE.
// Displays slides with a Lottie animation, title, indicator dots via
// UIPageControl, and supports auto-scrolling with a 3-second recurring timer
// that pauses on user touch/drag gestures.
@interface GeminiFirstRunCarouselView : UIView

// Initializes the carousel view with an array of `slides`.
- (instancetype)initWithSlides:(NSArray<GeminiFirstRunCarouselSlide*>*)slides
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Starts playing the current slide animation and begins auto-scrolling.
- (void)startAutoScrolling;

// Stops auto-scrolling.
- (void)stopAutoScrolling;

// Re-centers the currently active slide inside the scroll view (e.g. after
// orientation or size transitions).
- (void)recenterActiveSlide;

// Prepares the carousel for an upcoming size or orientation transition by
// suppressing scroll delegate updates during intermediate bounds animations.
- (void)prepareForSizeTransition;

// Completes a size or orientation transition and recenters the active slide.
- (void)completeSizeTransition;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_CAROUSEL_VIEW_H_
