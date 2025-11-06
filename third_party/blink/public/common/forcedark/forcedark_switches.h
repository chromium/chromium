// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FORCEDARK_FORCEDARK_SWITCHES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FORCEDARK_FORCEDARK_SWITCHES_H_

namespace blink {

// Specifies algorithm for modifying how colors are rendered in Force Dark.
enum class ForceDarkInversionMethod {
  // Use the value provided via command line with --blink_settings or, if that
  // flag is absent, get the value from the defaults in
  // renderer/core/frame/settings.json5.
  kUseBlinkSettings,

  // Modify colors by converting them to the HSL color space and inverting the
  // lightness (i.e. the "L" in HSL).
  kHslBased,

  // Modify colors by converting them to CIE L*a*b color space and inverting the
  // L value.
  kCielabBased
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FORCEDARK_FORCEDARK_SWITCHES_H_
