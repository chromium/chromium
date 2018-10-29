// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TEXT_DECORATION_OFFSET_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TEXT_DECORATION_OFFSET_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class ComputedStyle;
enum class FontVerticalPositionType;
enum class ResolvedUnderlinePosition;
class FontMetrics;

class CORE_EXPORT TextDecorationOffsetBase {
  STACK_ALLOCATED();

 public:
  TextDecorationOffsetBase(const ComputedStyle& style) : style_(style) {}
  ~TextDecorationOffsetBase() = default;

  virtual int ComputeUnderlineOffsetForUnder(
      float text_decoration_thickness,
      FontVerticalPositionType) const = 0;

  int ComputeUnderlineOffsetForRoman(const FontMetrics&,
                                     float text_decoration_thickness) const;

  int ComputeUnderlineOffset(ResolvedUnderlinePosition,
                             const FontMetrics&,
                             float text_decoration_thickness) const;

 protected:
  const ComputedStyle& style_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TEXT_DECORATION_OFFSET_BASE_H_
