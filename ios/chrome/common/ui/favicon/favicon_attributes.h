// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_FAVICON_FAVICON_ATTRIBUTES_H_
#define IOS_CHROME_COMMON_UI_FAVICON_FAVICON_ATTRIBUTES_H_

#import <UIKit/UIKit.h>

// Attributes of a favicon. A favicon is represented either with an image or
// with a fallback monogram of a given color and background color.
@interface FaviconAttributes : NSObject <NSCoding>

// Favicon image. Can be nil. If it is nil, monogram string and color are
// guaranteed to be not nil.
@property(nonatomic, readonly, strong, nullable) UIImage* faviconImage;
// Favicon monogram. Only available when there is no image.
@property(nonatomic, readonly, copy, nullable) NSString* monogramString;
// Favicon monogram color. Only available when there is no image.
@property(nonatomic, readonly, strong, nullable) UIColor* textColor;
// Favicon monogram background color. Only available when there is no image.
@property(nonatomic, readonly, strong, nullable) UIColor* backgroundColor;
// Whether the background color is the default one.Only available when there is
// no image.
@property(nonatomic, readonly, assign, getter=isDefaultBackgroundColor)
    BOOL defaultBackgroundColor;
// Whether the attributes are using the default image.
@property(nonatomic, readonly, assign) BOOL usesDefaultImage;

+ (nullable instancetype)attributesWithImage:(nonnull UIImage*)image;
+ (nullable instancetype)attributesWithMonogram:(nonnull NSString*)monogram
                                      textColor:(nonnull UIColor*)textColor
                                backgroundColor:
                                    (nonnull UIColor*)backgroundColor
                         defaultBackgroundColor:(BOOL)defaultBackgroundColor;

// Returns attributes with a placeholder favicon image and no monogram.
+ (nullable instancetype)attributesWithDefaultImage;

- (nullable instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_COMMON_UI_FAVICON_FAVICON_ATTRIBUTES_H_
