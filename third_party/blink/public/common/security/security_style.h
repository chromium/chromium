// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SECURITY_SECURITY_STYLE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SECURITY_SECURITY_STYLE_H_
namespace blink {
// This enum represents the security state of a resource.
enum class SecurityStyle {
  kUnknown,
  kNeutral,
  kInsecure,
  kSecure,
  kInsecureBroken,
  kLast = kInsecureBroken
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SECURITY_SECURITY_STYLE_H_"
