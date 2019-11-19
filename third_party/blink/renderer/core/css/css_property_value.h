/*
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_VALUE_H_

#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct CSSPropertyValueMetadata {
  DISALLOW_NEW();
  CSSPropertyValueMetadata(const CSSProperty& property,
                           bool is_set_from_shorthand,
                           int index_in_shorthands_vector,
                           bool important,
                           bool implicit,
                           bool inherited)
      : property_(&property),
        is_set_from_shorthand_(is_set_from_shorthand),
        index_in_shorthands_vector_(index_in_shorthands_vector),
        important_(important),
        implicit_(implicit),
        inherited_(inherited) {}

  CSSPropertyID ShorthandID() const;
  const CSSProperty& Property() const { return *property_; }

  const CSSProperty* property_;

  unsigned is_set_from_shorthand_ : 1;
  // If this property was set as part of an ambiguous shorthand, gives the index
  // in the shorthands vector.
  unsigned index_in_shorthands_vector_ : 2;
  unsigned important_ : 1;
  // Whether or not the property was set implicitly as the result of a
  // shorthand.
  unsigned implicit_ : 1;
  unsigned inherited_ : 1;
};

class CORE_EXPORT CSSPropertyValue {
  DISALLOW_NEW();

 public:
  CSSPropertyValue(const CSSProperty& property,
                   const CSSValue& value,
                   bool important = false,
                   bool is_set_from_shorthand = false,
                   int index_in_shorthands_vector = 0,
                   bool implicit = false)
      : metadata_(property,
                  is_set_from_shorthand,
                  index_in_shorthands_vector,
                  important,
                  implicit,
                  property.IsInherited()),
        value_(value) {}

  // FIXME: Remove this.
  CSSPropertyValue(CSSPropertyValueMetadata metadata, const CSSValue& value)
      : metadata_(metadata), value_(value) {}

  CSSPropertyID Id() const { return metadata_.Property().PropertyID(); }
  const CSSProperty& Property() const { return metadata_.Property(); }
  bool IsSetFromShorthand() const { return metadata_.is_set_from_shorthand_; }
  CSSPropertyID ShorthandID() const { return metadata_.ShorthandID(); }
  bool IsImportant() const { return metadata_.important_; }
  CSSPropertyName Name() const;

  const CSSValue* Value() const { return value_.Get(); }

  const CSSPropertyValueMetadata& Metadata() const { return metadata_; }

  bool operator==(const CSSPropertyValue& other) const;

  void Trace(blink::Visitor* visitor) { visitor->Trace(value_); }

 private:
  CSSPropertyValueMetadata metadata_;
  Member<const CSSValue> value_;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::CSSPropertyValue)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_VALUE_H_
