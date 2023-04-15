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

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_IMAGE_IMAGE_UTIL_H_
