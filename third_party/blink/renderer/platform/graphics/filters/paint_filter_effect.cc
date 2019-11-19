// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_effect.h"

#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/skia/include/effects/SkPaintImageFilter.h"

namespace blink {

PaintFilterEffect::PaintFilterEffect(Filter* filter, const PaintFlags& flags)
    : FilterEffect(filter), flags_(flags) {
  SetOperatingInterpolationSpace(kInterpolationSpaceSRGB);
}

PaintFilterEffect::~PaintFilterEffect() = default;

sk_sp<PaintFilter> PaintFilterEffect::CreateImageFilter() {
  return sk_make_sp<PaintFlagsPaintFilter>(flags_);
}

WTF::TextStream& PaintFilterEffect::ExternalRepresentation(WTF::TextStream& ts,
                                                           int indent) const {
  WriteIndent(ts, indent);
  ts << "[PaintFilterEffect]\n";
  return ts;
}

}  // namespace blink
