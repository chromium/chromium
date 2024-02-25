/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
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
 *
 */

#include "third_party/blink/renderer/core/style/shadow_data.h"

#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"

namespace blink {

ShadowData ShadowData::NeutralValue() {
  return ShadowData(gfx::Vector2dF(0, 0), 0, 0, ShadowStyle::kNormal,
                    StyleColor(Color::kTransparent));
}

gfx::OutsetsF ShadowData::RectOutsets() const {
  // 3 * sigma is how Skia computes the box blur extent.
  // See also https://crbug.com/624175.
  // TODO(fmalita): since the blur extent must reflect rasterization bounds,
  // its value should be queried from Skia (pending API availability).
  float blur_and_spread = ceil(3 * BlurRadiusToStdDev(Blur())) + Spread();
  return gfx::OutsetsF()
      .set_left(blur_and_spread - X())
      .set_right(blur_and_spread + X())
      .set_top(blur_and_spread - Y())
      .set_bottom(blur_and_spread + Y());
}

}  // namespace blink
