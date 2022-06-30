// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_IMAGE_UTIL_IMAGE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_IMAGE_UTIL_IMAGE_UTIL_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

namespace gfx {
class Image;
}

@class UIColor;
@class UIImage;

// Returns the dominant color for `image`.
UIColor* DominantColorForImage(const gfx::Image& image, CGFloat opacity);

// Returns a copy of `image` configured to stretch at the given offsets.
UIImage* StretchableImageFromUIImage(UIImage* image,
                                     NSInteger left_cap_width,
                                     NSInteger top_cap_height);

// Returns the image named `name`, configured to stretch at the center pixel.
UIImage* StretchableImageNamed(NSString* name);

// Returns the image named `name`, configured to stretch at the given offsets.
UIImage* StretchableImageNamed(NSString* name,
                               NSInteger left_cap_width,
                               NSInteger top_cap_height);

// Returns the extension by checking the first byte of image `data`. If `data`
// is nil, empty, or cannot be recognized, nil will be returned.
NSString* GetImageExtensionFromData(NSData* data);

// Returns the UTI by checking the first byte of image `data`. If `data`
// is nil, empty, or cannot be recognized, nil will be returned.
NSString* GetImageUTIFromData(NSData* data);

#endif  // IOS_CHROME_BROWSER_UI_IMAGE_UTIL_IMAGE_UTIL_H_
