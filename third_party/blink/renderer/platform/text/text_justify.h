// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_JUSTIFY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_JUSTIFY_H_

#include <cstdint>

namespace blink {

// Represent a justification method.
// https://drafts.csswg.org/css-text-4/#text-justify-property
enum class TextJustify : std::uint8_t {
  kAuto,
  kNone,
  kInterCharacter,
  kInterWord
  // When adding more values, ensure that the corresponding
  // 'field_size' in css_properties.json5 is large enough.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_JUSTIFY_H_
