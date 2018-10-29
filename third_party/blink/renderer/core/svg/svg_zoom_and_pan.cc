/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_zoom_and_pan.h"

#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"

namespace blink {

SVGZoomAndPan::SVGZoomAndPan() : zoom_and_pan_(kSVGZoomAndPanMagnify) {}

bool SVGZoomAndPan::IsKnownAttribute(const QualifiedName& attr_name) {
  return attr_name == svg_names::kZoomAndPanAttr;
}

bool SVGZoomAndPan::ParseAttribute(const QualifiedName& name,
                                   const AtomicString& value) {
  if (name != svg_names::kZoomAndPanAttr)
    return false;
  zoom_and_pan_ = kSVGZoomAndPanUnknown;
  if (!value.IsEmpty()) {
    if (value.Is8Bit()) {
      const LChar* start = value.Characters8();
      zoom_and_pan_ = Parse(start, start + value.length());
    } else {
      const UChar* start = value.Characters16();
      zoom_and_pan_ = Parse(start, start + value.length());
    }
  }
  return true;
}

template <typename CharType>
static SVGZoomAndPanType ParseZoomAndPanInternal(const CharType*& start,
                                                 const CharType* end) {
  if (SkipToken(start, end, "disable"))
    return kSVGZoomAndPanDisable;
  if (SkipToken(start, end, "magnify"))
    return kSVGZoomAndPanMagnify;
  return kSVGZoomAndPanUnknown;
}

SVGZoomAndPanType SVGZoomAndPan::Parse(const LChar*& start, const LChar* end) {
  return ParseZoomAndPanInternal(start, end);
}

SVGZoomAndPanType SVGZoomAndPan::Parse(const UChar*& start, const UChar* end) {
  return ParseZoomAndPanInternal(start, end);
}

}  // namespace blink
