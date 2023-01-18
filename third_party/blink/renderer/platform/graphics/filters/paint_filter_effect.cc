// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_effect.h"

#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

PaintFilterEffect::PaintFilterEffect(Filter* filter,
                                     const cc::PaintFlags& flags)
    : FilterEffect(filter), flags_(flags) {
  SetOperatingInterpolationSpace(kInterpolationSpaceSRGB);
}

PaintFilterEffect::~PaintFilterEffect() = default;

sk_sp<PaintFilter> PaintFilterEffect::CreateImageFilter() {
  // Only use the fields of PaintFlags that affect shading, ignore style and
  // other effects.
  const cc::PaintShader* shader = flags_.getShader();
  SkImageFilters::Dither dither = flags_.isDither()
                                      ? SkImageFilters::Dither::kYes
                                      : SkImageFilters::Dither::kNo;
  if (shader) {
    // Include the paint's alpha modulation
    return sk_make_sp<ShaderPaintFilter>(sk_ref_sp(shader), flags_.getAlphaf(),
                                         flags_.getFilterQuality(), dither);
  } else {
    // ShaderPaintFilter requires shader to be non-null
    return sk_make_sp<ShaderPaintFilter>(
        cc::PaintShader::MakeColor(flags_.getColor4f()), 1.0f,
        flags_.getFilterQuality(), dither);
  }
}

WTF::TextStream& PaintFilterEffect::ExternalRepresentation(WTF::TextStream& ts,
                                                           int indent) const {
  WriteIndent(ts, indent);
  ts << "[PaintFilterEffect]\n";
  return ts;
}

}  // namespace blink
