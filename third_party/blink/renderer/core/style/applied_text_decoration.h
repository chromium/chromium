// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_APPLIED_TEXT_DECORATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_APPLIED_TEXT_DECORATION_H_

#include <memory>

#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/text_decoration_thickness.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct SVGPaint;

class CORE_EXPORT AppliedTextDecoration {
  DISALLOW_NEW();

 public:
  AppliedTextDecoration(TextDecoration,
                        ETextDecorationStyle,
                        Color,
                        TextDecorationThickness,
                        Length);
  AppliedTextDecoration(TextDecoration line,
                        ETextDecorationStyle style,
                        const SVGPaint& fill,
                        const SVGPaint& stroke,
                        TextDecorationThickness thickness,
                        Length underline_offset);
  AppliedTextDecoration(const AppliedTextDecoration& other);
  ~AppliedTextDecoration();
  AppliedTextDecoration& operator=(const AppliedTextDecoration& other);

  TextDecoration Lines() const { return static_cast<TextDecoration>(lines_); }
  ETextDecorationStyle Style() const {
    return static_cast<ETextDecorationStyle>(style_);
  }
  // GetColor() should not be used for SVG elements. Use Fill() and
  // Stroke() instead.
  Color GetColor() const { return color_; }
  void SetColor(Color color) { color_ = color; }
  // Fill() and Stroke() should not be used for non-SVG elements. Use
  // GetColor() instead.
  const SVGPaint& Fill() const;
  const SVGPaint& Stroke() const;

  TextDecorationThickness Thickness() const { return thickness_; }
  Length UnderlineOffset() const { return underline_offset_; }

  bool operator==(const AppliedTextDecoration&) const;
  bool operator!=(const AppliedTextDecoration& o) const {
    return !(*this == o);
  }

 private:
  unsigned lines_ : kTextDecorationBits;
  unsigned style_ : 3;  // ETextDecorationStyle
  Color color_;
  TextDecorationThickness thickness_;
  Length underline_offset_;

  struct TextDecorationSvgPaints;
  // svg_paints_ is not null if and only if this instance is for an SVG element.
  std::unique_ptr<TextDecorationSvgPaints> svg_paints_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_APPLIED_TEXT_DECORATION_H_
