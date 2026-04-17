// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_RAINBOW_SLIDER_TESTING_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_RAINBOW_SLIDER_TESTING_H_

#import "ios/chrome/browser/home_customization/ui/rainbow_slider.h"

// Testing category exposing private methods for unit tests.
@interface RainbowSlider (Testing)

// Sets the slider value and updates the thumb color for the given horizontal
// touch position.
- (void)updateValueForLocationX:(CGFloat)locationX;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_RAINBOW_SLIDER_TESTING_H_
