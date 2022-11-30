// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

UIImage* GetBrandedImage(BrandedImage branded_image) {
  // No image in tests.
  return nil;
}

}  // namespace provider
}  // namespace ios
