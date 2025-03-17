// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_IMAGE_SOURCE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_IMAGE_SOURCE_H_

#import <UIKit/UIKit.h>

@protocol LensImageMetadata;

// Model for the source of a Lens image.
@interface LensImageSource : NSObject

// The snapshot image to use as base for Lens. Otherwise `nil`.
@property(nonatomic, readonly) UIImage* snapshot;

// The metadata object to use as base for Lens or `nil` if absent.
@property(nonatomic, readonly) id<LensImageMetadata> imageMetadata;

// Whether the image source is valid.
@property(nonatomic, readonly) BOOL isValid;

// Creates a new image source from an image.
- (instancetype)initWithSnapshot:(UIImage*)snapshot;

// Creates a new image source from an image metadata.
- (instancetype)initWithImageMetadata:(id<LensImageMetadata>)imageMetadata;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_IMAGE_SOURCE_H_
