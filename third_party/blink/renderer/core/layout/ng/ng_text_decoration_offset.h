// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_TEXT_DECORATION_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_TEXT_DECORATION_OFFSET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset_base.h"

namespace blink {

class ComputedStyle;

// Class for computing the decoration offset for text fragments in LayoutNG.
class CORE_EXPORT NGTextDecorationOffset : public TextDecorationOffsetBase {
  STACK_ALLOCATED();

 public:
  NGTextDecorationOffset(const ComputedStyle& style,
                         const ComputedStyle& text_style)
      : TextDecorationOffsetBase(style), text_style_(text_style) {}
  ~NGTextDecorationOffset() = default;

  int ComputeUnderlineOffsetForUnder(const Length& style_underline_offset,
                                     float computed_font_size,
                                     const SimpleFontData* font_data,
                                     float text_decoration_thickness,
                                     FontVerticalPositionType) const override;

 private:
  const ComputedStyle& text_style_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_TEXT_DECORATION_OFFSET_H_
