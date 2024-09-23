/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_string.h"

#include "base/notreached.h"

namespace blink {

void SVGString::Add(const SVGPropertyBase*, const SVGElement*) {
  NOTREACHED_IN_MIGRATION();
}

void SVGString::CalculateAnimatedValue(const SMILAnimationEffectParameters&,
                                       float percentage,
                                       unsigned repeat_count,
                                       const SVGPropertyBase* from,
                                       const SVGPropertyBase* to,
                                       const SVGPropertyBase*,
                                       const SVGElement*) {
  NOTREACHED_IN_MIGRATION();
}

float SVGString::CalculateDistance(const SVGPropertyBase*,
                                   const SVGElement*) const {
  // No paced animations for strings.
  return -1;
}

}  // namespace blink
