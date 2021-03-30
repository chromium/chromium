// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_SANDBOX_FLAGS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_SANDBOX_FLAGS_H_

#include <bitset>
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-shared.h"

namespace blink {
namespace mojom {

inline constexpr WebSandboxFlags operator&(WebSandboxFlags a,
                                           WebSandboxFlags b) {
  return static_cast<WebSandboxFlags>(static_cast<int>(a) &
                                      static_cast<int>(b));
}

inline constexpr WebSandboxFlags operator|(WebSandboxFlags a,
                                           WebSandboxFlags b) {
  return static_cast<WebSandboxFlags>(static_cast<int>(a) |
                                      static_cast<int>(b));
}

inline WebSandboxFlags& operator|=(WebSandboxFlags& a, WebSandboxFlags b) {
  return a = a | b;
}

inline constexpr WebSandboxFlags operator~(WebSandboxFlags flags) {
  return static_cast<WebSandboxFlags>(~static_cast<int>(flags));
}

}  // namespace mojom
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_SANDBOX_FLAGS_H_
