// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_IMAGE_UTIL_H_
#define IOS_CHROME_COMMON_UI_UTIL_IMAGE_UTIL_H_

#import <UIKit/UIKit.h>

/**
 Scale down the image if it's too large so it doesn't take too much space
 to store or too much data to upload.
 This is taken from Google Toolbox for Mac. Extensions should not have
 too many dependencies, so this is reproduced here. This should not be used
 outside of extensions. Instead, use the simpler version in
 //ui/gfx/image/image_util.h.
 */
UIImage* ResizeImageForSearchByImage(UIImage* image);

// Returns an image generated from the given |view|, using |backgroundColor| and
// adding |padding| around the centered image.
UIImage* ImageFromView(UIView* view,
                       UIColor* backgroundColor,
                       UIEdgeInsets padding);

#endif  // IOS_CHROME_COMMON_UI_UTIL_IMAGE_UTIL_H_
