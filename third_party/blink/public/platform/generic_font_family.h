// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_GENERIC_FONT_FAMILY_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_GENERIC_FONT_FAMILY_H_

namespace blink {

// Generic font families that the embedder configures per script, see
// GenericFontFamilySettings.
enum class GenericFontFamily {
  kStandard,
  kFixed,
  kSerif,
  kSansSerif,
  kCursive,
  kFantasy,
  kMath,
  kMaxValue = kMath,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_GENERIC_FONT_FAMILY_H_
