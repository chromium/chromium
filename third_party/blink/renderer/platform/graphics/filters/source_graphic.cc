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

#include "third_party/blink/renderer/platform/graphics/filters/source_graphic.h"

#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

SourceGraphic::SourceGraphic(Filter* filter) : FilterEffect(filter) {
  SetOperatingInterpolationSpace(kInterpolationSpaceSRGB);
}

SourceGraphic::~SourceGraphic() = default;

FloatRect SourceGraphic::MapInputs(const FloatRect& rect) const {
  return !source_rect_.IsEmpty() ? FloatRect(source_rect_) : rect;
}

void SourceGraphic::SetSourceRect(const IntRect& source_rect) {
  source_rect_ = source_rect;
}

WTF::TextStream& SourceGraphic::ExternalRepresentation(WTF::TextStream& ts,
                                                       int indent) const {
  WriteIndent(ts, indent);
  ts << "[SourceGraphic]\n";
  return ts;
}

}  // namespace blink
