// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_WEB_PREFERENCES_IMAGE_ANIMATION_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_WEB_PREFERENCES_IMAGE_ANIMATION_POLICY_H_

namespace blink {
namespace web_pref {

// ImageAnimationPolicy is used for controlling image animation
// when image frame is rendered for animation
enum ImageAnimationPolicy {
  // Animate the image (the default).
  kImageAnimationPolicyAllowed,
  // Animate image just once.
  kImageAnimationPolicyAnimateOnce,
  // Show the first frame and do not animate.
  kImageAnimationPolicyNoAnimation
};

}  // namespace web_pref
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_WEB_PREFERENCES_IMAGE_ANIMATION_POLICY_H_
