// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_BUFFER_H_

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PLATFORM_EXPORT ShapeResultBuffer {
  STACK_ALLOCATED();

 public:
  ShapeResultBuffer() = delete;
  ShapeResultBuffer(const ShapeResultBuffer&) = delete;
  ShapeResultBuffer& operator=(const ShapeResultBuffer&) = delete;

  struct CharacterRangeContext {
    const StringView& text;
    const bool is_rtl;
    int from;
    int to;
    float current_x;
    unsigned total_num_characters = 0;
    std::optional<float> from_x;
    std::optional<float> to_x;
    float min_y = 0;
    float max_y = 0;
  };
  // A helper for GetCharacterRange().
  static void ComputeRangeIn(const ShapeResult& result,
                             const gfx::RectF& ink_bounds,
                             CharacterRangeContext& context);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_BUFFER_H_
