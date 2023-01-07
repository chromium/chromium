/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    Based on khtml code by:
    Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
    Copyright (C) 2002-2003 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "third_party/blink/renderer/core/style/svg_paint.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"

namespace blink {

SVGPaint::SVGPaint() = default;
SVGPaint::SVGPaint(Color color) : color(color), type(SVGPaintType::kColor) {}
SVGPaint::SVGPaint(const SVGPaint& paint) = default;

SVGPaint::~SVGPaint() = default;

SVGPaint& SVGPaint::operator=(const SVGPaint& paint) = default;

bool SVGPaint::operator==(const SVGPaint& other) const {
  return type == other.type && color == other.color &&
         base::ValuesEquivalent(resource, other.resource);
}

const AtomicString& SVGPaint::GetUrl() const {
  return Resource()->Url();
}

}  // namespace blink
