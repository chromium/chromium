// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/applied_text_decoration.h"

#include <memory>
#include "third_party/blink/renderer/core/style/data_equivalency.h"
#include "third_party/blink/renderer/core/style/svg_paint.h"

namespace blink {

struct AppliedTextDecoration::TextDecorationSvgPaints {
  const SVGPaint fill;
  const SVGPaint stroke;

  TextDecorationSvgPaints(const SVGPaint& fill_arg, const SVGPaint& stroke_arg)
      : fill(fill_arg), stroke(stroke_arg) {}

  bool operator==(const TextDecorationSvgPaints& other) const {
    return fill == other.fill && stroke == other.stroke;
  }

  bool operator!=(const TextDecorationSvgPaints& other) const {
    return !(*this == other);
  }
};

AppliedTextDecoration::AppliedTextDecoration(TextDecoration line,
                                             ETextDecorationStyle style,
                                             Color color,
                                             TextDecorationThickness thickness,
                                             Length underline_offset)

    : lines_(static_cast<unsigned>(line)),
      style_(static_cast<unsigned>(style)),
      color_(color),
      thickness_(thickness),
      underline_offset_(underline_offset) {}

AppliedTextDecoration::AppliedTextDecoration(TextDecoration line,
                                             ETextDecorationStyle style,
                                             const SVGPaint& fill,
                                             const SVGPaint& stroke,
                                             TextDecorationThickness thickness,
                                             Length underline_offset)

    : lines_(static_cast<unsigned>(line)),
      style_(static_cast<unsigned>(style)),
      thickness_(thickness),
      underline_offset_(underline_offset),
      svg_paints_(std::make_unique<TextDecorationSvgPaints>(fill, stroke)) {}

AppliedTextDecoration::AppliedTextDecoration(const AppliedTextDecoration& other)
    : lines_(other.lines_),
      style_(other.style_),
      color_(other.color_),
      thickness_(other.thickness_),
      underline_offset_(other.underline_offset_),
      svg_paints_(other.svg_paints_ ? std::make_unique<TextDecorationSvgPaints>(
                                          *other.svg_paints_)
                                    : nullptr) {}

AppliedTextDecoration::~AppliedTextDecoration() = default;

AppliedTextDecoration& AppliedTextDecoration::operator=(
    const AppliedTextDecoration& other) {
  lines_ = other.lines_;
  style_ = other.style_;
  color_ = other.color_;
  thickness_ = other.thickness_;
  underline_offset_ = other.underline_offset_;
  svg_paints_.reset();
  if (other.svg_paints_)
    svg_paints_ = std::make_unique<TextDecorationSvgPaints>(*other.svg_paints_);
  return *this;
}

bool AppliedTextDecoration::operator==(const AppliedTextDecoration& o) const {
  if (!DataEquivalent(svg_paints_, o.svg_paints_))
    return false;
  return color_ == o.color_ && lines_ == o.lines_ && style_ == o.style_ &&
         thickness_ == o.thickness_ && underline_offset_ == o.underline_offset_;
}

const SVGPaint& AppliedTextDecoration::Fill() const {
  return svg_paints_->fill;
}

const SVGPaint& AppliedTextDecoration::Stroke() const {
  return svg_paints_->stroke;
}

}  // namespace blink
