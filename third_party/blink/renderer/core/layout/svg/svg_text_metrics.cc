/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/svg_text_metrics.h"

#include "third_party/blink/renderer/platform/fonts/font_orientation.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

SVGTextMetrics::SVGTextMetrics(unsigned length, float width, float height)
    : width_(width), height_(height), length_(length) {}

SVGTextMetrics::SVGTextMetrics(SVGTextMetrics::MetricsType)
    : SVGTextMetrics(1, 0, 0) {}

gfx::SizeF SVGTextMetrics::Extents() const {
  // TODO(fs): Negative glyph extents seems kind of weird to have, but
  // presently it can occur in some cases (like Arabic.)
  // gfx::SizeF clamps negative values to zeros.
  return gfx::SizeF(width_, height_);
}

float SVGTextMetrics::Advance(FontOrientation orientation) const {
  switch (orientation) {
    case FontOrientation::kHorizontal:
    case FontOrientation::kVerticalRotated:
      return width_;
    case FontOrientation::kVerticalUpright:
      return height_;
    default:
      NOTREACHED();
      return width_;
  }
}

}  // namespace blink
