// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TEXT_DECORATION_OFFSET_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TEXT_DECORATION_OFFSET_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

class ComputedStyle;
class SimpleFontData;
enum class FontVerticalPositionType;
enum class ResolvedUnderlinePosition;

class CORE_EXPORT TextDecorationOffsetBase {
  STACK_ALLOCATED();

 public:
  TextDecorationOffsetBase(const ComputedStyle& style) : style_(style) {}
  ~TextDecorationOffsetBase() = default;

  virtual int ComputeUnderlineOffsetForUnder(
      const Length& style_underline_offset,
      float computed_font_size,
      const SimpleFontData* font_data,
      float text_decoration_thickness,
      FontVerticalPositionType) const = 0;

  int ComputeUnderlineOffset(ResolvedUnderlinePosition,
                             float computed_font_size,
                             const SimpleFontData* font_data,
                             const Length& style_underline_offset,
                             float text_decoration_thickness) const;

 protected:
  static float StyleUnderlineOffsetToPixels(
      const Length& style_underline_offset,
      float font_size);
  const ComputedStyle& style_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TEXT_DECORATION_OFFSET_BASE_H_
