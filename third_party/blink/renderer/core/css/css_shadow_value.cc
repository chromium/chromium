/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Computer, Inc.
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
#include "third_party/blink/renderer/core/css/css_shadow_value.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Used for text-shadow and box-shadow
CSSShadowValue::CSSShadowValue(CSSPrimitiveValue* x,
                               CSSPrimitiveValue* y,
                               CSSPrimitiveValue* blur,
                               CSSPrimitiveValue* spread,
                               CSSIdentifierValue* style,
                               const CSSValue* color)
    : CSSValue(kShadowClass),
      x(x),
      y(y),
      blur(blur),
      spread(spread),
      style(style),
      color(color) {}

String CSSShadowValue::CustomCSSText() const {
  StringBuilder text;

  if (color) {
    text.Append(color->CssText());
    text.Append(' ');
  }

  text.Append(x->CssText());
  text.Append(' ');

  text.Append(y->CssText());

  if (blur) {
    text.Append(' ');
    text.Append(blur->CssText());
  }
  if (spread) {
    text.Append(' ');
    text.Append(spread->CssText());
  }
  if (style) {
    text.Append(' ');
    text.Append(style->CssText());
  }

  return text.ReleaseString();
}

bool CSSShadowValue::Equals(const CSSShadowValue& other) const {
  return base::ValuesEquivalent(color, other.color) &&
         base::ValuesEquivalent(x, other.x) &&
         base::ValuesEquivalent(y, other.y) &&
         base::ValuesEquivalent(blur, other.blur) &&
         base::ValuesEquivalent(spread, other.spread) &&
         base::ValuesEquivalent(style, other.style);
}

void CSSShadowValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(x);
  visitor->Trace(y);
  visitor->Trace(blur);
  visitor->Trace(spread);
  visitor->Trace(style);
  visitor->Trace(color);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
