// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/filters/fe_box_reflect.h"

#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

FEBoxReflect::FEBoxReflect(Filter* filter, const BoxReflection& reflection)
    : FilterEffect(filter), reflection_(reflection) {}

FEBoxReflect::~FEBoxReflect() = default;

gfx::RectF FEBoxReflect::MapEffect(const gfx::RectF& rect) const {
  return reflection_.MapRect(rect);
}

WTF::TextStream& FEBoxReflect::ExternalRepresentation(WTF::TextStream& ts,
                                                      int indent) const {
  // Only called for SVG layout tree printing.
  NOTREACHED_IN_MIGRATION();
  return ts;
}

sk_sp<PaintFilter> FEBoxReflect::CreateImageFilter() {
  return paint_filter_builder::BuildBoxReflectFilter(
      reflection_, paint_filter_builder::Build(InputEffect(0),
                                               OperatingInterpolationSpace()));
}

}  // namespace blink
