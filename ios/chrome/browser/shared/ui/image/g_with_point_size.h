// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_IMAGE_G_WITH_POINT_SIZE_H_
#define IOS_CHROME_BROWSER_SHARED_UI_IMAGE_G_WITH_POINT_SIZE_H_

#import <UIKit/UIKit.h>

// This file is for the gradient Super G icon. For the monochrome icon, please
// use the one in `symbols`.

// Supported point sizes for the Super G icon.
enum class SuperGSize {
  k16pt,
  k18pt,
  k24pt,
};

// Returns an image configured with the given `size`.
UIImage* GWithPointSize(SuperGSize size);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_IMAGE_G_WITH_POINT_SIZE_H_
