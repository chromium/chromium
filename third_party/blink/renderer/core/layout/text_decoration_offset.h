// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TEXT_DECORATION_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TEXT_DECORATION_OFFSET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset_base.h"

namespace blink {

class ComputedStyle;
class InlineTextBox;

class CORE_EXPORT TextDecorationOffset : public TextDecorationOffsetBase {
  STACK_ALLOCATED();

 public:
  TextDecorationOffset(const ComputedStyle& style,
                       const InlineTextBox* inline_text_box,
                       LineLayoutItem decorating_box)
      : TextDecorationOffsetBase(style),
        inline_text_box_(inline_text_box),
        decorating_box_(decorating_box) {}
  ~TextDecorationOffset() = default;

  int ComputeUnderlineOffsetForUnder(const Length& style_underline_offset,
                                     float computed_font_size,
                                     const SimpleFontData* font_data,
                                     float text_decoration_thickness,
                                     FontVerticalPositionType) const override;

 private:
  const InlineTextBox* inline_text_box_;
  LineLayoutItem decorating_box_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TEXT_DECORATION_OFFSET_H_
