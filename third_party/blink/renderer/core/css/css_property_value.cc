/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#include "third_party/blink/renderer/core/css/css_property_value.h"

#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsCSSPropertyValue {
  void* property;
  uint32_t bitfields;
  Member<void*> value;
};

ASSERT_SIZE(CSSPropertyValue, SameSizeAsCSSPropertyValue);

CSSPropertyID CSSPropertyValue::ShorthandID() const {
  if (!is_set_from_shorthand_) {
    return CSSPropertyID::kInvalid;
  }

  Vector<StylePropertyShorthand, 4> shorthands;
  getMatchingShorthandsForLonghand(PropertyID(), &shorthands);
  DCHECK(shorthands.size());
  DCHECK_GE(index_in_shorthands_vector_, 0u);
  DCHECK_LT(index_in_shorthands_vector_, shorthands.size());
  return shorthands.at(index_in_shorthands_vector_).id();
}

CSSPropertyName CSSPropertyValue::Name() const {
  if (PropertyID() != CSSPropertyID::kVariable) {
    return CSSPropertyName(PropertyID());
  }
  return CSSPropertyName(custom_name_);
}

bool CSSPropertyValue::operator==(const CSSPropertyValue& other) const {
  return base::ValuesEquivalent(value_, other.value_) &&
         IsImportant() == other.IsImportant();
}

}  // namespace blink
