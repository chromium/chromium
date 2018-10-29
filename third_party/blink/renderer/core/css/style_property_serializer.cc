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

#include "third_party/blink/renderer/core/css/style_property_serializer.h"

#include <bitset>
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

StylePropertySerializer::CSSPropertyValueSetForSerializer::
    CSSPropertyValueSetForSerializer(const CSSPropertyValueSet& properties)
    : property_set_(&properties),
      all_index_(property_set_->FindPropertyIndex(CSSPropertyAll)),
      need_to_expand_all_(false) {
  if (!HasAllProperty())
    return;

  CSSPropertyValueSet::PropertyReference all_property =
      property_set_->PropertyAt(all_index_);
  for (unsigned i = 0; i < property_set_->PropertyCount(); ++i) {
    CSSPropertyValueSet::PropertyReference property =
        property_set_->PropertyAt(i);
    if (property.Property().IsAffectedByAll()) {
      if (all_property.IsImportant() && !property.IsImportant())
        continue;
      if (static_cast<unsigned>(all_index_) >= i)
        continue;
      if (property.Value() == all_property.Value() &&
          property.IsImportant() == all_property.IsImportant())
        continue;
      need_to_expand_all_ = true;
    }
    if (!isCSSPropertyIDWithName(property.Id()))
      continue;
    longhand_property_used_.set(property.Id() - firstCSSProperty);
  }
}

void StylePropertySerializer::CSSPropertyValueSetForSerializer::Trace(
    blink::Visitor* visitor) {
  visitor->Trace(property_set_);
}

unsigned
StylePropertySerializer::CSSPropertyValueSetForSerializer::PropertyCount()
    const {
  if (!HasExpandedAllProperty())
    return property_set_->PropertyCount();
  return lastCSSProperty - firstCSSProperty + 1;
}

StylePropertySerializer::PropertyValueForSerializer
StylePropertySerializer::CSSPropertyValueSetForSerializer::PropertyAt(
    unsigned index) const {
  if (!HasExpandedAllProperty())
    return StylePropertySerializer::PropertyValueForSerializer(
        property_set_->PropertyAt(index));

  CSSPropertyID property_id =
      static_cast<CSSPropertyID>(index + firstCSSProperty);
  DCHECK(isCSSPropertyIDWithName(property_id));
  if (longhand_property_used_.test(index)) {
    int index = property_set_->FindPropertyIndex(property_id);
    DCHECK_NE(index, -1);
    return StylePropertySerializer::PropertyValueForSerializer(
        property_set_->PropertyAt(index));
  }

  CSSPropertyValueSet::PropertyReference property =
      property_set_->PropertyAt(all_index_);
  return StylePropertySerializer::PropertyValueForSerializer(
      CSSProperty::Get(property_id), &property.Value(), property.IsImportant());
}

bool StylePropertySerializer::CSSPropertyValueSetForSerializer::
    ShouldProcessPropertyAt(unsigned index) const {
  // CSSPropertyValueSet has all valid longhands. We should process.
  if (!HasAllProperty())
    return true;

  // If all is not expanded, we need to process "all" and properties which
  // are not overwritten by "all".
  if (!need_to_expand_all_) {
    CSSPropertyValueSet::PropertyReference property =
        property_set_->PropertyAt(index);
    if (property.Property().IDEquals(CSSPropertyAll) ||
        !property.Property().IsAffectedByAll())
      return true;
    if (!isCSSPropertyIDWithName(property.Id()))
      return false;
    return longhand_property_used_.test(property.Id() - firstCSSProperty);
  }

  CSSPropertyID property_id =
      static_cast<CSSPropertyID>(index + firstCSSProperty);
  DCHECK(isCSSPropertyIDWithName(property_id));
  const CSSProperty& property_class =
      CSSProperty::Get(resolveCSSPropertyID(property_id));

  // Since "all" is expanded, we don't need to process "all".
  // We should not process expanded shorthands (e.g. font, background,
  // and so on) either.
  if (property_class.IsShorthand() || property_class.IDEquals(CSSPropertyAll))
    return false;

  // The all property is a shorthand that resets all CSS properties except
  // direction and unicode-bidi. It only accepts the CSS-wide keywords.
  // c.f. https://drafts.csswg.org/css-cascade/#all-shorthand
  if (!property_class.IsAffectedByAll())
    return longhand_property_used_.test(index);

  return true;
}

int StylePropertySerializer::CSSPropertyValueSetForSerializer::
    FindPropertyIndex(const CSSProperty& property) const {
  CSSPropertyID property_id = property.PropertyID();
  if (!HasExpandedAllProperty())
    return property_set_->FindPropertyIndex(property_id);
  return property_id - firstCSSProperty;
}

const CSSValue*
StylePropertySerializer::CSSPropertyValueSetForSerializer::GetPropertyCSSValue(
    const CSSProperty& property) const {
  int index = FindPropertyIndex(property);
  if (index == -1)
    return nullptr;
  StylePropertySerializer::PropertyValueForSerializer value = PropertyAt(index);
  return value.Value();
}

bool StylePropertySerializer::CSSPropertyValueSetForSerializer::
    IsDescriptorContext() const {
  return property_set_->CssParserMode() == kCSSViewportRuleMode ||
         property_set_->CssParserMode() == kCSSFontFaceRuleMode;
}

StylePropertySerializer::StylePropertySerializer(
    const CSSPropertyValueSet& properties)
    : property_set_(properties) {}

String StylePropertySerializer::GetCustomPropertyText(
    const PropertyValueForSerializer& property,
    bool is_not_first_decl) const {
  DCHECK_EQ(property.Property().PropertyID(), CSSPropertyVariable);
  StringBuilder result;
  if (is_not_first_decl)
    result.Append(' ');
  const CSSCustomPropertyDeclaration* value =
      ToCSSCustomPropertyDeclaration(property.Value());
  SerializeIdentifier(value->GetName(), result, is_not_first_decl);
  result.Append(':');
  if (!value->Value())
    result.Append(' ');
  result.Append(value->CustomCSSText());
  if (property.IsImportant())
    result.Append(" !important");
  result.Append(';');
  return result.ToString();
}

String StylePropertySerializer::GetPropertyText(const CSSProperty& property,
                                                const String& value,
                                                bool is_important,
                                                bool is_not_first_decl) const {
  StringBuilder result;
  if (is_not_first_decl)
    result.Append(' ');
  result.Append(property.GetPropertyName());
  result.Append(": ");
  result.Append(value);
  if (is_important)
    result.Append(" !important");
  result.Append(';');
  return result.ToString();
}

String StylePropertySerializer::AsText() const {
  StringBuilder result;

  std::bitset<numCSSProperties> longhand_serialized;
  std::bitset<numCSSProperties> shorthand_appeared;

  unsigned size = property_set_.PropertyCount();
  unsigned num_decls = 0;
  for (unsigned n = 0; n < size; ++n) {
    if (!property_set_.ShouldProcessPropertyAt(n))
      continue;

    StylePropertySerializer::PropertyValueForSerializer property =
        property_set_.PropertyAt(n);
    const CSSProperty& property_class = property.Property();
    CSSPropertyID property_id = property_class.PropertyID();

    // Only enabled properties should be part of the style.
    DCHECK(property_class.IsEnabled());
    // All shorthand properties should have been expanded at parse time.
    DCHECK(property_set_.IsDescriptorContext() ||
           (property_class.IsProperty() && !property_class.IsShorthand()));
    DCHECK(!property_set_.IsDescriptorContext() ||
           property_class.IsDescriptor());

    switch (property_id) {
      case CSSPropertyVariable:
        result.Append(GetCustomPropertyText(property, num_decls++));
        continue;
      case CSSPropertyAll:
        result.Append(GetPropertyText(property_class,
                                      property.Value()->CssText(),
                                      property.IsImportant(), num_decls++));
        continue;
      default:
        break;
    }
    if (longhand_serialized.test(property_id - firstCSSProperty))
      continue;

    Vector<StylePropertyShorthand, 4> shorthands;
    getMatchingShorthandsForLonghand(property_id, &shorthands);
    bool serialized_as_shorthand = false;
    for (const StylePropertyShorthand& shorthand : shorthands) {
      // Some aliases are implemented as a shorthand, in which case
      // we prefer to not use the shorthand.
      if (shorthand.length() == 1)
        continue;

      CSSPropertyID shorthand_property = shorthand.id();
      int shorthand_property_index = shorthand_property - firstCSSProperty;
      // We already tried serializing as this shorthand
      if (shorthand_appeared.test(shorthand_property_index))
        continue;

      shorthand_appeared.set(shorthand_property_index);
      bool serialized_other_longhand = false;
      for (unsigned i = 0; i < shorthand.length(); i++) {
        if (longhand_serialized.test(shorthand.properties()[i]->PropertyID() -
                                     firstCSSProperty)) {
          serialized_other_longhand = true;
          break;
        }
      }
      if (serialized_other_longhand)
        continue;

      String shorthand_result = SerializeShorthand(shorthand_property);
      if (shorthand_result.IsEmpty())
        continue;

      result.Append(GetPropertyText(CSSProperty::Get(shorthand_property),
                                    shorthand_result, property.IsImportant(),
                                    num_decls++));
      serialized_as_shorthand = true;
      for (unsigned i = 0; i < shorthand.length(); i++) {
        longhand_serialized.set(shorthand.properties()[i]->PropertyID() -
                                firstCSSProperty);
      }
      break;
    }

    if (serialized_as_shorthand)
      continue;

    result.Append(GetPropertyText(property_class, property.Value()->CssText(),
                                  property.IsImportant(), num_decls++));
  }

  DCHECK(!num_decls ^ !result.IsEmpty());
  return result.ToString();
}

// As per css-cascade, shorthands do not expand longhands to the value
// "initial", except when the shorthand is set to "initial", instead
// setting "missing" sub-properties to their initial values. This means
// that a shorthand can never represent a list of subproperties where
// some are "initial" and some are not, and so serialization should
// always fail in these cases (as per cssom). However we currently use
// "initial" instead of the initial values for certain shorthands, so
// these are special-cased here.
// TODO(timloh): Don't use "initial" in shorthands and remove this
// special-casing
static bool AllowInitialInShorthand(CSSPropertyID property_id) {
  switch (property_id) {
    case CSSPropertyBackground:
    case CSSPropertyBorder:
    case CSSPropertyBorderTop:
    case CSSPropertyBorderRight:
    case CSSPropertyBorderBottom:
    case CSSPropertyBorderLeft:
    case CSSPropertyOutline:
    case CSSPropertyColumnRule:
    case CSSPropertyColumns:
    case CSSPropertyFlex:
    case CSSPropertyFlexFlow:
    case CSSPropertyGridColumn:
    case CSSPropertyGridRow:
    case CSSPropertyGridArea:
    case CSSPropertyGap:
    case CSSPropertyListStyle:
    case CSSPropertyOffset:
    case CSSPropertyTextDecoration:
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMask:
    case CSSPropertyWebkitTextEmphasis:
    case CSSPropertyWebkitTextStroke:
      return true;
    default:
      return false;
  }
}

String StylePropertySerializer::CommonShorthandChecks(
    const StylePropertyShorthand& shorthand) const {
  int longhand_count = shorthand.length();
  DCHECK_LE(longhand_count, 17);
  const CSSValue* longhands[17] = {};

  bool has_important = false;
  bool has_non_important = false;

  for (int i = 0; i < longhand_count; i++) {
    int index = property_set_.FindPropertyIndex(*shorthand.properties()[i]);
    if (index == -1)
      return g_empty_string;
    PropertyValueForSerializer value = property_set_.PropertyAt(index);

    has_important |= value.IsImportant();
    has_non_important |= !value.IsImportant();
    longhands[i] = value.Value();
  }

  if (has_important && has_non_important)
    return g_empty_string;

  if (longhands[0]->IsCSSWideKeyword() ||
      longhands[0]->IsPendingSubstitutionValue()) {
    bool success = true;
    for (int i = 1; i < longhand_count; i++) {
      if (!DataEquivalent(longhands[i], longhands[0])) {
        // This should just return emptyString but some shorthands currently
        // allow 'initial' for their longhands.
        success = false;
        break;
      }
    }
    if (success) {
      if (longhands[0]->IsPendingSubstitutionValue()) {
        const CSSPendingSubstitutionValue* substitution_value =
            ToCSSPendingSubstitutionValue(longhands[0]);
        if (substitution_value->ShorthandPropertyId() != shorthand.id())
          return g_empty_string;
        return substitution_value->ShorthandValue()->CssText();
      }
      return longhands[0]->CssText();
    }
  }

  bool allow_initial = AllowInitialInShorthand(shorthand.id());
  for (int i = 0; i < longhand_count; i++) {
    const CSSValue& value = *longhands[i];
    if (!allow_initial && value.IsInitialValue())
      return g_empty_string;
    if (value.IsInheritedValue() || value.IsUnsetValue() ||
        value.IsPendingSubstitutionValue())
      return g_empty_string;
    if (value.IsVariableReferenceValue())
      return g_empty_string;
  }

  return String();
}

String StylePropertySerializer::SerializeShorthand(
    CSSPropertyID property_id) const {
  const StylePropertyShorthand& shorthand = shorthandForProperty(property_id);
  DCHECK(shorthand.length());

  String result = CommonShorthandChecks(shorthand);
  if (!result.IsNull())
    return result;

  switch (property_id) {
    case CSSPropertyAnimation:
      return GetLayeredShorthandValue(animationShorthand());
    case CSSPropertyBorderSpacing:
      return Get2Values(borderSpacingShorthand());
    case CSSPropertyBackgroundPosition:
      return GetLayeredShorthandValue(backgroundPositionShorthand());
    case CSSPropertyBackgroundRepeat:
      return BackgroundRepeatPropertyValue();
    case CSSPropertyBackground:
      return GetLayeredShorthandValue(backgroundShorthand());
    case CSSPropertyBorder:
      return BorderPropertyValue(borderWidthShorthand(), borderStyleShorthand(),
                                 borderColorShorthand());
    case CSSPropertyBorderImage:
      return BorderImagePropertyValue();
    case CSSPropertyBorderTop:
      return GetShorthandValue(borderTopShorthand());
    case CSSPropertyBorderRight:
      return GetShorthandValue(borderRightShorthand());
    case CSSPropertyBorderBottom:
      return GetShorthandValue(borderBottomShorthand());
    case CSSPropertyBorderLeft:
      return GetShorthandValue(borderLeftShorthand());
    case CSSPropertyBorderBlock:
      return BorderPropertyValue(borderBlockWidthShorthand(),
                                 borderBlockStyleShorthand(),
                                 borderBlockColorShorthand());
    case CSSPropertyBorderBlockColor:
      return Get2Values(borderBlockColorShorthand());
    case CSSPropertyBorderBlockStyle:
      return Get2Values(borderBlockStyleShorthand());
    case CSSPropertyBorderBlockWidth:
      return Get2Values(borderBlockWidthShorthand());
    case CSSPropertyBorderBlockStart:
      return GetShorthandValue(borderBlockStartShorthand());
    case CSSPropertyBorderBlockEnd:
      return GetShorthandValue(borderBlockEndShorthand());
    case CSSPropertyBorderInline:
      return BorderPropertyValue(borderInlineWidthShorthand(),
                                 borderInlineStyleShorthand(),
                                 borderInlineColorShorthand());
    case CSSPropertyBorderInlineColor:
      return Get2Values(borderInlineColorShorthand());
    case CSSPropertyBorderInlineStyle:
      return Get2Values(borderInlineStyleShorthand());
    case CSSPropertyBorderInlineWidth:
      return Get2Values(borderInlineWidthShorthand());
    case CSSPropertyBorderInlineStart:
      return GetShorthandValue(borderInlineStartShorthand());
    case CSSPropertyBorderInlineEnd:
      return GetShorthandValue(borderInlineEndShorthand());
    case CSSPropertyOutline:
      return GetShorthandValue(outlineShorthand());
    case CSSPropertyBorderColor:
      return Get4Values(borderColorShorthand());
    case CSSPropertyBorderWidth:
      return Get4Values(borderWidthShorthand());
    case CSSPropertyBorderStyle:
      return Get4Values(borderStyleShorthand());
    case CSSPropertyColumnRule:
      return GetShorthandValue(columnRuleShorthand());
    case CSSPropertyColumns:
      return GetShorthandValue(columnsShorthand());
    case CSSPropertyFlex:
      return GetShorthandValue(flexShorthand());
    case CSSPropertyFlexFlow:
      return GetShorthandValue(flexFlowShorthand());
    case CSSPropertyGridColumn:
      return GetShorthandValue(gridColumnShorthand(), " / ");
    case CSSPropertyGridRow:
      return GetShorthandValue(gridRowShorthand(), " / ");
    case CSSPropertyGridArea:
      return GetShorthandValue(gridAreaShorthand(), " / ");
    case CSSPropertyGap:
      return GetShorthandValue(gapShorthand());
    case CSSPropertyInset:
      return Get4Values(insetShorthand());
    case CSSPropertyInsetBlock:
      return Get2Values(insetBlockShorthand());
    case CSSPropertyInsetInline:
      return Get2Values(insetInlineShorthand());
    case CSSPropertyPlaceContent:
      return Get2Values(placeContentShorthand());
    case CSSPropertyPlaceItems:
      return Get2Values(placeItemsShorthand());
    case CSSPropertyPlaceSelf:
      return Get2Values(placeSelfShorthand());
    case CSSPropertyFont:
      return FontValue();
    case CSSPropertyFontVariant:
      return FontVariantValue();
    case CSSPropertyMargin:
      return Get4Values(marginShorthand());
    case CSSPropertyMarginBlock:
      return Get2Values(marginBlockShorthand());
    case CSSPropertyMarginInline:
      return Get2Values(marginInlineShorthand());
    case CSSPropertyOffset:
      return OffsetValue();
    case CSSPropertyWebkitMarginCollapse:
      return GetShorthandValue(webkitMarginCollapseShorthand());
    case CSSPropertyOverflow:
      return Get2Values(overflowShorthand());
    case CSSPropertyOverscrollBehavior:
      return GetShorthandValue(overscrollBehaviorShorthand());
    case CSSPropertyPadding:
      return Get4Values(paddingShorthand());
    case CSSPropertyPaddingBlock:
      return Get2Values(paddingBlockShorthand());
    case CSSPropertyPaddingInline:
      return Get2Values(paddingInlineShorthand());
    case CSSPropertyTextDecoration:
      return GetShorthandValue(textDecorationShorthand());
    case CSSPropertyTransition:
      return GetLayeredShorthandValue(transitionShorthand());
    case CSSPropertyListStyle:
      return GetShorthandValue(listStyleShorthand());
    case CSSPropertyWebkitMaskPosition:
      return GetLayeredShorthandValue(webkitMaskPositionShorthand());
    case CSSPropertyWebkitMaskRepeat:
      return GetLayeredShorthandValue(webkitMaskRepeatShorthand());
    case CSSPropertyWebkitMask:
      return GetLayeredShorthandValue(webkitMaskShorthand());
    case CSSPropertyWebkitTextEmphasis:
      return GetShorthandValue(webkitTextEmphasisShorthand());
    case CSSPropertyWebkitTextStroke:
      return GetShorthandValue(webkitTextStrokeShorthand());
    case CSSPropertyMarker: {
      if (const CSSValue* value =
              property_set_.GetPropertyCSSValue(GetCSSPropertyMarkerStart()))
        return value->CssText();
      return String();
    }
    case CSSPropertyBorderRadius:
      return Get4Values(borderRadiusShorthand());
    case CSSPropertyScrollPadding:
      return Get4Values(scrollPaddingShorthand());
    case CSSPropertyScrollPaddingBlock:
      return Get2Values(scrollPaddingBlockShorthand());
    case CSSPropertyScrollPaddingInline:
      return Get2Values(scrollPaddingInlineShorthand());
    case CSSPropertyScrollMargin:
      return Get4Values(scrollMarginShorthand());
    case CSSPropertyScrollMarginBlock:
      return Get2Values(scrollMarginBlockShorthand());
    case CSSPropertyScrollMarginInline:
      return Get2Values(scrollMarginInlineShorthand());
    case CSSPropertyPageBreakAfter:
      return PageBreakPropertyValue(pageBreakAfterShorthand());
    case CSSPropertyPageBreakBefore:
      return PageBreakPropertyValue(pageBreakBeforeShorthand());
    case CSSPropertyPageBreakInside:
      return PageBreakPropertyValue(pageBreakInsideShorthand());
    default:
      return String();
  }
}

// The font shorthand only allows keyword font-stretch values. Thus, we check if
// a percentage value can be parsed as a keyword, and if so, serialize it as
// that keyword.
const CSSValue* GetFontStretchKeyword(const CSSValue* font_stretch_value) {
  if (font_stretch_value->IsIdentifierValue())
    return font_stretch_value;
  if (font_stretch_value->IsPrimitiveValue()) {
    double value = ToCSSPrimitiveValue(font_stretch_value)->GetDoubleValue();
    if (value == 50)
      return CSSIdentifierValue::Create(CSSValueUltraCondensed);
    if (value == 62.5)
      return CSSIdentifierValue::Create(CSSValueExtraCondensed);
    if (value == 75)
      return CSSIdentifierValue::Create(CSSValueCondensed);
    if (value == 87.5)
      return CSSIdentifierValue::Create(CSSValueSemiCondensed);
    if (value == 100)
      return CSSIdentifierValue::Create(CSSValueNormal);
    if (value == 112.5)
      return CSSIdentifierValue::Create(CSSValueSemiExpanded);
    if (value == 125)
      return CSSIdentifierValue::Create(CSSValueExpanded);
    if (value == 150)
      return CSSIdentifierValue::Create(CSSValueExtraExpanded);
    if (value == 200)
      return CSSIdentifierValue::Create(CSSValueUltraExpanded);
  }
  return nullptr;
}

// Returns false if the value cannot be represented in the font shorthand
bool StylePropertySerializer::AppendFontLonghandValueIfNotNormal(
    const CSSProperty& property,
    StringBuilder& result) const {
  int found_property_index = property_set_.FindPropertyIndex(property);
  DCHECK_NE(found_property_index, -1);

  const CSSValue* val = property_set_.PropertyAt(found_property_index).Value();
  if (property.IDEquals(CSSPropertyFontStretch)) {
    const CSSValue* keyword = GetFontStretchKeyword(val);
    if (!keyword)
      return false;
    val = keyword;
  }
  if (val->IsIdentifierValue() &&
      ToCSSIdentifierValue(val)->GetValueID() == CSSValueNormal)
    return true;

  char prefix = '\0';
  switch (property.PropertyID()) {
    case CSSPropertyFontStyle:
      break;  // No prefix.
    case CSSPropertyFontFamily:
    case CSSPropertyFontStretch:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyFontVariantNumeric:
    case CSSPropertyFontVariantEastAsian:
    case CSSPropertyFontWeight:
      prefix = ' ';
      break;
    case CSSPropertyLineHeight:
      prefix = '/';
      break;
    default:
      NOTREACHED();
  }

  if (prefix && !result.IsEmpty())
    result.Append(prefix);

  String value;
  // In the font-variant shorthand a "none" ligatures value needs to be
  // expanded.
  if (property.IDEquals(CSSPropertyFontVariantLigatures) &&
      val->IsIdentifierValue() &&
      ToCSSIdentifierValue(val)->GetValueID() == CSSValueNone) {
    value =
        "no-common-ligatures no-discretionary-ligatures "
        "no-historical-ligatures no-contextual";
  } else {
    value = val->CssText();
  }

  result.Append(value);
  return true;
}

String StylePropertySerializer::FontValue() const {
  int font_size_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontSize());
  int font_family_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontFamily());
  int font_variant_caps_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontVariantCaps());
  int font_variant_ligatures_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontVariantLigatures());
  int font_variant_numeric_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontVariantNumeric());
  int font_variant_east_asian_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontVariantEastAsian());
  DCHECK_NE(font_size_property_index, -1);
  DCHECK_NE(font_family_property_index, -1);
  DCHECK_NE(font_variant_caps_property_index, -1);
  DCHECK_NE(font_variant_ligatures_property_index, -1);
  DCHECK_NE(font_variant_numeric_property_index, -1);
  DCHECK_NE(font_variant_east_asian_property_index, -1);

  PropertyValueForSerializer font_size_property =
      property_set_.PropertyAt(font_size_property_index);
  PropertyValueForSerializer font_family_property =
      property_set_.PropertyAt(font_family_property_index);
  PropertyValueForSerializer font_variant_caps_property =
      property_set_.PropertyAt(font_variant_caps_property_index);
  PropertyValueForSerializer font_variant_ligatures_property =
      property_set_.PropertyAt(font_variant_ligatures_property_index);
  PropertyValueForSerializer font_variant_numeric_property =
      property_set_.PropertyAt(font_variant_numeric_property_index);
  PropertyValueForSerializer font_variant_east_asian_property =
      property_set_.PropertyAt(font_variant_east_asian_property_index);

  // Check that non-initial font-variant subproperties are not conflicting with
  // this serialization.
  const CSSValue* ligatures_value = font_variant_ligatures_property.Value();
  const CSSValue* numeric_value = font_variant_numeric_property.Value();
  const CSSValue* east_asian_value = font_variant_east_asian_property.Value();
  if ((ligatures_value->IsIdentifierValue() &&
       ToCSSIdentifierValue(ligatures_value)->GetValueID() != CSSValueNormal) ||
      ligatures_value->IsValueList() ||
      (numeric_value->IsIdentifierValue() &&
       ToCSSIdentifierValue(numeric_value)->GetValueID() != CSSValueNormal) ||
      numeric_value->IsValueList() ||
      (east_asian_value->IsIdentifierValue() &&
       ToCSSIdentifierValue(east_asian_value)->GetValueID() !=
           CSSValueNormal) ||
      east_asian_value->IsValueList())
    return g_empty_string;

  StringBuilder result;
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontStyle(), result);

  const CSSValue* val = font_variant_caps_property.Value();
  if (val->IsIdentifierValue() &&
      (ToCSSIdentifierValue(val)->GetValueID() != CSSValueSmallCaps &&
       ToCSSIdentifierValue(val)->GetValueID() != CSSValueNormal))
    return g_empty_string;
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontVariantCaps(), result);

  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontWeight(), result);
  bool font_stretch_valid =
      AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontStretch(), result);
  if (!font_stretch_valid)
    return String();
  if (!result.IsEmpty())
    result.Append(' ');
  result.Append(font_size_property.Value()->CssText());
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyLineHeight(), result);
  if (!result.IsEmpty())
    result.Append(' ');
  result.Append(font_family_property.Value()->CssText());
  return result.ToString();
}

String StylePropertySerializer::FontVariantValue() const {
  StringBuilder result;

  // TODO(drott): Decide how we want to return ligature values in shorthands,
  // reduced to "none" or spelled out, filed as W3C bug:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=29594
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontVariantLigatures(),
                                     result);
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontVariantCaps(), result);
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontVariantNumeric(),
                                     result);
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontVariantEastAsian(),
                                     result);

  if (result.IsEmpty()) {
    return "normal";
  }

  return result.ToString();
}

String StylePropertySerializer::OffsetValue() const {
  StringBuilder result;
  if (RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled()) {
    const CSSValue* position =
        property_set_.GetPropertyCSSValue(GetCSSPropertyOffsetPosition());
    if (!position->IsInitialValue()) {
      result.Append(position->CssText());
    }
  }
  const CSSValue* path =
      property_set_.GetPropertyCSSValue(GetCSSPropertyOffsetPath());
  const CSSValue* distance =
      property_set_.GetPropertyCSSValue(GetCSSPropertyOffsetDistance());
  const CSSValue* rotate =
      property_set_.GetPropertyCSSValue(GetCSSPropertyOffsetRotate());
  if (!path->IsInitialValue()) {
    if (!result.IsEmpty())
      result.Append(" ");
    result.Append(path->CssText());
    if (!distance->IsInitialValue()) {
      result.Append(" ");
      result.Append(distance->CssText());
    }
    if (!rotate->IsInitialValue()) {
      result.Append(" ");
      result.Append(rotate->CssText());
    }
  } else {
    DCHECK(distance->IsInitialValue());
    DCHECK(rotate->IsInitialValue());
  }
  if (RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled()) {
    const CSSValue* anchor =
        property_set_.GetPropertyCSSValue(GetCSSPropertyOffsetAnchor());
    if (!anchor->IsInitialValue()) {
      result.Append(" / ");
      result.Append(anchor->CssText());
    }
  }
  return result.ToString();
}

String StylePropertySerializer::Get2Values(
    const StylePropertyShorthand& shorthand) const {
  // Assume the properties are in the usual order start, end.
  int start_value_index =
      property_set_.FindPropertyIndex(*shorthand.properties()[0]);
  int end_value_index =
      property_set_.FindPropertyIndex(*shorthand.properties()[1]);

  if (start_value_index == -1 || end_value_index == -1)
    return String();

  PropertyValueForSerializer start =
      property_set_.PropertyAt(start_value_index);
  PropertyValueForSerializer end = property_set_.PropertyAt(end_value_index);

  bool show_end = !DataEquivalent(start.Value(), end.Value());

  StringBuilder result;
  result.Append(start.Value()->CssText());
  if (show_end) {
    result.Append(' ');
    result.Append(end.Value()->CssText());
  }
  return result.ToString();
}

String StylePropertySerializer::Get4Values(
    const StylePropertyShorthand& shorthand) const {
  // Assume the properties are in the usual order top, right, bottom, left.
  int top_value_index =
      property_set_.FindPropertyIndex(*shorthand.properties()[0]);
  int right_value_index =
      property_set_.FindPropertyIndex(*shorthand.properties()[1]);
  int bottom_value_index =
      property_set_.FindPropertyIndex(*shorthand.properties()[2]);
  int left_value_index =
      property_set_.FindPropertyIndex(*shorthand.properties()[3]);

  if (top_value_index == -1 || right_value_index == -1 ||
      bottom_value_index == -1 || left_value_index == -1)
    return String();

  PropertyValueForSerializer top = property_set_.PropertyAt(top_value_index);
  PropertyValueForSerializer right =
      property_set_.PropertyAt(right_value_index);
  PropertyValueForSerializer bottom =
      property_set_.PropertyAt(bottom_value_index);
  PropertyValueForSerializer left = property_set_.PropertyAt(left_value_index);

  bool show_left = !DataEquivalent(right.Value(), left.Value());
  bool show_bottom = !DataEquivalent(top.Value(), bottom.Value()) || show_left;
  bool show_right = !DataEquivalent(top.Value(), right.Value()) || show_bottom;

  StringBuilder result;
  result.Append(top.Value()->CssText());
  if (show_right) {
    result.Append(' ');
    result.Append(right.Value()->CssText());
  }
  if (show_bottom) {
    result.Append(' ');
    result.Append(bottom.Value()->CssText());
  }
  if (show_left) {
    result.Append(' ');
    result.Append(left.Value()->CssText());
  }
  return result.ToString();
}

String StylePropertySerializer::GetLayeredShorthandValue(
    const StylePropertyShorthand& shorthand) const {
  const unsigned size = shorthand.length();

  // Begin by collecting the properties into a vector.
  HeapVector<Member<const CSSValue>> values(size);
  // If the below loop succeeds, there should always be at minimum 1 layer.
  wtf_size_t num_layers = 1U;

  // TODO(timloh): Shouldn't we fail if the lists are differently sized, with
  // the exception of background-color?
  for (unsigned i = 0; i < size; i++) {
    values[i] = property_set_.GetPropertyCSSValue(*shorthand.properties()[i]);
    if (values[i]->IsBaseValueList()) {
      const CSSValueList* value_list = ToCSSValueList(values[i]);
      num_layers = std::max(num_layers, value_list->length());
    }
  }

  StringBuilder result;

  // Now stitch the properties together.
  for (wtf_size_t layer = 0; layer < num_layers; layer++) {
    StringBuilder layer_result;
    bool use_repeat_x_shorthand = false;
    bool use_repeat_y_shorthand = false;
    bool use_single_word_shorthand = false;
    bool found_position_xcss_property = false;
    bool found_position_ycss_property = false;

    for (unsigned property_index = 0; property_index < size; property_index++) {
      const CSSValue* value = nullptr;
      const CSSProperty* property = shorthand.properties()[property_index];

      // Get a CSSValue for this property and layer.
      if (values[property_index]->IsBaseValueList()) {
        const CSSValueList* property_values =
            ToCSSValueList(values[property_index]);
        // There might not be an item for this layer for this property.
        if (layer < property_values->length())
          value = &property_values->Item(layer);
      } else if ((layer == 0 &&
                  !property->IDEquals(CSSPropertyBackgroundColor)) ||
                 (layer == num_layers - 1 &&
                  property->IDEquals(CSSPropertyBackgroundColor))) {
        // Singletons except background color belong in the 0th layer.
        // Background color belongs in the last layer.
        value = values[property_index];
      }
      // No point proceeding if there's not a value to look at.
      if (!value)
        continue;

      // Special case for background-repeat.
      if (property->IDEquals(CSSPropertyBackgroundRepeatX) ||
          property->IDEquals(CSSPropertyWebkitMaskRepeatX)) {
        DCHECK(shorthand.properties()[property_index + 1]->IDEquals(
                   CSSPropertyBackgroundRepeatY) ||
               shorthand.properties()[property_index + 1]->IDEquals(
                   CSSPropertyWebkitMaskRepeatY));
        const CSSValue& y_value =
            values[property_index + 1]->IsValueList()
                ? ToCSSValueList(values[property_index + 1])->Item(layer)
                : *values[property_index + 1];

        // FIXME: At some point we need to fix this code to avoid returning an
        // invalid shorthand, since some longhand combinations are not
        // serializable into a single shorthand.
        if (!value->IsIdentifierValue() || !y_value.IsIdentifierValue())
          continue;

        CSSValueID x_id = ToCSSIdentifierValue(value)->GetValueID();
        CSSValueID y_id = ToCSSIdentifierValue(y_value).GetValueID();
        // Maybe advance propertyIndex to look at the next CSSValue in the list
        // for the checks below.
        if (x_id == y_id) {
          use_single_word_shorthand = true;
          property = shorthand.properties()[++property_index];
        } else if (x_id == CSSValueRepeat && y_id == CSSValueNoRepeat) {
          use_repeat_x_shorthand = true;
          property = shorthand.properties()[++property_index];
        } else if (x_id == CSSValueNoRepeat && y_id == CSSValueRepeat) {
          use_repeat_y_shorthand = true;
          property = shorthand.properties()[++property_index];
        }
      }

      if (!value->IsInitialValue()) {
        if (property->IDEquals(CSSPropertyBackgroundSize) ||
            property->IDEquals(CSSPropertyWebkitMaskSize)) {
          if (found_position_ycss_property || found_position_xcss_property)
            layer_result.Append(" / ");
          else
            layer_result.Append(" 0% 0% / ");
        } else if (!layer_result.IsEmpty()) {
          // Do this second to avoid ending up with an extra space in the output
          // if we hit the continue above.
          layer_result.Append(' ');
        }

        if (use_repeat_x_shorthand) {
          use_repeat_x_shorthand = false;
          layer_result.Append(getValueName(CSSValueRepeatX));
        } else if (use_repeat_y_shorthand) {
          use_repeat_y_shorthand = false;
          layer_result.Append(getValueName(CSSValueRepeatY));
        } else {
          if (use_single_word_shorthand)
            use_single_word_shorthand = false;
          layer_result.Append(value->CssText());
        }
        if (property->IDEquals(CSSPropertyBackgroundPositionX) ||
            property->IDEquals(CSSPropertyWebkitMaskPositionX))
          found_position_xcss_property = true;
        if (property->IDEquals(CSSPropertyBackgroundPositionY) ||
            property->IDEquals(CSSPropertyWebkitMaskPositionY)) {
          found_position_ycss_property = true;
          // background-position is a special case. If only the first offset is
          // specified, the second one defaults to "center", not the same value.
        }
      }
    }
    if (!layer_result.IsEmpty()) {
      if (!result.IsEmpty())
        result.Append(", ");
      result.Append(layer_result);
    }
  }

  return result.ToString();
}

String StylePropertySerializer::GetShorthandValue(
    const StylePropertyShorthand& shorthand,
    String separator) const {
  StringBuilder result;
  for (unsigned i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value =
        property_set_.GetPropertyCSSValue(*shorthand.properties()[i]);
    String value_text = value->CssText();
    if (value->IsInitialValue())
      continue;
    if (!result.IsEmpty())
      result.Append(separator);
    result.Append(value_text);
  }
  return result.ToString();
}

// only returns a non-null value if all properties have the same, non-null value
String StylePropertySerializer::GetCommonValue(
    const StylePropertyShorthand& shorthand) const {
  String res;
  for (unsigned i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value =
        property_set_.GetPropertyCSSValue(*shorthand.properties()[i]);
    // FIXME: CSSInitialValue::CssText should generate the right value.
    String text = value->CssText();
    if (res.IsNull())
      res = text;
    else if (res != text)
      return String();
  }
  return res;
}

String StylePropertySerializer::BorderPropertyValue(
    const StylePropertyShorthand& width,
    const StylePropertyShorthand& style,
    const StylePropertyShorthand& color) const {
  const StylePropertyShorthand properties[3] = {width, style, color};
  StringBuilder result;
  for (size_t i = 0; i < arraysize(properties); ++i) {
    String value = GetCommonValue(properties[i]);
    if (value.IsNull())
      return String();
    if (value == "initial")
      continue;
    if (!result.IsEmpty())
      result.Append(' ');
    result.Append(value);
  }
  return result.IsEmpty() ? String() : result.ToString();
}

String StylePropertySerializer::BorderImagePropertyValue() const {
  StringBuilder result;
  const CSSProperty* properties[] = {
      &GetCSSPropertyBorderImageSource(), &GetCSSPropertyBorderImageSlice(),
      &GetCSSPropertyBorderImageWidth(), &GetCSSPropertyBorderImageOutset(),
      &GetCSSPropertyBorderImageRepeat()};
  size_t length = arraysize(properties);
  for (size_t i = 0; i < length; ++i) {
    const CSSValue& value = *property_set_.GetPropertyCSSValue(*properties[i]);
    if (!result.IsEmpty())
      result.Append(" ");
    if (i == 2 || i == 3)
      result.Append("/ ");
    result.Append(value.CssText());
  }
  return result.ToString();
}

static void AppendBackgroundRepeatValue(StringBuilder& builder,
                                        const CSSValue& repeat_xcss_value,
                                        const CSSValue& repeat_ycss_value) {
  // FIXME: Ensure initial values do not appear in CSS_VALUE_LISTS.
  DEFINE_STATIC_LOCAL(Persistent<CSSIdentifierValue>, initial_repeat_value,
                      (CSSIdentifierValue::Create(CSSValueRepeat)));
  const CSSIdentifierValue& repeat_x =
      repeat_xcss_value.IsInitialValue()
          ? *initial_repeat_value
          : ToCSSIdentifierValue(repeat_xcss_value);
  const CSSIdentifierValue& repeat_y =
      repeat_ycss_value.IsInitialValue()
          ? *initial_repeat_value
          : ToCSSIdentifierValue(repeat_ycss_value);
  CSSValueID repeat_x_value_id = repeat_x.GetValueID();
  CSSValueID repeat_y_value_id = repeat_y.GetValueID();
  if (repeat_x_value_id == repeat_y_value_id) {
    builder.Append(repeat_x.CssText());
  } else if (repeat_x_value_id == CSSValueNoRepeat &&
             repeat_y_value_id == CSSValueRepeat) {
    builder.Append("repeat-y");
  } else if (repeat_x_value_id == CSSValueRepeat &&
             repeat_y_value_id == CSSValueNoRepeat) {
    builder.Append("repeat-x");
  } else {
    builder.Append(repeat_x.CssText());
    builder.Append(' ');
    builder.Append(repeat_y.CssText());
  }
}

String StylePropertySerializer::BackgroundRepeatPropertyValue() const {
  const CSSValue& repeat_x =
      *property_set_.GetPropertyCSSValue(GetCSSPropertyBackgroundRepeatX());
  const CSSValue& repeat_y =
      *property_set_.GetPropertyCSSValue(GetCSSPropertyBackgroundRepeatY());

  const CSSValueList* repeat_x_list = nullptr;
  int repeat_x_length = 1;
  if (repeat_x.IsValueList()) {
    repeat_x_list = &ToCSSValueList(repeat_x);
    repeat_x_length = repeat_x_list->length();
  } else if (!repeat_x.IsIdentifierValue()) {
    return String();
  }

  const CSSValueList* repeat_y_list = nullptr;
  int repeat_y_length = 1;
  if (repeat_y.IsValueList()) {
    repeat_y_list = &ToCSSValueList(repeat_y);
    repeat_y_length = repeat_y_list->length();
  } else if (!repeat_y.IsIdentifierValue()) {
    return String();
  }

  size_t shorthand_length =
      lowestCommonMultiple(repeat_x_length, repeat_y_length);
  StringBuilder builder;
  for (size_t i = 0; i < shorthand_length; ++i) {
    if (i)
      builder.Append(", ");

    const CSSValue& x_value =
        repeat_x_list ? repeat_x_list->Item(i % repeat_x_list->length())
                      : repeat_x;
    const CSSValue& y_value =
        repeat_y_list ? repeat_y_list->Item(i % repeat_y_list->length())
                      : repeat_y;
    AppendBackgroundRepeatValue(builder, x_value, y_value);
  }
  return builder.ToString();
}

String StylePropertySerializer::PageBreakPropertyValue(
    const StylePropertyShorthand& shorthand) const {
  const CSSValue* value =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[0]);
  CSSValueID value_id = ToCSSIdentifierValue(value)->GetValueID();
  // https://drafts.csswg.org/css-break/#page-break-properties
  if (value_id == CSSValuePage)
    return "always";
  if (value_id == CSSValueAuto || value_id == CSSValueLeft ||
      value_id == CSSValueRight || value_id == CSSValueAvoid)
    return value->CssText();
  return String();
}

}  // namespace blink
