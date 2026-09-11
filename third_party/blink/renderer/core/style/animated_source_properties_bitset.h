// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_SOURCE_PROPERTIES_BITSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_SOURCE_PROPERTIES_BITSET_H_

#include <cstdint>

#include "third_party/blink/renderer/core/css/css_property_names.h"

namespace blink {

// Bitmask representing a set of CSS properties that track animated sources.
using AnimatedSourceBits = uint32_t;
static_assert(kAnimatedSourcePropertyCount <= sizeof(AnimatedSourceBits) * 8,
              "AnimatedSourceBits needs a bit per tracked property");

// Returns the bit representing the given property.
constexpr AnimatedSourceBits BitForAnimatedSourceProperty(
    AnimatedSourceProperty property) {
  return AnimatedSourceBits{1} << static_cast<unsigned>(property);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_SOURCE_PROPERTIES_BITSET_H_
