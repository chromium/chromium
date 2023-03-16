/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "third_party/blink/renderer/core/svg/color_distance.h"

#include "third_party/blink/renderer/platform/graphics/color.h"

#include <math.h>

namespace blink {

Color ColorDistance::AddColors(const Color& first, const Color& second) {
  return Color(first.Red() + second.Red(), first.Green() + second.Green(),
               first.Blue() + second.Blue());
}

float ColorDistance::Distance(const Color& from_color, const Color& to_color) {
  int red_diff = to_color.Red() - from_color.Red();
  int green_diff = to_color.Green() - from_color.Green();
  int blue_diff = to_color.Blue() - from_color.Blue();

  // This is just a simple distance calculation, not respecting color spaces
  return sqrtf(red_diff * red_diff + blue_diff * blue_diff +
               green_diff * green_diff);
}

}  // namespace blink
