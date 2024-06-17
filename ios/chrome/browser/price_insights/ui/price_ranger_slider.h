// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_RANGER_SLIDER_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_RANGER_SLIDER_H_

#import <UIKit/UIKit.h>

// A custom UISlider designed for displaying and adjusting price ranges.
@interface PriceRangeSlider : UISlider

@end

// A UIStackView including labels, a UISlider, and horizontal line with rounded
// corners.
@interface PriceRangeSliderView : UIStackView

- (instancetype)initWithMinimumLabelText:(NSString*)minimumLabelText
                        maximumLabelText:(NSString*)maximumLabelText
                            minimumValue:(int64_t)minimumValue
                            maximumValue:(int64_t)maximumValue
                            currentValue:(int64_t)currentValue
                         sliderViewWidth:(CGFloat)sliderViewWidth
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_RANGER_SLIDER_H_
