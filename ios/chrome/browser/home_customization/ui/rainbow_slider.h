// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_RAINBOW_SLIDER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_RAINBOW_SLIDER_H_

#import <UIKit/UIKit.h>

// A custom UISlider subclass that displays a rainbow gradient for selecting hue
// values.
@interface RainbowSlider : UISlider

// Sets the slider's value based on the hue of the provided color.
- (void)setColor:(UIColor*)color;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_RAINBOW_SLIDER_H_
