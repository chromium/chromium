/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_PROPERTY_SERIALIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_PROPERTY_SERIALIZER_H_

#include <bitset>
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"

namespace blink {

class CSSPropertyValueSet;
class StylePropertyShorthand;

class StylePropertySerializer {
  STACK_ALLOCATED();

 public:
  explicit StylePropertySerializer(const CSSPropertyValueSet&);

  String AsText() const;
  String SerializeShorthand(CSSPropertyID) const;

 private:
  String GetCommonValue(const StylePropertyShorthand&) const;
  String BorderPropertyValue(const StylePropertyShorthand&,
                             const StylePropertyShorthand&,
                             const StylePropertyShorthand&) const;
  String BorderImagePropertyValue() const;
  String BorderRadiusValue() const;
  String GetLayeredShorthandValue(const StylePropertyShorthand&) const;
  String Get2Values(const StylePropertyShorthand&) const;
  String Get4Values(const StylePropertyShorthand&) const;
  String PageBreakPropertyValue(const StylePropertyShorthand&) const;
  String GetShorthandValue(const StylePropertyShorthand&,
                           String separator = " ") const;
  String FontValue() const;
  String FontVariantValue() const;
  bool AppendFontLonghandValueIfNotNormal(const CSSProperty&,
                                          StringBuilder& result) const;
  String OffsetValue() const;
  String BackgroundRepeatPropertyValue() const;
  String GetPropertyText(const CSSProperty&,
                         const String& value,
                         bool is_important,
                         bool is_not_first_decl) const;
  bool IsPropertyShorthandAvailable(const StylePropertyShorthand&) const;
  bool ShorthandHasOnlyInitialOrInheritedValue(
      const StylePropertyShorthand&) const;
  void AppendBackgroundPropertyAsText(StringBuilder& result,
                                      unsigned& num_decls) const;

  // This function does checks common to all shorthands, and returns:
  // - The serialization if the shorthand serializes as a css-wide keyword.
  // - An empty string if either some longhands are not set, the important
  // flag is not set consistently, or css-wide keywords are used. In these
  // cases serialization will always fail.
  // - A null string otherwise.
  String CommonShorthandChecks(const StylePropertyShorthand&) const;

  // Only StylePropertySerializer uses the following two classes.
  class PropertyValueForSerializer {
    STACK_ALLOCATED();

   public:
    explicit PropertyValueForSerializer(
        CSSPropertyValueSet::PropertyReference property)
        : value_(property.Value()),
          property_(property.Property()),
          is_important_(property.IsImportant()),
          is_inherited_(property.IsInherited()) {}

    // TODO(sashab): Make this take a const CSSValue&.
    PropertyValueForSerializer(const CSSProperty& property,
                               const CSSValue* value,
                               bool is_important)
        : value_(value),
          property_(property),
          is_important_(is_important),
          is_inherited_(value->IsInheritedValue()) {}

    const CSSProperty& Property() const { return property_; }
    const CSSValue* Value() const { return value_; }
    bool IsImportant() const { return is_important_; }
    bool IsInherited() const { return is_inherited_; }
    bool IsValid() const { return value_; }

   private:
    Member<const CSSValue> value_;
    const CSSProperty& property_;
    bool is_important_;
    bool is_inherited_;
  };

  String GetCustomPropertyText(const PropertyValueForSerializer&,
                               bool is_not_first_decl) const;

  class CSSPropertyValueSetForSerializer final {
    DISALLOW_NEW();

   public:
    explicit CSSPropertyValueSetForSerializer(const CSSPropertyValueSet&);

    unsigned PropertyCount() const;
    PropertyValueForSerializer PropertyAt(unsigned index) const;
    bool ShouldProcessPropertyAt(unsigned index) const;
    int FindPropertyIndex(const CSSProperty&) const;
    const CSSValue* GetPropertyCSSValue(const CSSProperty&) const;
    bool IsDescriptorContext() const;

    void Trace(blink::Visitor*);

   private:
    bool HasExpandedAllProperty() const {
      return HasAllProperty() && need_to_expand_all_;
    }
    bool HasAllProperty() const { return all_index_ != -1; }

    Member<const CSSPropertyValueSet> property_set_;
    int all_index_;
    std::bitset<numCSSProperties> longhand_property_used_;
    bool need_to_expand_all_;
  };

  const CSSPropertyValueSetForSerializer property_set_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_PROPERTY_SERIALIZER_H_
