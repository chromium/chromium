/*
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 *           (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_cursor_image_value.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace cssvalue {

CSSCursorImageValue::CSSCursorImageValue(const CSSValue& image_value,
                                         bool hot_spot_specified,
                                         const gfx::Point& hot_spot)
    : CSSValue(kCursorImageClass),
      image_value_(&image_value),
      hot_spot_(hot_spot),
      hot_spot_specified_(hot_spot_specified) {
  DCHECK(image_value.IsImageValue() || image_value.IsImageSetValue());
}

String CSSCursorImageValue::CustomCSSText() const {
  StringBuilder result;
  result.Append(image_value_->CssText());
  if (hot_spot_specified_) {
    result.Append(' ');
    result.AppendNumber(hot_spot_.x());
    result.Append(' ');
    result.AppendNumber(hot_spot_.y());
  }
  return result.ReleaseString();
}

bool CSSCursorImageValue::Equals(const CSSCursorImageValue& other) const {
  return (hot_spot_specified_
              ? other.hot_spot_specified_ && hot_spot_ == other.hot_spot_
              : !other.hot_spot_specified_) &&
         base::ValuesEquivalent(image_value_, other.image_value_);
}

void CSSCursorImageValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(image_value_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue

}  // namespace blink
