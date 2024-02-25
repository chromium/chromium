// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_MULTI_AVATAR_IMAGE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_MULTI_AVATAR_IMAGE_UTIL_H_

#import <UIKit/UIKit.h>

// Creates a multi-avatar image from `images` based on the amount of them:
// * For 0 the default avatar is returned.
// * For 1 the only image is fully displayed.
// * For 2 images are split between the left and the right half.
// * For 3 one image is on the left half, other two are split horizontally on
//   the right half.
// * For 4 each image takes a quarter.
// * For 5 and more handling is as in the previous point, but the bottom-right
//   quarter displays how many more images are apart from the ones displayed.
UIImage* CreateMultiAvatarImage(NSArray<UIImage*>* images, CGFloat size);

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_MULTI_AVATAR_IMAGE_UTIL_H_
