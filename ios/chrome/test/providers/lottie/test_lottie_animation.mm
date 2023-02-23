// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

// Generate an instance of LottieAnimation.
id<LottieAnimation> GenerateLottieAnimation(
    LottieAnimationConfiguration* config) {
  return nil;
}

}  // namespace provider
}  // namespace ios