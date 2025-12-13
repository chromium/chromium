// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_FAVICON_FAVICON_ATTRIBUTES_H_
#define IOS_CHROME_COMMON_UI_FAVICON_FAVICON_ATTRIBUTES_H_

#import <UIKit/UIKit.h>

// The grayscale value of the default color for monogram string.
extern const CGFloat kFallbackIconDefaultTextColorGrayscale;

// Attributes of a favicon. A favicon is represented either with an image or
// with a fallback monogram of a given color and background color.
@interface FaviconAttributes : NSObject <NSCoding>

// Favicon image. Can be nil. If it is nil, monogram string and color are
// guaranteed to be not nil.
@property(nonatomic, readonly, strong) UIImage* faviconImage;
// Favicon monogram. Only available when there is no image.
@property(nonatomic, readonly, copy) NSString* monogramString;
// Favicon monogram color. Only available when there is no image.
@property(nonatomic, readonly, strong) UIColor* textColor;
// Favicon monogram background color. Only available when there is no image.
@property(nonatomic, readonly, strong) UIColor* backgroundColor;
// Whether the background color is the default one. Only available when there is
// no image.
@property(nonatomic, readonly, assign, getter=isDefaultBackgroundColor)
    BOOL defaultBackgroundColor;

+ (instancetype)attributesWithImage:(UIImage*)image;
+ (instancetype)attributesWithMonogram:(NSString*)monogram
                             textColor:(UIColor*)textColor
                       backgroundColor:(UIColor*)backgroundColor
                defaultBackgroundColor:(BOOL)defaultBackgroundColor;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_COMMON_UI_FAVICON_FAVICON_ATTRIBUTES_H_
