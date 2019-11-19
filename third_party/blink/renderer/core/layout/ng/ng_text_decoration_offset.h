// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_TEXT_DECORATION_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_TEXT_DECORATION_OFFSET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset_base.h"

namespace blink {

class ComputedStyle;
class NGPhysicalBoxFragment;

// Class for computing the decoration offset for text fragments in LayoutNG.
class CORE_EXPORT NGTextDecorationOffset : public TextDecorationOffsetBase {
  STACK_ALLOCATED();

 public:
  NGTextDecorationOffset(const ComputedStyle& style,
                         const ComputedStyle& text_style,
                         const NGPhysicalBoxFragment* decorating_box)
      : TextDecorationOffsetBase(style),
        text_style_(text_style),
        decorating_box_(decorating_box) {}
  ~NGTextDecorationOffset() = default;

  int ComputeUnderlineOffsetForUnder(float text_decoration_thickness,
                                     FontVerticalPositionType) const override;

 private:
  const ComputedStyle& text_style_;
  const NGPhysicalBoxFragment* decorating_box_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_TEXT_DECORATION_OFFSET_H_
