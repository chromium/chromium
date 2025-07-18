// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_STRETCHY_OPERATOR_SHAPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_STRETCHY_OPERATOR_SHAPER_H_

#include <unicode/uchar.h>

#include "third_party/blink/renderer/platform/fonts/glyph.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Font;
class ShapeResult;
class StretchyOperatorShaper;

// https://w3c.github.io/mathml-core/#algorithms-for-glyph-stretching
class PLATFORM_EXPORT StretchyOperatorShaper final {
  DISALLOW_NEW();

 public:
  StretchyOperatorShaper(UChar32 stretchy_character,
                         OpenTypeMathStretchData::StretchAxis stretch_axis,
                         TextDirection direction)
      : stretchy_character_(stretchy_character),
        stretch_axis_(stretch_axis),
        direction_(direction) {}

  struct Metrics {
    float advance { 0.0f };
    float ascent { 0.0f };
    float descent { 0.0f };
    float italic_correction { 0.0f };
  };
  // Shape the stretched operator. The coordinates of the glyph(s) use the same
  // origin as the rectangle assigned to the optional OUT Metrics parameter.
  // May be called multiple times; font and direction may vary between calls.
  // https://w3c.github.io/mathml-core/#dfn-shape-a-stretchy-glyph
  const ShapeResult* Shape(const Font*,
                           float target_size,
                           Metrics* metrics = nullptr) const;

  ~StretchyOperatorShaper() = default;

 private:
  const UChar32 stretchy_character_;
  const OpenTypeMathStretchData::StretchAxis stretch_axis_;
  const TextDirection direction_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_STRETCHY_OPERATOR_SHAPER_H_
