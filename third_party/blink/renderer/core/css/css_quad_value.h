/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_QUAD_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_QUAD_VALUE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CORE_EXPORT CSSQuadValue : public CSSValue {
 public:
  enum TypeForSerialization { kSerializeAsRect, kSerializeAsQuad };

  static CSSQuadValue* Create(CSSValue* value,
                              TypeForSerialization serialization_type) {
    return MakeGarbageCollected<CSSQuadValue>(value, value, value, value,
                                              serialization_type);
  }

  CSSQuadValue(CSSValue* top,
               CSSValue* right,
               CSSValue* bottom,
               CSSValue* left,
               TypeForSerialization serialization_type)
      : CSSValue(kQuadClass),
        serialization_type_(serialization_type),
        top_(top),
        right_(right),
        bottom_(bottom),
        left_(left) {}

  CSSValue* Top() const { return top_.Get(); }
  CSSValue* Right() const { return right_.Get(); }
  CSSValue* Bottom() const { return bottom_.Get(); }
  CSSValue* Left() const { return left_.Get(); }

  TypeForSerialization SerializationType() { return serialization_type_; }

  String CustomCSSText() const;

  bool Equals(const CSSQuadValue& other) const {
    return DataEquivalent(top_, other.top_) &&
           DataEquivalent(right_, other.right_) &&
           DataEquivalent(left_, other.left_) &&
           DataEquivalent(bottom_, other.bottom_);
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  TypeForSerialization serialization_type_;
  Member<CSSValue> top_;
  Member<CSSValue> right_;
  Member<CSSValue> bottom_;
  Member<CSSValue> left_;
};

template <>
struct DowncastTraits<CSSQuadValue> {
  static bool AllowFrom(const CSSValue& value) { return value.IsQuadValue(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_QUAD_VALUE_H_
