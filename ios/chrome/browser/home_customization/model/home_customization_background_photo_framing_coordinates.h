// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_COORDINATES_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_COORDINATES_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#import "base/values.h"

struct FramingCoordinates;

// Represents the framing coordinates for the home customization background
// image.
@interface HomeCustomizationFramingCoordinates : NSObject <NSCopying>

// Rectangle representing the visible area in the original image.
// This defines which pixels from the original image should be displayed.
@property(nonatomic, assign) CGRect visibleRect;

// Designated initializer.
- (instancetype)initWithVisibleRect:(CGRect)visibleRect
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Methods for serialization.
- (base::Value::Dict)toValue;
+ (instancetype)fromValue:(const base::Value::Dict&)dict;

// C++ framing coordinates conversion methods.
+ (instancetype)fromFramingCoordinates:(const FramingCoordinates&)coordinates;
- (FramingCoordinates)toFramingCoordinates;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_COORDINATES_H_
