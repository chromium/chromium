// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_PUBLIC_PICTURE_IN_PICTURE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_PUBLIC_PICTURE_IN_PICTURE_CONFIGURATION_H_

#import "ios/chrome/browser/picture_in_picture/public/picture_in_picture_constants.h"

// Configuration object for picture in picture.
@interface PictureInPictureConfiguration : NSObject

// The feature for which the configuration is created.
@property(nonatomic, assign) PictureInPictureFeature feature;

// The URL of the video to play in picture in picture.
@property(nonatomic, copy) NSURL* videoURL;

// The title of the video to display in picture in picture.
@property(nonatomic, copy) NSString* title;

// The primary button title of the video to display in picture in picture.
@property(nonatomic, copy) NSString* primaryButtonTitle;

@end

#endif  // IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_PUBLIC_PICTURE_IN_PICTURE_CONFIGURATION_H_
