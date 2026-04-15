// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_IMAGE_IMAGE_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_IMAGE_IMAGE_UTIL_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

namespace gfx {
class Image;
}

@class UIColor;
@class UIImage;

// Returns the dominant color for `image`.
UIColor* DominantColorForImage(const gfx::Image& image, CGFloat opacity);

// Returns the image named `name`, configured to stretch at the center pixel.
// This image will no longer change dark/light mode dynamically, see
// crbug.com/1351094.
UIImage* StretchableImageNamed(NSString* name);

// Returns the extension by checking the first byte of image `data`. If `data`
// is nil, empty, or cannot be recognized, nil will be returned.
NSString* GetImageExtensionFromData(NSData* data);

// Returns the UTI by checking the first byte of image `data`. If `data`
// is nil, empty, or cannot be recognized, nil will be returned.
NSString* GetImageUTIFromData(NSData* data);

// Returns the pixel dimensions of the image in `data` without decoding
// the full image. Returns CGSizeZero if the data is nil or cannot be read.
CGSize ImageSizeFromData(NSData* data);

// Returns the pixel dimensions of the image at `fileURL` without decoding
// the full image. Returns CGSizeZero if the URL is nil or cannot be read.
CGSize ImageSizeFromURL(NSURL* fileURL);

// Decodes `data` into a UIImage downsampled to fit within `point_size` at
// the given display `scale`. Uses CGImageSource to decode only the needed
// pixels, significantly reducing memory usage compared to decoding the
// full image. Returns nil if the data is nil or cannot be decoded.
UIImage* DownsampledImageFromData(NSData* data,
                                  CGSize point_size,
                                  CGFloat scale);

// Decodes the image at `fileURL` into a UIImage downsampled to fit within
// `point_size` at the given display `scale`. Reads directly from disk
// without loading the entire file into memory first. Returns nil if the
// URL is nil or the image cannot be decoded.
UIImage* DownsampledImageFromURL(NSURL* fileURL,
                                 CGSize point_size,
                                 CGFloat scale);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_IMAGE_IMAGE_UTIL_H_
