// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_BLOCKED_NAVIGATION_TYPES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_BLOCKED_NAVIGATION_TYPES_H_

namespace blink {

// The reason the navigation was blocked.
enum class NavigationBlockedReason {
  // Frame attempted to navigate the top window without user activation.
  kRedirectWithNoUserGesture,
  // Frame attempted to navigate the top window without user activation and is
  // sandboxed.
  kRedirectWithNoUserGestureSandbox,
  kMaxValue = kRedirectWithNoUserGestureSandbox,
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_BLOCKED_NAVIGATION_TYPES_H_
