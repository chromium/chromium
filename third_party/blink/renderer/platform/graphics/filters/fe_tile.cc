/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
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

#include "third_party/blink/renderer/platform/graphics/filters/fe_tile.h"

#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

FETile::FETile(Filter* filter) : FilterEffect(filter) {}

gfx::RectF FETile::MapInputs(const gfx::RectF& rect) const {
  return AbsoluteBounds();
}

gfx::RectF FETile::GetSourceRect() const {
  const FilterEffect* input = InputEffect(0);
  if (input->GetFilterEffectType() == kFilterEffectTypeSourceInput)
    return GetFilter()->FilterRegion();
  return input->FilterPrimitiveSubregion();
}

sk_sp<PaintFilter> FETile::CreateImageFilter() {
  sk_sp<PaintFilter> input(paint_filter_builder::Build(
      InputEffect(0), OperatingInterpolationSpace()));
  if (!input)
    return nullptr;
  gfx::RectF src_rect =
      GetFilter()->MapLocalRectToAbsoluteRect(GetSourceRect());
  gfx::RectF dst_rect =
      GetFilter()->MapLocalRectToAbsoluteRect(FilterPrimitiveSubregion());
  return sk_make_sp<TilePaintFilter>(gfx::RectFToSkRect(src_rect),
                                     gfx::RectFToSkRect(dst_rect),
                                     std::move(input));
}

WTF::TextStream& FETile::ExternalRepresentation(WTF::TextStream& ts,
                                                int indent) const {
  WriteIndent(ts, indent);
  ts << "[feTile";
  FilterEffect::ExternalRepresentation(ts);
  ts << "]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);

  return ts;
}

}  // namespace blink
