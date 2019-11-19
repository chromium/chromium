/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/platform/graphics/filters/source_alpha.h"

#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"

namespace blink {

SourceAlpha::SourceAlpha(FilterEffect* source_effect)
    : FilterEffect(source_effect->GetFilter()) {
  SetOperatingInterpolationSpace(source_effect->OperatingInterpolationSpace());
  InputEffects().push_back(source_effect);
}

sk_sp<PaintFilter> SourceAlpha::CreateImageFilter() {
  sk_sp<PaintFilter> source_graphic(paint_filter_builder::Build(
      InputEffect(0), OperatingInterpolationSpace()));
  float matrix[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                      0, 0, 0, 0, 0, 0, 0, 0, 1, 0};
  sk_sp<SkColorFilter> color_filter = SkColorFilters::Matrix(matrix);
  return sk_make_sp<ColorFilterPaintFilter>(std::move(color_filter),
                                            std::move(source_graphic));
}

WTF::TextStream& SourceAlpha::ExternalRepresentation(WTF::TextStream& ts,
                                                     int indent) const {
  WriteIndent(ts, indent);
  ts << "[SourceAlpha]\n";
  return ts;
}

}  // namespace blink
