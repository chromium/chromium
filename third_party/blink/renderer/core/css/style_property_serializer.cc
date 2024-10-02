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

#include "base/logging.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/animation/css/css_animation_data.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_pending_system_font_value.h"
#include "third_party/blink/renderer/core/css/css_repeat_style_value.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/core/css/cssom_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/css_property_instances.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/css_to_style_map.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

template <typename T>
T ConvertIdentifierTo(const CSSValue* value, T initial_value) {
  if (const auto* ident = DynamicTo<CSSIdentifierValue>(value)) {
    return ident->ConvertTo<T>();
  }
  DCHECK(value->IsInitialValue());
  return initial_value;
}

inline WhiteSpaceCollapse ToWhiteSpaceCollapse(const CSSValue* value) {
  return ConvertIdentifierTo<WhiteSpaceCollapse>(
      value, ComputedStyleInitialValues::InitialWhiteSpaceCollapse());
}

inline TextWrapMode ToTextWrapMode(const CSSValue* value) {
  return ConvertIdentifierTo<TextWrapMode>(
      value, ComputedStyleInitialValues::InitialTextWrapMode());
}

inline TextWrapStyle ToTextWrapStyle(const CSSValue* value) {
  return ConvertIdentifierTo<TextWrapStyle>(
      value, ComputedStyleInitialValues::InitialTextWrapStyle());
}

bool IsZeroPercent(const CSSValue* value) {
  if (const auto* num = DynamicTo<CSSNumericLiteralValue>(value)) {
    return num->IsZero() == CSSPrimitiveValue::BoolStatus::kTrue &&
           num->IsPercentage();
  }

  return false;
}

}  // namespace

StylePropertySerializer::CSSPropertyValueSetForSerializer::
    CSSPropertyValueSetForSerializer(const CSSPropertyValueSet& properties)
    : property_set_(&properties),
      all_index_(property_set_->FindPropertyIndex(CSSPropertyID::kAll)),
      need_to_expand_all_(false) {
  if (!HasAllProperty()) {
    return;
  }

  CSSPropertyValueSet::PropertyReference all_property =
      property_set_->PropertyAt(all_index_);
  for (unsigned i = 0; i < property_set_->PropertyCount(); ++i) {
    CSSPropertyValueSet::PropertyReference property =
        property_set_->PropertyAt(i);
    if (property.IsAffectedByAll()) {
      if (all_property.IsImportant() && !property.IsImportant()) {
        continue;
      }
      if (static_cast<unsigned>(all_index_) >= i) {
        continue;
      }
      if (property.Value() == all_property.Value() &&
          property.IsImportant() == all_property.IsImportant()) {
        continue;
      }
      need_to_expand_all_ = true;
    }
    if (!IsCSSPropertyIDWithName(property.Id())) {
      continue;
    }
    longhand_property_used_.set(GetCSSPropertyIDIndex(property.Id()));
  }
}

void StylePropertySerializer::CSSPropertyValueSetForSerializer::Trace(
    blink::Visitor* visitor) const {
  visitor->Trace(property_set_);
}

unsigned
StylePropertySerializer::CSSPropertyValueSetForSerializer::PropertyCount()
    const {
  unsigned count = property_set_->PropertyCount();
  if (HasExpandedAllProperty()) {
    // When expanding all:* we need to serialize all properties set by the "all"
    // property, but also still walk the actual property set to include any
    // custom property declarations.
    count += kIntLastCSSProperty - kIntFirstCSSProperty + 1;
  }
  return count;
}

StylePropertySerializer::PropertyValueForSerializer
StylePropertySerializer::CSSPropertyValueSetForSerializer::PropertyAt(
    unsigned index) const {
  if (IsIndexInPropertySet(index)) {
    return StylePropertySerializer::PropertyValueForSerializer(
        property_set_->PropertyAt(index));
  }

  // When expanding "all" into longhands, PropertyAt() is called with indices
  // outside the size of the property_set_ to serialize all longshands.
  DCHECK(HasExpandedAllProperty());
  CSSPropertyID property_id = IndexToPropertyID(index);
  DCHECK(IsCSSPropertyIDWithName(property_id));
  if (longhand_property_used_.test(GetCSSPropertyIDIndex(property_id))) {
    // A property declaration for property_id overrides the "all" declaration.
    // Access that declaration from the property set.
    int real_index = property_set_->FindPropertyIndex(property_id);
    DCHECK_NE(real_index, -1);
    return StylePropertySerializer::PropertyValueForSerializer(
        property_set_->PropertyAt(real_index));
  }

  CSSPropertyValueSet::PropertyReference property =
      property_set_->PropertyAt(all_index_);
  return StylePropertySerializer::PropertyValueForSerializer(
      CSSProperty::Get(property_id).GetCSSPropertyName(), &property.Value(),
      property.IsImportant());
}

bool StylePropertySerializer::CSSPropertyValueSetForSerializer::
    ShouldProcessPropertyAt(unsigned index) const {
  // CSSPropertyValueSet has all valid longhands. We should process.
  if (!HasAllProperty()) {
    return true;
  }

  // If all is not expanded, we need to process "all" and properties which
  // are not overwritten by "all".
  if (!need_to_expand_all_) {
    CSSPropertyValueSet::PropertyReference property =
        property_set_->PropertyAt(index);
    if (property.Id() == CSSPropertyID::kAll || !property.IsAffectedByAll()) {
      return true;
    }
    if (!IsCSSPropertyIDWithName(property.Id())) {
      return false;
    }
    return longhand_property_used_.test(GetCSSPropertyIDIndex(property.Id()));
  }

  // Custom property declarations are never overridden by "all" and are only
  // traversed for the indices into the property set.
  if (IsIndexInPropertySet(index)) {
    return property_set_->PropertyAt(index).Id() == CSSPropertyID::kVariable;
  }

  CSSPropertyID property_id = IndexToPropertyID(index);
  DCHECK(IsCSSPropertyIDWithName(property_id));
  const CSSProperty& property_class =
      CSSProperty::Get(ResolveCSSPropertyID(property_id));

  // Since "all" is expanded, we don't need to process "all".
  // We should not process expanded shorthands (e.g. font, background,
  // and so on) either.
  if (property_class.IsShorthand() ||
      property_class.IDEquals(CSSPropertyID::kAll)) {
    return false;
  }

  // The all property is a shorthand that resets all CSS properties except
  // direction and unicode-bidi. It only accepts the CSS-wide keywords.
  // c.f. https://drafts.csswg.org/css-cascade/#all-shorthand
  if (!property_class.IsAffectedByAll()) {
    return longhand_property_used_.test(GetCSSPropertyIDIndex(property_id));
  }

  return true;
}

int StylePropertySerializer::CSSPropertyValueSetForSerializer::
    FindPropertyIndex(const CSSProperty& property) const {
  CSSPropertyID property_id = property.PropertyID();
  if (!HasExpandedAllProperty()) {
    return property_set_->FindPropertyIndex(property_id);
  }
  return GetCSSPropertyIDIndex(property_id) + property_set_->PropertyCount();
}

const CSSValue*
StylePropertySerializer::CSSPropertyValueSetForSerializer::GetPropertyCSSValue(
    const CSSProperty& property) const {
  int index = FindPropertyIndex(property);
  if (index == -1) {
    return nullptr;
  }
  StylePropertySerializer::PropertyValueForSerializer value = PropertyAt(index);
  return value.Value();
}

bool StylePropertySerializer::CSSPropertyValueSetForSerializer::
    IsDescriptorContext() const {
  return property_set_->CssParserMode() == kCSSFontFaceRuleMode;
}

StylePropertySerializer::StylePropertySerializer(
    const CSSPropertyValueSet& properties)
    : property_set_(properties) {}

String StylePropertySerializer::GetCustomPropertyText(
    const PropertyValueForSerializer& property,
    bool is_not_first_decl) const {
  DCHECK_EQ(property.Name().Id(), CSSPropertyID::kVariable);
  StringBuilder result;
  if (is_not_first_decl) {
    result.Append(' ');
  }
  const CSSValue* value = property.Value();
  SerializeIdentifier(property.Name().ToAtomicString(), result,
                      is_not_first_decl);
  result.Append(": ");
  result.Append(value->CssText());
  if (property.IsImportant()) {
    result.Append(" !important");
  }
  result.Append(';');
  return result.ReleaseString();
}

String StylePropertySerializer::GetPropertyText(const CSSPropertyName& name,
                                                const String& value,
                                                bool is_important,
                                                bool is_not_first_decl) const {
  StringBuilder result;
  if (is_not_first_decl) {
    result.Append(' ');
  }
  result.Append(name.ToAtomicString());
  result.Append(": ");
  result.Append(value);
  if (is_important) {
    result.Append(" !important");
  }
  result.Append(';');
  return result.ReleaseString();
}

String StylePropertySerializer::AsText() const {
  StringBuilder result;

  std::bitset<kNumCSSPropertyIDs> longhand_serialized;
  std::bitset<kNumCSSPropertyIDs> shorthand_appeared;

  unsigned size = property_set_.PropertyCount();
  unsigned num_decls = 0;
  for (unsigned n = 0; n < size; ++n) {
    if (!property_set_.ShouldProcessPropertyAt(n)) {
      continue;
    }

    StylePropertySerializer::PropertyValueForSerializer property =
        property_set_.PropertyAt(n);

    const CSSPropertyName& name = property.Name();
    CSSPropertyID property_id = name.Id();

#if DCHECK_IS_ON()
    if (property_id != CSSPropertyID::kVariable) {
      const CSSProperty& property_class = CSSProperty::Get(property_id);
      // Only web exposed properties should be part of the style.
      DCHECK(property_class.IsWebExposed());
      // All shorthand properties should have been expanded at parse time.
      DCHECK(property_set_.IsDescriptorContext() ||
             (property_class.IsProperty() && !property_class.IsShorthand()));
      DCHECK(!property_set_.IsDescriptorContext() ||
             property_class.IsDescriptor());
    }
#endif  // DCHECK_IS_ON()

    switch (property_id) {
      case CSSPropertyID::kVariable:
        result.Append(GetCustomPropertyText(property, num_decls++));
        continue;
      case CSSPropertyID::kAll:
        result.Append(GetPropertyText(name, property.Value()->CssText(),
                                      property.IsImportant(), num_decls++));
        continue;
      default:
        break;
    }
    if (longhand_serialized.test(GetCSSPropertyIDIndex(property_id))) {
      continue;
    }

    Vector<StylePropertyShorthand, 4> shorthands;
    getMatchingShorthandsForLonghand(property_id, &shorthands);
    bool serialized_as_shorthand = false;
    for (const StylePropertyShorthand& shorthand : shorthands) {
      // Some aliases are implemented as a shorthand, in which case
      // we prefer to not use the shorthand.
      if (shorthand.length() == 1) {
        continue;
      }

      CSSPropertyID shorthand_property = shorthand.id();
      int shorthand_property_index = GetCSSPropertyIDIndex(shorthand_property);
      // We already tried serializing as this shorthand
      if (shorthand_appeared.test(shorthand_property_index)) {
        continue;
      }

      shorthand_appeared.set(shorthand_property_index);
      bool serialized_other_longhand = false;
      for (const CSSProperty* const longhand : shorthand.properties()) {
        if (longhand_serialized.test(
                GetCSSPropertyIDIndex(longhand->PropertyID()))) {
          serialized_other_longhand = true;
          break;
        }
      }
      if (serialized_other_longhand) {
        continue;
      }

      String shorthand_result = SerializeShorthand(shorthand_property);
      if (shorthand_result.empty()) {
        continue;
      }

      result.Append(GetPropertyText(
          CSSProperty::Get(shorthand_property).GetCSSPropertyName(),
          shorthand_result, property.IsImportant(), num_decls++));
      serialized_as_shorthand = true;
      for (const CSSProperty* const longhand : shorthand.properties()) {
        longhand_serialized.set(GetCSSPropertyIDIndex(longhand->PropertyID()));
      }
      break;
    }

    if (serialized_as_shorthand) {
      continue;
    }

    result.Append(GetPropertyText(name, property.Value()->CssText(),
                                  property.IsImportant(), num_decls++));
  }

  DCHECK(!num_decls ^ !result.empty());
  return result.ReleaseString();
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
    case CSSPropertyID::kBackground:
    case CSSPropertyID::kBorder:
    case CSSPropertyID::kBorderTop:
    case CSSPropertyID::kBorderRight:
    case CSSPropertyID::kBorderBottom:
    case CSSPropertyID::kBorderLeft:
    case CSSPropertyID::kBorderBlockStart:
    case CSSPropertyID::kBorderBlockEnd:
    case CSSPropertyID::kBorderInlineStart:
    case CSSPropertyID::kBorderInlineEnd:
    case CSSPropertyID::kBorderBlock:
    case CSSPropertyID::kBorderInline:
    case CSSPropertyID::kOutline:
    case CSSPropertyID::kColumnRule:
    case CSSPropertyID::kColumns:
    case CSSPropertyID::kGridColumn:
    case CSSPropertyID::kGridRow:
    case CSSPropertyID::kGridArea:
    case CSSPropertyID::kGap:
    case CSSPropertyID::kListStyle:
    case CSSPropertyID::kTextDecoration:
    case CSSPropertyID::kTextEmphasis:
    case CSSPropertyID::kTextWrap:
    case CSSPropertyID::kMask:
    case CSSPropertyID::kWebkitTextStroke:
    case CSSPropertyID::kWhiteSpace:
      return true;
    default:
      return false;
  }
}

String StylePropertySerializer::CommonShorthandChecks(
    const StylePropertyShorthand& shorthand) const {
  unsigned longhand_count = shorthand.length();
  if (!longhand_count || longhand_count > kMaxShorthandExpansion) {
    NOTREACHED_IN_MIGRATION();
    return g_empty_string;
  }

  std::array<const CSSValue*, kMaxShorthandExpansion> longhands;

  bool has_important = false;
  bool has_non_important = false;

  for (unsigned i = 0; i < longhand_count; i++) {
    int index = property_set_.FindPropertyIndex(*shorthand.properties()[i]);
    if (index == -1) {
      return g_empty_string;
    }
    PropertyValueForSerializer value = property_set_.PropertyAt(index);

    has_important |= value.IsImportant();
    has_non_important |= !value.IsImportant();
    longhands[i] = value.Value();
  }

  if (has_important && has_non_important) {
    return g_empty_string;
  }

  if (longhands[0]->IsCSSWideKeyword() ||
      longhands[0]->IsPendingSubstitutionValue()) {
    bool success = true;
    for (unsigned i = 1; i < longhand_count; i++) {
      if (!base::ValuesEquivalent(longhands[i], longhands[0])) {
        // This should just return emptyString but some shorthands currently
        // allow 'initial' for their longhands.
        success = false;
        break;
      }
    }
    if (success) {
      if (const auto* substitution_value =
              DynamicTo<cssvalue::CSSPendingSubstitutionValue>(longhands[0])) {
        if (substitution_value->ShorthandPropertyId() != shorthand.id()) {
          return g_empty_string;
        }
        return substitution_value->ShorthandValue()->CssText();
      }
      return longhands[0]->CssText();
    }
  }

  bool allow_initial = AllowInitialInShorthand(shorthand.id());
  for (unsigned i = 0; i < longhand_count; i++) {
    const CSSValue& value = *longhands[i];
    if (!allow_initial && value.IsInitialValue()) {
      return g_empty_string;
    }
    if ((value.IsCSSWideKeyword() && !value.IsInitialValue()) ||
        value.IsPendingSubstitutionValue()) {
      return g_empty_string;
    }
    if (value.IsUnparsedDeclaration()) {
      return g_empty_string;
    }
  }

  return String();
}

String StylePropertySerializer::SerializeShorthand(
    CSSPropertyID property_id) const {
  const StylePropertyShorthand& shorthand = shorthandForProperty(property_id);
  DCHECK(shorthand.length());

  String result = CommonShorthandChecks(shorthand);
  if (!result.IsNull()) {
    return result;
  }

  switch (property_id) {
    case CSSPropertyID::kAnimation:
      return GetLayeredShorthandValue(animationShorthand());
    case CSSPropertyID::kAlternativeAnimationWithTimeline:
      return GetLayeredShorthandValue(
          alternativeAnimationWithTimelineShorthand());
    case CSSPropertyID::kAnimationRange:
      return AnimationRangeShorthandValue();
    case CSSPropertyID::kBorderSpacing:
      return Get2Values(borderSpacingShorthand());
    case CSSPropertyID::kBackgroundPosition:
      return GetLayeredShorthandValue(backgroundPositionShorthand());
    case CSSPropertyID::kBackground:
      return GetLayeredShorthandValue(backgroundShorthand());
    case CSSPropertyID::kBorder:
      return BorderPropertyValue(borderWidthShorthand(), borderStyleShorthand(),
                                 borderColorShorthand());
    case CSSPropertyID::kBorderImage:
      return BorderImagePropertyValue();
    case CSSPropertyID::kBorderTop:
      return GetShorthandValue(borderTopShorthand());
    case CSSPropertyID::kBorderRight:
      return GetShorthandValue(borderRightShorthand());
    case CSSPropertyID::kBorderBottom:
      return GetShorthandValue(borderBottomShorthand());
    case CSSPropertyID::kBorderLeft:
      return GetShorthandValue(borderLeftShorthand());
    case CSSPropertyID::kBorderBlock:
      return BorderPropertyValue(borderBlockWidthShorthand(),
                                 borderBlockStyleShorthand(),
                                 borderBlockColorShorthand());
    case CSSPropertyID::kBorderBlockColor:
      return Get2Values(borderBlockColorShorthand());
    case CSSPropertyID::kBorderBlockStyle:
      return Get2Values(borderBlockStyleShorthand());
    case CSSPropertyID::kBorderBlockWidth:
      return Get2Values(borderBlockWidthShorthand());
    case CSSPropertyID::kBorderBlockStart:
      return GetShorthandValue(borderBlockStartShorthand());
    case CSSPropertyID::kBorderBlockEnd:
      return GetShorthandValue(borderBlockEndShorthand());
    case CSSPropertyID::kBorderInline:
      return BorderPropertyValue(borderInlineWidthShorthand(),
                                 borderInlineStyleShorthand(),
                                 borderInlineColorShorthand());
    case CSSPropertyID::kBorderInlineColor:
      return Get2Values(borderInlineColorShorthand());
    case CSSPropertyID::kBorderInlineStyle:
      return Get2Values(borderInlineStyleShorthand());
    case CSSPropertyID::kBorderInlineWidth:
      return Get2Values(borderInlineWidthShorthand());
    case CSSPropertyID::kBorderInlineStart:
      return GetShorthandValue(borderInlineStartShorthand());
    case CSSPropertyID::kBorderInlineEnd:
      return GetShorthandValue(borderInlineEndShorthand());
    case CSSPropertyID::kContainer:
      return ContainerValue();
    case CSSPropertyID::kOutline:
      return GetShorthandValue(outlineShorthand());
    case CSSPropertyID::kBorderColor:
      return Get4Values(borderColorShorthand());
    case CSSPropertyID::kBorderWidth:
      return Get4Values(borderWidthShorthand());
    case CSSPropertyID::kBorderStyle:
      return Get4Values(borderStyleShorthand());
    case CSSPropertyID::kColumnRule:
      return GetShorthandValueForColumnRule(columnRuleShorthand());
    case CSSPropertyID::kColumns:
      return GetShorthandValueForColumns(columnsShorthand());
    case CSSPropertyID::kContainIntrinsicSize:
      return ContainIntrinsicSizeValue();
    case CSSPropertyID::kFlex:
      return GetShorthandValue(flexShorthand());
    case CSSPropertyID::kFlexFlow:
      return GetShorthandValueForDoubleBarCombinator(flexFlowShorthand());
    case CSSPropertyID::kGrid:
      return GetShorthandValueForGrid(gridShorthand());
    case CSSPropertyID::kGridTemplate:
      return GetShorthandValueForGridTemplate(gridTemplateShorthand());
    case CSSPropertyID::kGridColumn:
      return GetShorthandValueForGridLine(gridColumnShorthand());
    case CSSPropertyID::kGridRow:
      return GetShorthandValueForGridLine(gridRowShorthand());
    case CSSPropertyID::kGridArea:
      return GetShorthandValueForGridArea(gridAreaShorthand());
    case CSSPropertyID::kGap:
      return Get2Values(gapShorthand());
    case CSSPropertyID::kInset:
      return Get4Values(insetShorthand());
    case CSSPropertyID::kInsetBlock:
      return Get2Values(insetBlockShorthand());
    case CSSPropertyID::kInsetInline:
      return Get2Values(insetInlineShorthand());
    case CSSPropertyID::kPlaceContent:
      return Get2Values(placeContentShorthand());
    case CSSPropertyID::kPlaceItems:
      return Get2Values(placeItemsShorthand());
    case CSSPropertyID::kPlaceSelf:
      return Get2Values(placeSelfShorthand());
    case CSSPropertyID::kFont:
      return FontValue();
    case CSSPropertyID::kFontSynthesis:
      return FontSynthesisValue();
    case CSSPropertyID::kFontVariant:
      return FontVariantValue();
    case CSSPropertyID::kMargin:
      return Get4Values(marginShorthand());
    case CSSPropertyID::kMarginBlock:
      return Get2Values(marginBlockShorthand());
    case CSSPropertyID::kMarginInline:
      return Get2Values(marginInlineShorthand());
    case CSSPropertyID::kMasonryTrack:
      return GetShorthandValueForMasonryTrack();
    case CSSPropertyID::kOffset:
      return OffsetValue();
    case CSSPropertyID::kOverflow:
      return Get2Values(overflowShorthand());
    case CSSPropertyID::kOverscrollBehavior:
      return Get2Values(overscrollBehaviorShorthand());
    case CSSPropertyID::kPadding:
      return Get4Values(paddingShorthand());
    case CSSPropertyID::kPaddingBlock:
      return Get2Values(paddingBlockShorthand());
    case CSSPropertyID::kPaddingInline:
      return Get2Values(paddingInlineShorthand());
    case CSSPropertyID::kTextDecoration:
      return TextDecorationValue();
    case CSSPropertyID::kTransition:
      return GetLayeredShorthandValue(transitionShorthand());
    case CSSPropertyID::kListStyle:
      return GetShorthandValue(listStyleShorthand());
    case CSSPropertyID::kMaskPosition:
      return GetLayeredShorthandValue(maskPositionShorthand());
    case CSSPropertyID::kMask:
      return GetLayeredShorthandValue(maskShorthand());
    case CSSPropertyID::kTextBox:
      return TextBoxValue();
    case CSSPropertyID::kTextEmphasis:
      return GetShorthandValue(textEmphasisShorthand());
    case CSSPropertyID::kTextSpacing:
      return TextSpacingValue();
    case CSSPropertyID::kWebkitTextStroke:
      return GetShorthandValue(webkitTextStrokeShorthand());
    case CSSPropertyID::kTextWrap:
      return TextWrapValue();
    case CSSPropertyID::kMarker: {
      if (const CSSValue* start =
              property_set_.GetPropertyCSSValue(GetCSSPropertyMarkerStart())) {
        const CSSValue* mid =
            property_set_.GetPropertyCSSValue(GetCSSPropertyMarkerMid());
        const CSSValue* end =
            property_set_.GetPropertyCSSValue(GetCSSPropertyMarkerEnd());
        if (mid && end && *start == *mid && *start == *end) {
          return start->CssText();
        }
      }
      return String();
    }
    case CSSPropertyID::kBorderRadius:
      return BorderRadiusValue();
    case CSSPropertyID::kScrollPadding:
      return Get4Values(scrollPaddingShorthand());
    case CSSPropertyID::kScrollPaddingBlock:
      return Get2Values(scrollPaddingBlockShorthand());
    case CSSPropertyID::kScrollPaddingInline:
      return Get2Values(scrollPaddingInlineShorthand());
    case CSSPropertyID::kScrollMargin:
      return Get4Values(scrollMarginShorthand());
    case CSSPropertyID::kScrollMarginBlock:
      return Get2Values(scrollMarginBlockShorthand());
    case CSSPropertyID::kScrollMarginInline:
      return Get2Values(scrollMarginInlineShorthand());
    case CSSPropertyID::kScrollTimeline:
      return ScrollTimelineValue();
    case CSSPropertyID::kPageBreakAfter:
      return PageBreakPropertyValue(pageBreakAfterShorthand());
    case CSSPropertyID::kPageBreakBefore:
      return PageBreakPropertyValue(pageBreakBeforeShorthand());
    case CSSPropertyID::kPageBreakInside:
      return PageBreakPropertyValue(pageBreakInsideShorthand());
    case CSSPropertyID::kViewTimeline:
      return ViewTimelineValue();
    case CSSPropertyID::kWhiteSpace:
      return WhiteSpaceValue();
    case CSSPropertyID::kWebkitColumnBreakAfter:
    case CSSPropertyID::kWebkitColumnBreakBefore:
    case CSSPropertyID::kWebkitColumnBreakInside:
    case CSSPropertyID::kWebkitMaskBoxImage:
      // Temporary exceptions to the NOTREACHED() below.
      // TODO(crbug.com/1316689): Write something real here.
      return String();
    case CSSPropertyID::kScrollStart:
      return ScrollStartValue();
    case CSSPropertyID::kPositionTry:
      return PositionTryValue(positionTryShorthand());
    default:
      NOTREACHED_IN_MIGRATION()
          << "Shorthand property "
          << CSSPropertyName(property_id).ToAtomicString()
          << " must be handled in StylePropertySerializer::SerializeShorthand.";
      return String();
  }
}

// The font shorthand only allows keyword font-stretch values. Thus, we check if
// a percentage value can be parsed as a keyword, and if so, serialize it as
// that keyword.
const CSSValue* GetFontStretchKeyword(const CSSValue* font_stretch_value) {
  if (IsA<CSSIdentifierValue>(font_stretch_value)) {
    return font_stretch_value;
  }
  if (auto* primitive_value =
          DynamicTo<CSSPrimitiveValue>(font_stretch_value)) {
    double value = primitive_value->GetDoubleValue();
    if (value == 50) {
      return CSSIdentifierValue::Create(CSSValueID::kUltraCondensed);
    }
    if (value == 62.5) {
      return CSSIdentifierValue::Create(CSSValueID::kExtraCondensed);
    }
    if (value == 75) {
      return CSSIdentifierValue::Create(CSSValueID::kCondensed);
    }
    if (value == 87.5) {
      return CSSIdentifierValue::Create(CSSValueID::kSemiCondensed);
    }
    if (value == 100) {
      return CSSIdentifierValue::Create(CSSValueID::kNormal);
    }
    if (value == 112.5) {
      return CSSIdentifierValue::Create(CSSValueID::kSemiExpanded);
    }
    if (value == 125) {
      return CSSIdentifierValue::Create(CSSValueID::kExpanded);
    }
    if (value == 150) {
      return CSSIdentifierValue::Create(CSSValueID::kExtraExpanded);
    }
    if (value == 200) {
      return CSSIdentifierValue::Create(CSSValueID::kUltraExpanded);
    }
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
  if (property.IDEquals(CSSPropertyID::kFontStretch)) {
    const CSSValue* keyword = GetFontStretchKeyword(val);
    if (!keyword) {
      return false;
    }
    val = keyword;
  }
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(val);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kNormal) {
    return true;
  }

  String value;
  if (property.IDEquals(CSSPropertyID::kFontVariantLigatures) &&
      identifier_value && identifier_value->GetValueID() == CSSValueID::kNone) {
    // A shorter representation is preferred in general. Thus, 'none' returns
    // instead of the spelling-out form.
    // https://www.w3.org/Bugs/Public/show_bug.cgi?id=29594#c1
    value = "none";
  } else {
    value = val->CssText();
  }

  // The font longhand property values can be empty where the font shorthand
  // properties (e.g., font, font-variant, etc.) initialize them.
  if (value.empty()) {
    return true;
  }

  if (!result.empty()) {
    switch (property.PropertyID()) {
      case CSSPropertyID::kFontStyle:
        break;  // No prefix.
      case CSSPropertyID::kFontFamily:
      case CSSPropertyID::kFontStretch:
      case CSSPropertyID::kFontVariantCaps:
      case CSSPropertyID::kFontVariantLigatures:
      case CSSPropertyID::kFontVariantNumeric:
      case CSSPropertyID::kFontVariantEastAsian:
      case CSSPropertyID::kFontVariantAlternates:
      case CSSPropertyID::kFontVariantPosition:
      case CSSPropertyID::kFontVariantEmoji:
      case CSSPropertyID::kFontWeight:
        result.Append(' ');
        break;
      case CSSPropertyID::kLineHeight:
        result.Append(" / ");
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
  result.Append(value);
  return true;
}

String StylePropertySerializer::ContainerValue() const {
  CHECK_EQ(containerShorthand().length(), 2u);
  CHECK_EQ(containerShorthand().properties()[0],
           &GetCSSPropertyContainerName());
  CHECK_EQ(containerShorthand().properties()[1],
           &GetCSSPropertyContainerType());

  CSSValueList* list = CSSValueList::CreateSlashSeparated();

  const CSSValue* name =
      property_set_.GetPropertyCSSValue(GetCSSPropertyContainerName());
  const CSSValue* type =
      property_set_.GetPropertyCSSValue(GetCSSPropertyContainerType());

  DCHECK(name);
  DCHECK(type);

  list->Append(*name);

  if (const auto* ident_value = DynamicTo<CSSIdentifierValue>(type);
      !ident_value || ident_value->GetValueID() != CSSValueID::kNormal) {
    list->Append(*type);
  }

  return list->CssText();
}

namespace {

bool IsIdentifier(const CSSValue& value, CSSValueID ident) {
  const auto* ident_value = DynamicTo<CSSIdentifierValue>(value);
  return ident_value && ident_value->GetValueID() == ident;
}

bool IsIdentifierPair(const CSSValue& value, CSSValueID ident) {
  const auto* pair_value = DynamicTo<CSSValuePair>(value);
  return pair_value && IsIdentifier(pair_value->First(), ident) &&
         IsIdentifier(pair_value->Second(), ident);
}

CSSValue* TimelineValueItem(wtf_size_t index,
                            const CSSValueList& name_list,
                            const CSSValueList& axis_list,
                            const CSSValueList* inset_list) {
  DCHECK_LT(index, name_list.length());
  DCHECK_LT(index, axis_list.length());
  DCHECK(!inset_list || index < inset_list->length());

  const CSSValue& name = name_list.Item(index);
  const CSSValue& axis = axis_list.Item(index);
  const CSSValue* inset = inset_list ? &inset_list->Item(index) : nullptr;

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  // Note that the name part can never be omitted, since e.g. serializing
  // "view-timeline:none inline" as "view-timeline:inline" doesn't roundtrip.
  // (It would set view-timeline-name to inline).
  list->Append(name);

  if (!IsIdentifier(axis, CSSValueID::kBlock)) {
    list->Append(axis);
  }
  if (inset && !IsIdentifierPair(*inset, CSSValueID::kAuto)) {
    list->Append(*inset);
  }

  return list;
}

}  // namespace

String StylePropertySerializer::TimelineValue(
    const StylePropertyShorthand& shorthand) const {
  CHECK_GE(shorthand.length(), 2u);
  CHECK_LE(shorthand.length(), 3u);

  const CSSValueList& name_list = To<CSSValueList>(
      *property_set_.GetPropertyCSSValue(*shorthand.properties()[0]));
  const CSSValueList& axis_list = To<CSSValueList>(
      *property_set_.GetPropertyCSSValue(*shorthand.properties()[1]));
  const CSSValueList* inset_list =
      shorthand.length() == 3u
          ? To<CSSValueList>(
                property_set_.GetPropertyCSSValue(*shorthand.properties()[2]))
          : nullptr;

  // The scroll/view-timeline shorthand can not expand to longhands of two
  // different lengths, so we can also not contract two different-longhands
  // into a single shorthand.
  if (name_list.length() != axis_list.length()) {
    return "";
  }
  if (inset_list && name_list.length() != inset_list->length()) {
    return "";
  }

  CSSValueList* list = CSSValueList::CreateCommaSeparated();

  for (wtf_size_t i = 0; i < name_list.length(); ++i) {
    list->Append(*TimelineValueItem(i, name_list, axis_list, inset_list));
  }

  return list->CssText();
}

String StylePropertySerializer::ScrollTimelineValue() const {
  CHECK_EQ(scrollTimelineShorthand().length(), 2u);
  CHECK_EQ(scrollTimelineShorthand().properties()[0],
           &GetCSSPropertyScrollTimelineName());
  CHECK_EQ(scrollTimelineShorthand().properties()[1],
           &GetCSSPropertyScrollTimelineAxis());
  return TimelineValue(scrollTimelineShorthand());
}

String StylePropertySerializer::ViewTimelineValue() const {
  CHECK_EQ(viewTimelineShorthand().length(), 3u);
  CHECK_EQ(viewTimelineShorthand().properties()[0],
           &GetCSSPropertyViewTimelineName());
  CHECK_EQ(viewTimelineShorthand().properties()[1],
           &GetCSSPropertyViewTimelineAxis());
  CHECK_EQ(viewTimelineShorthand().properties()[2],
           &GetCSSPropertyViewTimelineInset());
  return TimelineValue(viewTimelineShorthand());
}

namespace {

// Return the name and offset (in percent). This is useful for
// contracting '<somename> 0%' and '<somename> 100%' into just <somename>.
//
// If the offset is present, but not a <percentage>, -1 is returned as the
// offset. Otherwise (also in the 'normal' case), the `default_offset_percent`
// is returned.
std::pair<CSSValueID, double> GetTimelineRangePercent(
    const CSSValue& value,
    double default_offset_percent) {
  const auto* list = DynamicTo<CSSValueList>(value);
  if (!list) {
    return {CSSValueID::kNormal, default_offset_percent};
  }
  DCHECK_GE(list->length(), 1u);
  DCHECK_LE(list->length(), 2u);
  CSSValueID name = CSSValueID::kNormal;
  double offset_percent = default_offset_percent;

  if (list->Item(0).IsIdentifierValue()) {
    name = To<CSSIdentifierValue>(list->Item(0)).GetValueID();
    if (list->length() == 2u) {
      const auto& offset = To<CSSPrimitiveValue>(list->Item(1));
      offset_percent = offset.IsPercentage() ? offset.GetValue<double>() : -1.0;
    }
  } else {
    const auto& offset = To<CSSPrimitiveValue>(list->Item(0));
    offset_percent = offset.IsPercentage() ? offset.GetValue<double>() : -1.0;
  }

  return {name, offset_percent};
}

CSSValue* AnimationRangeShorthandValueItem(wtf_size_t index,
                                           const CSSValueList& start_list,
                                           const CSSValueList& end_list) {
  DCHECK_LT(index, start_list.length());
  DCHECK_LT(index, end_list.length());

  const CSSValue& start = start_list.Item(index);
  const CSSValue& end = end_list.Item(index);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  list->Append(start);

  // The form "name X name 100%" must contract to "name X".
  //
  // https://github.com/w3c/csswg-drafts/issues/8438
  const auto& start_pair = GetTimelineRangePercent(start, 0.0);
  const auto& end_pair = GetTimelineRangePercent(end, 100.0);
  std::pair<CSSValueID, double> omittable_end = {start_pair.first, 100.0};
  if (end_pair != omittable_end) {
    list->Append(end);
  }

  return list;
}

}  // namespace

String StylePropertySerializer::AnimationRangeShorthandValue() const {
  CHECK_EQ(animationRangeShorthand().length(), 2u);
  CHECK_EQ(animationRangeShorthand().properties()[0],
           &GetCSSPropertyAnimationRangeStart());
  CHECK_EQ(animationRangeShorthand().properties()[1],
           &GetCSSPropertyAnimationRangeEnd());

  const CSSValueList& start_list = To<CSSValueList>(
      *property_set_.GetPropertyCSSValue(GetCSSPropertyAnimationRangeStart()));
  const CSSValueList& end_list = To<CSSValueList>(
      *property_set_.GetPropertyCSSValue(GetCSSPropertyAnimationRangeEnd()));

  if (start_list.length() != end_list.length()) {
    return "";
  }

  CSSValueList* list = CSSValueList::CreateCommaSeparated();

  for (wtf_size_t i = 0; i < start_list.length(); ++i) {
    list->Append(*AnimationRangeShorthandValueItem(i, start_list, end_list));
  }

  return list->CssText();
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
  int font_kerning_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontKerning());
  int font_optical_sizing_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontOpticalSizing());
  int font_variation_settings_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontVariationSettings());
  int font_feature_settings_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontFeatureSettings());
  DCHECK_NE(font_size_property_index, -1);
  DCHECK_NE(font_family_property_index, -1);
  DCHECK_NE(font_variant_caps_property_index, -1);
  DCHECK_NE(font_variant_ligatures_property_index, -1);
  DCHECK_NE(font_variant_numeric_property_index, -1);
  DCHECK_NE(font_variant_east_asian_property_index, -1);
  DCHECK_NE(font_kerning_property_index, -1);
  DCHECK_NE(font_optical_sizing_property_index, -1);
  DCHECK_NE(font_variation_settings_property_index, -1);
  DCHECK_NE(font_feature_settings_property_index, -1);

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
  PropertyValueForSerializer font_kerning_property =
      property_set_.PropertyAt(font_kerning_property_index);
  PropertyValueForSerializer font_optical_sizing_property =
      property_set_.PropertyAt(font_optical_sizing_property_index);
  PropertyValueForSerializer font_variation_settings_property =
      property_set_.PropertyAt(font_variation_settings_property_index);
  PropertyValueForSerializer font_feature_settings_property =
      property_set_.PropertyAt(font_feature_settings_property_index);

  // Check that non-initial font-variant subproperties are not conflicting with
  // this serialization.
  const CSSValue* ligatures_value = font_variant_ligatures_property.Value();
  const CSSValue* numeric_value = font_variant_numeric_property.Value();
  const CSSValue* east_asian_value = font_variant_east_asian_property.Value();
  const CSSValue* feature_settings_value =
      font_feature_settings_property.Value();
  const CSSValue* variation_settings_value =
      font_variation_settings_property.Value();

  auto IsPropertyNonInitial = [](const CSSValue& value,
                                 const CSSValueID initial_value_id) {
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
    return (identifier_value &&
            identifier_value->GetValueID() != initial_value_id);
  };

  if (IsPropertyNonInitial(*ligatures_value, CSSValueID::kNormal) ||
      ligatures_value->IsValueList()) {
    return g_empty_string;
  }

  if (IsPropertyNonInitial(*numeric_value, CSSValueID::kNormal) ||
      numeric_value->IsValueList()) {
    return g_empty_string;
  }

  if (IsPropertyNonInitial(*east_asian_value, CSSValueID::kNormal) ||
      east_asian_value->IsValueList()) {
    return g_empty_string;
  }

  if (IsPropertyNonInitial(*font_kerning_property.Value(), CSSValueID::kAuto) ||
      IsPropertyNonInitial(*font_optical_sizing_property.Value(),
                           CSSValueID::kAuto)) {
    return g_empty_string;
  }

  if (IsPropertyNonInitial(*variation_settings_value, CSSValueID::kNormal) ||
      variation_settings_value->IsValueList()) {
    return g_empty_string;
  }

  if (IsPropertyNonInitial(*feature_settings_value, CSSValueID::kNormal) ||
      feature_settings_value->IsValueList()) {
    return g_empty_string;
  }

  int font_variant_alternates_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontVariantAlternates());
  DCHECK_NE(font_variant_alternates_property_index, -1);
  PropertyValueForSerializer font_variant_alternates_property =
      property_set_.PropertyAt(font_variant_alternates_property_index);
  const CSSValue* alternates_value = font_variant_alternates_property.Value();
  if (IsPropertyNonInitial(*alternates_value, CSSValueID::kNormal) ||
      alternates_value->IsValueList()) {
    return g_empty_string;
  }

  int font_variant_position_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontVariantPosition());
  DCHECK_NE(font_variant_position_property_index, -1);
  PropertyValueForSerializer font_variant_position_property =
      property_set_.PropertyAt(font_variant_position_property_index);
  if (IsPropertyNonInitial(*font_variant_position_property.Value(),
                           CSSValueID::kNormal)) {
    return g_empty_string;
  }

  if (RuntimeEnabledFeatures::FontVariantEmojiEnabled()) {
    int font_variant_emoji_property_index =
        property_set_.FindPropertyIndex(GetCSSPropertyFontVariantEmoji());
    DCHECK_NE(font_variant_emoji_property_index, -1);
    PropertyValueForSerializer font_variant_emoji_property =
        property_set_.PropertyAt(font_variant_emoji_property_index);
    if (IsPropertyNonInitial(*font_variant_emoji_property.Value(),
                             CSSValueID::kNormal)) {
      return g_empty_string;
    }
  }

  if (RuntimeEnabledFeatures::CSSFontSizeAdjustEnabled()) {
    int font_size_adjust_property_index =
        property_set_.FindPropertyIndex(GetCSSPropertyFontSizeAdjust());
    DCHECK_NE(font_size_adjust_property_index, -1);
    PropertyValueForSerializer font_size_adjust_property =
        property_set_.PropertyAt(font_size_adjust_property_index);
    const CSSValue* size_adjust_value = font_size_adjust_property.Value();
    if (IsPropertyNonInitial(*size_adjust_value, CSSValueID::kNone) ||
        size_adjust_value->IsNumericLiteralValue()) {
      return g_empty_string;
    }
  }

  const StylePropertyShorthand& shorthand = fontShorthand();
  const StylePropertyShorthand::Properties& longhands = shorthand.properties();
  const CSSValue* first = property_set_.GetPropertyCSSValue(*longhands[0]);
  if (const auto* system_font =
          DynamicTo<cssvalue::CSSPendingSystemFontValue>(first)) {
    for (const CSSProperty* const longhand : longhands.subspan(1)) {
      const CSSValue* value = property_set_.GetPropertyCSSValue(*longhand);
      if (!base::ValuesEquivalent(first, value)) {
        return g_empty_string;
      }
    }
    return getValueName(system_font->SystemFontId());
  } else {
    for (const CSSProperty* const longhand : longhands.subspan(1)) {
      const CSSValue* value = property_set_.GetPropertyCSSValue(*longhand);
      if (value->IsPendingSystemFontValue()) {
        return g_empty_string;
      }
    }
  }

  StringBuilder result;
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontStyle(), result);

  const CSSValue* val = font_variant_caps_property.Value();
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(val);
  if (identifier_value &&
      (identifier_value->GetValueID() != CSSValueID::kSmallCaps &&
       identifier_value->GetValueID() != CSSValueID::kNormal)) {
    return g_empty_string;
  }
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontVariantCaps(), result);

  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontWeight(), result);
  bool font_stretch_valid =
      AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontStretch(), result);
  if (!font_stretch_valid) {
    return String();
  }
  if (!result.empty()) {
    result.Append(' ');
  }
  result.Append(font_size_property.Value()->CssText());
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyLineHeight(), result);
  if (!result.empty()) {
    result.Append(' ');
  }
  result.Append(font_family_property.Value()->CssText());
  return result.ReleaseString();
}

String StylePropertySerializer::FontVariantValue() const {
  StringBuilder result;
  bool is_variant_ligatures_none = false;

  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontVariantLigatures(),
                                     result);
  if (result.ToString() == "none") {
    is_variant_ligatures_none = true;
  }
  const unsigned variant_ligatures_result_length = result.length();

  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontVariantCaps(), result);
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontVariantAlternates(),
                                     result);
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontVariantNumeric(),
                                     result);
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontVariantEastAsian(),
                                     result);
  AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontVariantPosition(),
                                     result);
  if (RuntimeEnabledFeatures::FontVariantEmojiEnabled()) {
    AppendFontLonghandValueIfNotNormal(GetCSSPropertyFontVariantEmoji(),
                                       result);
  }

  // The font-variant shorthand should return an empty string where
  // it cannot represent "font-variant-ligatures: none" along
  // with any other non-normal longhands.
  // https://drafts.csswg.org/cssom-1/#serializing-css-values
  if (is_variant_ligatures_none &&
      result.length() != variant_ligatures_result_length) {
    return g_empty_string;
  }

  if (result.empty()) {
    return "normal";
  }

  return result.ReleaseString();
}

String StylePropertySerializer::FontSynthesisValue() const {
  StringBuilder result;

  int font_synthesis_weight_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontSynthesisWeight());
  int font_synthesis_style_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontSynthesisStyle());
  int font_synthesis_small_caps_property_index =
      property_set_.FindPropertyIndex(GetCSSPropertyFontSynthesisSmallCaps());
  DCHECK_NE(font_synthesis_weight_property_index, -1);
  DCHECK_NE(font_synthesis_style_property_index, -1);
  DCHECK_NE(font_synthesis_small_caps_property_index, -1);

  PropertyValueForSerializer font_synthesis_weight_property =
      property_set_.PropertyAt(font_synthesis_weight_property_index);
  PropertyValueForSerializer font_synthesis_style_property =
      property_set_.PropertyAt(font_synthesis_style_property_index);
  PropertyValueForSerializer font_synthesis_small_caps_property =
      property_set_.PropertyAt(font_synthesis_small_caps_property_index);

  const CSSValue* font_synthesis_weight_value =
      font_synthesis_weight_property.Value();
  const CSSValue* font_synthesis_style_value =
      font_synthesis_style_property.Value();
  const CSSValue* font_synthesis_small_caps_value =
      font_synthesis_small_caps_property.Value();

  auto* font_synthesis_weight_identifier_value =
      DynamicTo<CSSIdentifierValue>(font_synthesis_weight_value);
  if (font_synthesis_weight_identifier_value &&
      font_synthesis_weight_identifier_value->GetValueID() ==
          CSSValueID::kAuto) {
    result.Append("weight");
  }

  auto* font_synthesis_style_identifier_value =
      DynamicTo<CSSIdentifierValue>(font_synthesis_style_value);
  if (font_synthesis_style_identifier_value &&
      font_synthesis_style_identifier_value->GetValueID() ==
          CSSValueID::kAuto) {
    if (!result.empty()) {
      result.Append(' ');
    }
    result.Append("style");
  }

  auto* font_synthesis_small_caps_identifier_value =
      DynamicTo<CSSIdentifierValue>(font_synthesis_small_caps_value);
  if (font_synthesis_small_caps_identifier_value &&
      font_synthesis_small_caps_identifier_value->GetValueID() ==
          CSSValueID::kAuto) {
    if (!result.empty()) {
      result.Append(' ');
    }
    result.Append("small-caps");
  }

  if (result.empty()) {
    return "none";
  }

  return result.ReleaseString();
}

String StylePropertySerializer::OffsetValue() const {
  const CSSValue* position =
      property_set_.GetPropertyCSSValue(GetCSSPropertyOffsetPosition());
  const CSSValue* path =
      property_set_.GetPropertyCSSValue(GetCSSPropertyOffsetPath());
  const CSSValue* distance =
      property_set_.GetPropertyCSSValue(GetCSSPropertyOffsetDistance());
  const CSSValue* rotate =
      property_set_.GetPropertyCSSValue(GetCSSPropertyOffsetRotate());
  const CSSValue* anchor =
      property_set_.GetPropertyCSSValue(GetCSSPropertyOffsetAnchor());

  auto is_initial_identifier_value = [](const CSSValue* value,
                                        CSSValueID id) -> bool {
    return value->IsIdentifierValue() &&
           DynamicTo<CSSIdentifierValue>(value)->GetValueID() == id;
  };

  bool use_distance =
      distance && !(distance->IsNumericLiteralValue() &&
                    To<CSSNumericLiteralValue>(*distance).DoubleValue() == 0.0);
  const auto* rotate_list_value = DynamicTo<CSSValueList>(rotate);
  bool is_rotate_auto = rotate_list_value && rotate_list_value->length() == 1 &&
                        is_initial_identifier_value(&rotate_list_value->First(),
                                                    CSSValueID::kAuto);
  bool is_rotate_zero =
      rotate_list_value && rotate_list_value->length() == 1 &&
      rotate_list_value->First().IsNumericLiteralValue() &&
      (To<CSSNumericLiteralValue>(rotate_list_value->First()).DoubleValue() ==
       0.0);
  bool is_rotate_auto_zero =
      rotate_list_value && rotate_list_value->length() == 2 &&
      rotate_list_value->Item(1).IsNumericLiteralValue() &&
      (To<CSSNumericLiteralValue>(rotate_list_value->Item(1)).DoubleValue() ==
       0.0) &&
      is_initial_identifier_value(&rotate_list_value->Item(0),
                                  CSSValueID::kAuto);
  bool use_rotate =
      rotate && ((use_distance && is_rotate_zero) ||
                 (!is_initial_identifier_value(rotate, CSSValueID::kAuto) &&
                  !is_rotate_auto && !is_rotate_auto_zero));
  bool use_path =
      path && (use_rotate || use_distance ||
               !is_initial_identifier_value(path, CSSValueID::kNone));
  bool use_position =
      position && (!use_path ||
                   !is_initial_identifier_value(position, CSSValueID::kNormal));
  bool use_anchor =
      anchor && (!is_initial_identifier_value(anchor, CSSValueID::kAuto));

  StringBuilder result;
  if (use_position) {
    result.Append(position->CssText());
  }
  if (use_path) {
    if (!result.empty()) {
      result.Append(" ");
    }
    result.Append(path->CssText());
  }
  if (use_distance) {
    result.Append(" ");
    result.Append(distance->CssText());
  }
  if (use_rotate) {
    result.Append(" ");
    result.Append(rotate->CssText());
  }
  if (use_anchor) {
    result.Append(" / ");
    result.Append(anchor->CssText());
  }
  return result.ReleaseString();
}

String StylePropertySerializer::TextDecorationValue() const {
  StringBuilder result;
  const auto& shorthand = shorthandForProperty(CSSPropertyID::kTextDecoration);
  for (const CSSProperty* const longhand : shorthand.properties()) {
    const CSSValue* value = property_set_.GetPropertyCSSValue(*longhand);
    String value_text = value->CssText();
    if (value->IsInitialValue()) {
      continue;
    }
    if (longhand->PropertyID() == CSSPropertyID::kTextDecorationThickness) {
      if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
        // Do not include initial value 'auto' for thickness.
        // TODO(https://crbug.com/1093826): general shorthand serialization
        // issues remain, in particular for text-decoration.
        CSSValueID value_id = identifier_value->GetValueID();
        if (value_id == CSSValueID::kAuto) {
          continue;
        }
      }
    }
    if (!result.empty()) {
      result.Append(" ");
    }
    result.Append(value_text);
  }

  if (result.empty()) {
    return "none";
  }
  return result.ReleaseString();
}

String StylePropertySerializer::Get2Values(
    const StylePropertyShorthand& shorthand) const {
  // Assume the properties are in the usual order start, end.
  int start_value_index =
      property_set_.FindPropertyIndex(*shorthand.properties()[0]);
  int end_value_index =
      property_set_.FindPropertyIndex(*shorthand.properties()[1]);

  if (start_value_index == -1 || end_value_index == -1) {
    return String();
  }

  PropertyValueForSerializer start =
      property_set_.PropertyAt(start_value_index);
  PropertyValueForSerializer end = property_set_.PropertyAt(end_value_index);

  bool show_end = !base::ValuesEquivalent(start.Value(), end.Value());

  StringBuilder result;
  result.Append(start.Value()->CssText());
  if (show_end) {
    result.Append(' ');
    result.Append(end.Value()->CssText());
  }
  return result.ReleaseString();
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
      bottom_value_index == -1 || left_value_index == -1) {
    return String();
  }

  PropertyValueForSerializer top = property_set_.PropertyAt(top_value_index);
  PropertyValueForSerializer right =
      property_set_.PropertyAt(right_value_index);
  PropertyValueForSerializer bottom =
      property_set_.PropertyAt(bottom_value_index);
  PropertyValueForSerializer left = property_set_.PropertyAt(left_value_index);

  bool show_left = !base::ValuesEquivalent(right.Value(), left.Value());
  bool show_bottom =
      !base::ValuesEquivalent(top.Value(), bottom.Value()) || show_left;
  bool show_right =
      !base::ValuesEquivalent(top.Value(), right.Value()) || show_bottom;

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
  return result.ReleaseString();
}

namespace {

// Serialize clip and origin (https://drafts.fxtf.org/css-masking/#the-mask):
// * If one <geometry-box> value and the no-clip keyword are present then
//   <geometry-box> sets mask-origin and no-clip sets mask-clip to that value.
// * If one <geometry-box> value and no no-clip keyword are present then
//   <geometry-box> sets both mask-origin and mask-clip to that value.
// * If two <geometry-box> values are present, then the first sets mask-origin
//   and the second mask-clip.
// Additionally, omits components when possible (see:
// https://drafts.csswg.org/cssom/#serialize-a-css-value).
void SerializeMaskOriginAndClip(StringBuilder& result,
                                const CSSValueID& origin_id,
                                const CSSValueID& clip_id) {
  // If both values are border-box, omit everything as it is the default.
  if (origin_id == CSSValueID::kBorderBox &&
      clip_id == CSSValueID::kBorderBox) {
    return;
  }

  if (!result.empty()) {
    result.Append(' ');
  }
  if (origin_id == clip_id) {
    // If the values are the same, only emit one value. Note that mask-origin
    // does not support no-clip, so there is no need to consider no-clip
    // special cases.
    result.Append(getValueName(origin_id));
  } else if (origin_id == CSSValueID::kBorderBox &&
             clip_id == CSSValueID::kNoClip) {
    // Mask-origin does not support no-clip, so mask-origin can be omitted if it
    // is the default.
    result.Append(getValueName(clip_id));
  } else {
    result.Append(getValueName(origin_id));
    result.Append(' ');
    result.Append(getValueName(clip_id));
  }
}

}  // namespace

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
      const CSSValueList* value_list = To<CSSValueList>(values[i].Get());
      num_layers = std::max(num_layers, value_list->length());
    }
  }

  StringBuilder result;

  // Now stitch the properties together.
  for (wtf_size_t layer = 0; layer < num_layers; layer++) {
    StringBuilder layer_result;
    bool is_position_x_serialized = false;
    bool is_position_y_serialized = false;
    const CSSValue* mask_position_x = nullptr;
    CSSValueID mask_origin_value = CSSValueID::kBorderBox;

    for (unsigned property_index = 0; property_index < size; property_index++) {
      const CSSValue* value = nullptr;
      const CSSProperty* property = shorthand.properties()[property_index];

      // Get a CSSValue for this property and layer.
      if (values[property_index]->IsBaseValueList()) {
        const auto* property_values =
            To<CSSValueList>(values[property_index].Get());
        // There might not be an item for this layer for this property.
        if (layer < property_values->length()) {
          value = &property_values->Item(layer);
        }
      } else if ((layer == 0 &&
                  !property->IDEquals(CSSPropertyID::kBackgroundColor)) ||
                 (layer == num_layers - 1 &&
                  property->IDEquals(CSSPropertyID::kBackgroundColor))) {
        // Singletons except background color belong in the 0th layer.
        // Background color belongs in the last layer.
        value = values[property_index];
      }
      // No point proceeding if there's not a value to look at.
      if (!value) {
        continue;
      }

      bool omit_value = value->IsInitialValue();

      // The shorthand can not represent the following properties if they have
      // non-initial values. This is because they are always reset to their
      // initial value by the shorthand.
      //
      // Note that initial values for animation-* properties only contain
      // one list item, hence the check for 'layer > 0'.
      if (property->IDEquals(CSSPropertyID::kAnimationTimeline)) {
        auto* ident = DynamicTo<CSSIdentifierValue>(value);
        if (!ident ||
            (ident->GetValueID() !=
             CSSAnimationData::InitialTimeline().GetKeyword()) ||
            layer > 0) {
          DCHECK(RuntimeEnabledFeatures::ScrollTimelineEnabled());
          return g_empty_string;
        }
        omit_value = true;
      }
      if (property->IDEquals(CSSPropertyID::kAnimationRangeStart)) {
        auto* ident = DynamicTo<CSSIdentifierValue>(value);
        if (!ident || (ident->GetValueID() != CSSValueID::kNormal) ||
            layer > 0) {
          DCHECK(RuntimeEnabledFeatures::ScrollTimelineEnabled());
          return g_empty_string;
        }
        omit_value = true;
      }
      if (property->IDEquals(CSSPropertyID::kAnimationRangeEnd)) {
        auto* ident = DynamicTo<CSSIdentifierValue>(value);
        if (!ident || (ident->GetValueID() != CSSValueID::kNormal) ||
            layer > 0) {
          DCHECK(RuntimeEnabledFeatures::ScrollTimelineEnabled());
          return g_empty_string;
        }
        omit_value = true;
      }

      if (property->IDEquals(CSSPropertyID::kTransitionBehavior)) {
        CHECK_EQ(shorthand.id(), CSSPropertyID::kTransition);
        auto* ident = DynamicTo<CSSIdentifierValue>(value);
        CHECK(ident) << " transition-behavior should only have a "
                        "CSSIdentifierValue for a value. CssText: "
                     << value->CssText();
        if (ident->GetValueID() == CSSValueID::kNormal) {
          // transition-behavior overrides InitialValue to return "normal"
          // instead of "initial", but we don't want to include "normal" in the
          // shorthand serialization, so this special case is needed.
          // TODO(http://crbug.com/501673): We should have a better solution
          // before fixing all CSS properties to fix the above bug.
          omit_value = true;
        }
      }
      // The transition shorthand should only serialize values which aren't
      // set to their default value:
      // https://github.com/web-platform-tests/wpt/issues/43574
      if (property->IDEquals(CSSPropertyID::kTransitionDelay) ||
          property->IDEquals(CSSPropertyID::kTransitionDuration)) {
        auto* numeric_value = DynamicTo<CSSNumericLiteralValue>(value);
        if (numeric_value &&
            numeric_value->IsZero() == CSSPrimitiveValue::BoolStatus::kTrue) {
          omit_value = true;
        }
      } else if (property->IDEquals(
                     CSSPropertyID::kTransitionTimingFunction)) {
        if (auto* ident = DynamicTo<CSSIdentifierValue>(value)) {
          if (ident->GetValueID() == CSSValueID::kEase) {
            omit_value = true;
          }
        }
      } else if (property->IDEquals(CSSPropertyID::kTransitionProperty)) {
        if (auto* custom_ident = DynamicTo<CSSCustomIdentValue>(value)) {
          if (custom_ident->IsKnownPropertyID() &&
              custom_ident->ValueAsPropertyID() == CSSPropertyID::kAll) {
            omit_value = true;
          }
        } else if (auto* ident = DynamicTo<CSSIdentifierValue>(value)) {
          if (ident->GetValueID() == CSSValueID::kAll) {
            omit_value = true;
          }
        }
      }

      if (shorthand.id() == CSSPropertyID::kMask) {
        if (property->IDEquals(CSSPropertyID::kMaskImage)) {
          if (auto* image_value = DynamicTo<CSSIdentifierValue>(value)) {
            if (image_value->GetValueID() == CSSValueID::kNone) {
              omit_value = true;
            }
          }
        } else if (property->IDEquals(CSSPropertyID::kMaskOrigin)) {
          if (auto* ident = DynamicTo<CSSIdentifierValue>(value)) {
            mask_origin_value = ident->GetValueID();
          }
          // Omit this value as it is serialized alongside mask-clip.
          omit_value = true;
        } else if (property->IDEquals(CSSPropertyID::kMaskClip)) {
          CSSValueID mask_clip_id = CSSValueID::kBorderBox;
          if (auto* ident = DynamicTo<CSSIdentifierValue>(value)) {
            mask_clip_id = ident->GetValueID();
          }
          SerializeMaskOriginAndClip(layer_result, mask_origin_value,
                                     mask_clip_id);
          omit_value = true;
        } else if (property->IDEquals(CSSPropertyID::kMaskComposite)) {
          if (auto* ident = DynamicTo<CSSIdentifierValue>(value)) {
            if (ident->GetValueID() == CSSValueID::kAdd) {
              omit_value = true;
            }
          }
        } else if (property->IDEquals(CSSPropertyID::kMaskMode)) {
          if (auto* ident = DynamicTo<CSSIdentifierValue>(value)) {
            if (ident->GetValueID() == CSSValueID::kMatchSource) {
              omit_value = true;
            }
          }
        } else if (property->IDEquals(CSSPropertyID::kMaskRepeat)) {
          if (auto* repeat = DynamicTo<CSSRepeatStyleValue>(value)) {
            if (repeat->IsRepeat()) {
              omit_value = true;
            }
          }
        } else if (property->IDEquals(CSSPropertyID::kMaskSize)) {
          if (auto* size_value = DynamicTo<CSSIdentifierValue>(value)) {
            if (size_value->GetValueID() == CSSValueID::kAuto) {
              omit_value = true;
            }
          }
        } else if (property->IDEquals(CSSPropertyID::kWebkitMaskPositionX)) {
          omit_value = true;
          mask_position_x = value;
        } else if (property->IDEquals(CSSPropertyID::kWebkitMaskPositionY)) {
          omit_value = true;

          if (!IsZeroPercent(mask_position_x) || !IsZeroPercent(value)) {
            is_position_x_serialized = true;
            is_position_y_serialized = true;

            if (!layer_result.empty()) {
              layer_result.Append(' ');
            }
            layer_result.Append(mask_position_x->CssText());
            layer_result.Append(' ');
            layer_result.Append(value->CssText());
          }
        }
      }

      if (!omit_value) {
        if (property->IDEquals(CSSPropertyID::kBackgroundSize) ||
            property->IDEquals(CSSPropertyID::kMaskSize)) {
          if (is_position_y_serialized || is_position_x_serialized) {
            layer_result.Append(" / ");
          } else {
            layer_result.Append(" 0% 0% / ");
          }
        } else if (!layer_result.empty()) {
          // Do this second to avoid ending up with an extra space in the output
          // if we hit the continue above.
          layer_result.Append(' ');
        }

        layer_result.Append(value->CssText());

        if (property->IDEquals(CSSPropertyID::kBackgroundPositionX)) {
          is_position_x_serialized = true;
        }
        if (property->IDEquals(CSSPropertyID::kBackgroundPositionY)) {
          is_position_y_serialized = true;
          // background-position is a special case. If only the first offset is
          // specified, the second one defaults to "center", not the same value.
        }
      }
    }
    if (shorthand.id() == CSSPropertyID::kMask && layer_result.empty()) {
      layer_result.Append(getValueName(CSSValueID::kNone));
    }
    if (shorthand.id() == CSSPropertyID::kTransition && layer_result.empty()) {
      // When serializing the transition shorthand, we omit all values which are
      // set to their defaults. If everything is set to the default, then emit
      // "all" instead of an empty string.
      layer_result.Append("all");
    }
    if (!layer_result.empty()) {
      if (!result.empty()) {
        result.Append(", ");
      }
      result.Append(layer_result);
    }
  }

  return result.ReleaseString();
}

String StylePropertySerializer::GetShorthandValue(
    const StylePropertyShorthand& shorthand,
    String separator) const {
  StringBuilder result;
  for (const CSSProperty* const longhand : shorthand.properties()) {
    const CSSValue* value = property_set_.GetPropertyCSSValue(*longhand);
    String value_text = value->CssText();
    if (value->IsInitialValue()) {
      continue;
    }
    if (!result.empty()) {
      result.Append(separator);
    }
    result.Append(value_text);
  }
  return result.ReleaseString();
}

String StylePropertySerializer::GetShorthandValueForColumnRule(
    const StylePropertyShorthand& shorthand) const {
  DCHECK_EQ(shorthand.length(), 3u);

  const CSSValue* column_rule_width =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[0]);
  const CSSValue* column_rule_style =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[1]);
  const CSSValue* column_rule_color =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[2]);

  StringBuilder result;
  if (const auto* ident_value =
          DynamicTo<CSSIdentifierValue>(column_rule_width);
      !(ident_value && ident_value->GetValueID() == CSSValueID::kMedium) &&
      !column_rule_width->IsInitialValue()) {
    String column_rule_width_text = column_rule_width->CssText();
    result.Append(column_rule_width_text);
  }

  if (const auto* ident_value =
          DynamicTo<CSSIdentifierValue>(column_rule_style);
      !(ident_value && ident_value->GetValueID() == CSSValueID::kNone) &&
      !column_rule_style->IsInitialValue()) {
    String column_rule_style_text = column_rule_style->CssText();
    if (!result.empty()) {
      result.Append(" ");
    }

    result.Append(column_rule_style_text);
  }
  if (const auto* ident_value =
          DynamicTo<CSSIdentifierValue>(column_rule_color);
      !(ident_value &&
        ident_value->GetValueID() == CSSValueID::kCurrentcolor) &&
      !column_rule_color->IsInitialValue()) {
    String column_rule_color_text = column_rule_color->CssText();
    if (!result.empty()) {
      result.Append(" ");
    }

    result.Append(column_rule_color_text);
  }

  if (result.empty()) {
    return "medium";
  }

  return result.ReleaseString();
}

String StylePropertySerializer::GetShorthandValueForColumns(
    const StylePropertyShorthand& shorthand) const {
  DCHECK_EQ(shorthand.length(), 2u);

  StringBuilder result;
  for (const CSSProperty* const longhand : shorthand.properties()) {
    const CSSValue* value = property_set_.GetPropertyCSSValue(*longhand);
    String value_text = value->CssText();
    if (const auto* ident_value = DynamicTo<CSSIdentifierValue>(value);
        ident_value && ident_value->GetValueID() == CSSValueID::kAuto) {
      continue;
    }
    if (!result.empty()) {
      result.Append(" ");
    }
    result.Append(value_text);
  }

  if (result.empty()) {
    return "auto";
  }

  return result.ReleaseString();
}

String StylePropertySerializer::GetShorthandValueForDoubleBarCombinator(
    const StylePropertyShorthand& shorthand) const {
  StringBuilder result;
  for (const CSSProperty* const property : shorthand.properties()) {
    const Longhand* longhand = To<Longhand>(property);
    DCHECK(!longhand->InitialValue()->IsInitialValue())
        << "Without InitialValue() implemented, 'initial' will show up in the "
           "serialization below.";
    const CSSValue* value = property_set_.GetPropertyCSSValue(*longhand);
    if (*value == *longhand->InitialValue()) {
      continue;
    }
    String value_text = value->CssText();
    if (!result.empty()) {
      result.Append(" ");
    }
    result.Append(value_text);
  }

  if (result.empty()) {
    return To<Longhand>(shorthand.properties()[0])->InitialValue()->CssText();
  }

  return result.ReleaseString();
}

String StylePropertySerializer::GetShorthandValueForGrid(
    const StylePropertyShorthand& shorthand) const {
  DCHECK_EQ(shorthand.length(), 6u);

  const auto* template_row_values =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[0]);
  const auto* template_column_values =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[1]);
  const auto* template_area_value =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[2]);
  const auto* auto_flow_values =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[3]);
  const auto* auto_row_values =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[4]);
  const auto* auto_column_values =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[5]);

  // `auto-flow`, `grid-auto-rows`, and `grid-auto-columns` are parsed as either
  // an identifier with the default value, or a CSSValueList containing a single
  // entry with the default value. Unlike `grid-template-rows` and
  // `grid-template-columns`, we *can* determine if the author specified them by
  // the presence of an associated CSSValueList.
  auto HasInitialValueListValue = [](const CSSValueList* value_list,
                                     auto* definition) -> bool {
    return value_list && value_list->length() == 1 &&
           value_list->First() == *(definition().InitialValue());
  };
  auto HasInitialIdentifierValue = [](const CSSValue* value,
                                      CSSValueID initial_value) -> bool {
    return IsA<CSSIdentifierValue>(value) &&
           To<CSSIdentifierValue>(value)->GetValueID() == initial_value;
  };

  const auto* auto_row_value_list = DynamicTo<CSSValueList>(auto_row_values);
  const bool is_auto_rows_initial_value =
      HasInitialValueListValue(auto_row_value_list,
                               GetCSSPropertyGridAutoRows) ||
      HasInitialIdentifierValue(auto_row_values, CSSValueID::kAuto);
  const bool specified_non_initial_auto_rows =
      auto_row_value_list && !is_auto_rows_initial_value;

  const auto* auto_column_value_list =
      DynamicTo<CSSValueList>(auto_column_values);
  const bool is_auto_columns_initial_value =
      HasInitialValueListValue(auto_column_value_list,
                               GetCSSPropertyGridAutoColumns) ||
      HasInitialIdentifierValue(auto_column_values, CSSValueID::kAuto);
  const bool specified_non_initial_auto_columns =
      auto_column_value_list && !is_auto_columns_initial_value;

  const auto* auto_flow_value_list = DynamicTo<CSSValueList>(auto_flow_values);
  const bool is_auto_flow_initial_value =
      HasInitialValueListValue(auto_flow_value_list,
                               GetCSSPropertyGridAutoFlow) ||
      HasInitialIdentifierValue(auto_flow_values, CSSValueID::kRow);

  // `grid-auto-*` along with named lines is not valid per the grammar.
  if ((auto_flow_value_list || auto_row_value_list || auto_column_value_list) &&
      *template_area_value !=
          *GetCSSPropertyGridTemplateAreas().InitialValue()) {
    return String();
  }

  // `grid-template-rows` and `grid-template-columns` are shorthards within this
  // shorthand. Based on how parsing works, we can't differentiate between an
  // author specifying `none` and uninitialized.
  const bool non_initial_template_rows =
      (*template_row_values !=
       *GetCSSPropertyGridTemplateRows().InitialValue());
  const bool non_initial_template_columns =
      *template_column_values !=
      *GetCSSPropertyGridTemplateColumns().InitialValue();

  // `grid-template-*` and `grid-auto-*` are mutually exclusive per direction.
  if ((non_initial_template_rows && specified_non_initial_auto_rows) ||
      (non_initial_template_columns && specified_non_initial_auto_columns) ||
      (specified_non_initial_auto_rows && specified_non_initial_auto_columns)) {
    return String();
  }

  // 1- <'grid-template'>
  // If the author didn't specify `auto-flow`, we should go down the
  // `grid-template` path. This should also round-trip if the author specified
  // the initial value for `auto-flow`, unless `auto-columns` or `auto-rows`
  // were also set, causing it to match the shorthand syntax below.
  if (!auto_flow_value_list ||
      (is_auto_flow_initial_value && !(specified_non_initial_auto_columns ||
                                       specified_non_initial_auto_rows))) {
    return GetShorthandValueForGridTemplate(shorthand);
  } else if (non_initial_template_rows && non_initial_template_columns) {
    // Specifying both rows and columns is not valid per the grammar.
    return String();
  }

  // At this point, the syntax matches:
  // <'grid-template-rows'> / [ auto-flow && dense? ] <'grid-auto-columns'>? |
  // [ auto-flow && dense? ] <'grid-auto-rows'>? / <'grid-template-columns'>
  // ...and thus will include `auto-flow` no matter what.
  StringBuilder auto_flow_text;
  auto_flow_text.Append("auto-flow");
  if (auto_flow_value_list &&
      auto_flow_value_list->HasValue(
          *CSSIdentifierValue::Create(CSSValueID::kDense))) {
    auto_flow_text.Append(" dense");
  }

  // 2- <'grid-template-rows'> / [ auto-flow && dense? ] <'grid-auto-columns'>?
  // We can't distinguish between `grid-template-rows` being unspecified or
  // being specified as `none` (see the comment near the definition of
  // `non_initial_template_rows`), as both are initial values. So we must
  // distinguish between the remaining two possible paths via `auto-flow`.
  StringBuilder result;
  if (auto_flow_value_list &&
      auto_flow_value_list->HasValue(
          *CSSIdentifierValue::Create(CSSValueID::kColumn))) {
    result.Append(template_row_values->CssText());
    result.Append(" / ");
    result.Append(auto_flow_text);

    if (specified_non_initial_auto_columns) {
      result.Append(" ");
      result.Append(auto_column_values->CssText());
    }
  } else {
    // 3- [ auto-flow && dense? ] <'grid-auto-rows'>? /
    // <'grid-template-columns'>
    result.Append(auto_flow_text);

    if (specified_non_initial_auto_rows) {
      result.Append(" ");
      result.Append(auto_row_values->CssText());
    }

    result.Append(" / ");
    result.Append(template_column_values->CssText());
  }
  return result.ReleaseString();
}

String StylePropertySerializer::GetShorthandValueForGridArea(
    const StylePropertyShorthand& shorthand) const {
  const String separator = " / ";

  DCHECK_EQ(shorthand.length(), 4u);
  const CSSValue* grid_row_start =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[0]);
  const CSSValue* grid_column_start =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[1]);
  const CSSValue* grid_row_end =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[2]);
  const CSSValue* grid_column_end =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[3]);

  // `grid-row-end` depends on `grid-row-start`, and `grid-column-end` depends
  // on on `grid-column-start`, but what's not consistent is that
  // `grid-column-start` has a dependency on `grid-row-start`. For more details,
  // see https://www.w3.org/TR/css-grid-2/#placement-shorthands
  const bool include_column_start =
      CSSOMUtils::IncludeDependentGridLineEndValue(grid_row_start,
                                                   grid_column_start);
  const bool include_row_end = CSSOMUtils::IncludeDependentGridLineEndValue(
      grid_row_start, grid_row_end);
  const bool include_column_end = CSSOMUtils::IncludeDependentGridLineEndValue(
      grid_column_start, grid_column_end);

  StringBuilder result;

  // `grid-row-start` is always included.
  result.Append(grid_row_start->CssText());

  // If `IncludeDependentGridLineEndValue` returns true for a property,
  // all preceding values must be included.
  if (include_column_start || include_row_end || include_column_end) {
    result.Append(separator);
    result.Append(grid_column_start->CssText());
  }
  if (include_row_end || include_column_end) {
    result.Append(separator);
    result.Append(grid_row_end->CssText());
  }
  if (include_column_end) {
    result.Append(separator);
    result.Append(grid_column_end->CssText());
  }

  return result.ReleaseString();
}

String StylePropertySerializer::GetShorthandValueForGridLine(
    const StylePropertyShorthand& shorthand) const {
  const String separator = " / ";

  DCHECK_EQ(shorthand.length(), 2u);
  const CSSValue* line_start =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[0]);
  const CSSValue* line_end =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[1]);

  StringBuilder result;

  // `grid-line-start` is always included.
  result.Append(line_start->CssText());
  if (CSSOMUtils::IncludeDependentGridLineEndValue(line_start, line_end)) {
    result.Append(separator);
    result.Append(line_end->CssText());
  }

  return result.ReleaseString();
}

String StylePropertySerializer::GetShorthandValueForMasonryTrack() const {
  CHECK_EQ(masonryTrackShorthand().length(), 2u);
  CHECK_EQ(masonryTrackShorthand().properties()[0],
           &GetCSSPropertyMasonryTrackStart());
  CHECK_EQ(masonryTrackShorthand().properties()[1],
           &GetCSSPropertyMasonryTrackEnd());

  const auto* track_start =
      property_set_.GetPropertyCSSValue(GetCSSPropertyMasonryTrackStart());
  const auto* track_end =
      property_set_.GetPropertyCSSValue(GetCSSPropertyMasonryTrackStart());

  StringBuilder result;

  // `masonry-track-start` is always included.
  result.Append(track_start->CssText());
  if (CSSOMUtils::IncludeDependentGridLineEndValue(track_start, track_end)) {
    result.Append(" / ");
    result.Append(track_end->CssText());
  }

  return result.ReleaseString();
}

String StylePropertySerializer::GetShorthandValueForGridTemplate(
    const StylePropertyShorthand& shorthand) const {
  const CSSValue* template_row_values =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[0]);
  const CSSValue* template_column_values =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[1]);
  const CSSValue* template_area_values =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[2]);

  const CSSValueList* grid_template_list =
      CSSOMUtils::ComputedValueForGridTemplateShorthand(
          template_row_values, template_column_values, template_area_values);
  return grid_template_list->CssText();
}

// only returns a non-null value if all properties have the same, non-null value
String StylePropertySerializer::GetCommonValue(
    const StylePropertyShorthand& shorthand) const {
  String res;
  for (const CSSProperty* const longhand : shorthand.properties()) {
    const CSSValue* value = property_set_.GetPropertyCSSValue(*longhand);
    // FIXME: CSSInitialValue::CssText should generate the right value.
    String text = value->CssText();
    if (res.IsNull()) {
      res = text;
    } else if (res != text) {
      return String();
    }
  }
  return res;
}

String StylePropertySerializer::BorderPropertyValue(
    const StylePropertyShorthand& width,
    const StylePropertyShorthand& style,
    const StylePropertyShorthand& color) const {
  const CSSProperty* border_image_properties[] = {
      &GetCSSPropertyBorderImageSource(), &GetCSSPropertyBorderImageSlice(),
      &GetCSSPropertyBorderImageWidth(), &GetCSSPropertyBorderImageOutset(),
      &GetCSSPropertyBorderImageRepeat()};

  // If any of the border-image longhands differ from their initial
  // specified values, we should not serialize to a border shorthand
  // declaration.
  for (const auto* border_image_property : border_image_properties) {
    const CSSValue* value =
        property_set_.GetPropertyCSSValue(*border_image_property);
    const CSSValue* initial_specified_value =
        To<Longhand>(*border_image_property).InitialValue();
    if (value && !value->IsInitialValue() &&
        *value != *initial_specified_value) {
      return String();
    }
  }

  const StylePropertyShorthand shorthand_properties[3] = {width, style, color};
  StringBuilder result;
  for (const auto& shorthand_property : shorthand_properties) {
    const String value = GetCommonValue(shorthand_property);
    if (value.IsNull()) {
      return String();
    }
    if (value == "initial") {
      continue;
    }
    if (!result.empty()) {
      result.Append(' ');
    }
    result.Append(value);
  }
  return result.empty() ? String() : result.ReleaseString();
}

String StylePropertySerializer::BorderImagePropertyValue() const {
  StringBuilder result;
  const CSSProperty* properties[] = {
      &GetCSSPropertyBorderImageSource(), &GetCSSPropertyBorderImageSlice(),
      &GetCSSPropertyBorderImageWidth(), &GetCSSPropertyBorderImageOutset(),
      &GetCSSPropertyBorderImageRepeat()};
  size_t index = 0;
  for (const CSSProperty* property : properties) {
    const CSSValue& value = *property_set_.GetPropertyCSSValue(*property);
    if (!result.empty()) {
      result.Append(" ");
    }
    if (index == 2 || index == 3) {
      result.Append("/ ");
    }
    result.Append(value.CssText());
    index++;
  }
  return result.ReleaseString();
}

String StylePropertySerializer::BorderRadiusValue() const {
  auto serialize = [](const CSSValue& top_left, const CSSValue& top_right,
                      const CSSValue& bottom_right,
                      const CSSValue& bottom_left) -> String {
    bool show_bottom_left = !(top_right == bottom_left);
    bool show_bottom_right = !(top_left == bottom_right) || show_bottom_left;
    bool show_top_right = !(top_left == top_right) || show_bottom_right;

    StringBuilder result;
    result.Append(top_left.CssText());
    if (show_top_right) {
      result.Append(' ');
      result.Append(top_right.CssText());
    }
    if (show_bottom_right) {
      result.Append(' ');
      result.Append(bottom_right.CssText());
    }
    if (show_bottom_left) {
      result.Append(' ');
      result.Append(bottom_left.CssText());
    }
    return result.ReleaseString();
  };

  const CSSValuePair& top_left = To<CSSValuePair>(
      *property_set_.GetPropertyCSSValue(GetCSSPropertyBorderTopLeftRadius()));
  const CSSValuePair& top_right = To<CSSValuePair>(
      *property_set_.GetPropertyCSSValue(GetCSSPropertyBorderTopRightRadius()));
  const CSSValuePair& bottom_right =
      To<CSSValuePair>(*property_set_.GetPropertyCSSValue(
          GetCSSPropertyBorderBottomRightRadius()));
  const CSSValuePair& bottom_left =
      To<CSSValuePair>(*property_set_.GetPropertyCSSValue(
          GetCSSPropertyBorderBottomLeftRadius()));

  StringBuilder builder;
  builder.Append(serialize(top_left.First(), top_right.First(),
                           bottom_right.First(), bottom_left.First()));

  if (!(top_left.First() == top_left.Second()) ||
      !(top_right.First() == top_right.Second()) ||
      !(bottom_right.First() == bottom_right.Second()) ||
      !(bottom_left.First() == bottom_left.Second())) {
    builder.Append(" / ");
    builder.Append(serialize(top_left.Second(), top_right.Second(),
                             bottom_right.Second(), bottom_left.Second()));
  }

  return builder.ReleaseString();
}

String StylePropertySerializer::PageBreakPropertyValue(
    const StylePropertyShorthand& shorthand) const {
  const CSSValue* value =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[0]);
  CSSValueID value_id = To<CSSIdentifierValue>(value)->GetValueID();
  // https://drafts.csswg.org/css-break/#page-break-properties
  if (value_id == CSSValueID::kPage) {
    return "always";
  }
  if (value_id == CSSValueID::kAuto || value_id == CSSValueID::kLeft ||
      value_id == CSSValueID::kRight || value_id == CSSValueID::kAvoid) {
    return value->CssText();
  }
  return String();
}

String StylePropertySerializer::ContainIntrinsicSizeValue() const {
  // If the two values are identical, we return just one.
  String res = GetCommonValue(containIntrinsicSizeShorthand());
  if (!res.IsNull()) {
    return res;
  }
  // Otherwise just serialize them in sequence.
  return GetShorthandValue(containIntrinsicSizeShorthand());
}

String StylePropertySerializer::TextBoxValue() const {
  const auto* trim_value = DynamicTo<CSSIdentifierValue>(
      property_set_.GetPropertyCSSValue(GetCSSPropertyTextBoxTrim()));
  CHECK(trim_value);
  const CSSValueID trim_id = trim_value->GetValueID();
  const CSSValue* edge_value =
      property_set_.GetPropertyCSSValue(GetCSSPropertyTextBoxEdge());
  CHECK(edge_value);

  // If `text-box-edge: auto`, produce `normal` or `<text-box-trim>`.
  if (const auto* edge_identifier = DynamicTo<CSSIdentifierValue>(edge_value)) {
    const CSSValueID edge_id = edge_identifier->GetValueID();
    if (edge_id == CSSValueID::kAuto) {
      if (trim_id == CSSValueID::kNone) {
        return getValueName(CSSValueID::kNormal);
      }
      return trim_value->CssText();
    }
  }

  // Omit `text-box-trim` if `trim-both`, not when it's initial.
  if (trim_id == CSSValueID::kTrimBoth) {
    return edge_value->CssText();
  }

  // Otherwise build a multi-value list.
  StringBuilder result;
  result.Append(trim_value->CssText());
  result.Append(kSpaceCharacter);
  result.Append(edge_value->CssText());
  return result.ToString();
}

String StylePropertySerializer::TextSpacingValue() const {
  const auto* autospace_value = DynamicTo<CSSIdentifierValue>(
      property_set_.GetPropertyCSSValue(GetCSSPropertyTextAutospace()));
  DCHECK(autospace_value);
  const auto* spacing_trim_value = DynamicTo<CSSIdentifierValue>(
      property_set_.GetPropertyCSSValue(GetCSSPropertyTextSpacingTrim()));
  DCHECK(spacing_trim_value);

  // Check if longhands are one of pre-defined keywords.
  const CSSValueID autospace_id = autospace_value->GetValueID();
  const CSSValueID spacing_trim_id = spacing_trim_value->GetValueID();
  if (autospace_id == CSSValueID::kNormal &&
      spacing_trim_id == CSSValueID::kNormal) {
    return getValueName(CSSValueID::kNormal);
  }
  if (autospace_id == CSSValueID::kNoAutospace &&
      spacing_trim_id == CSSValueID::kSpaceAll) {
    return getValueName(CSSValueID::kNone);
  }

  // Otherwise build a multi-value list.
  StringBuilder result;
  if (spacing_trim_id != CSSValueID::kNormal) {
    result.Append(getValueName(spacing_trim_id));
  }
  if (autospace_id != CSSValueID::kNormal) {
    if (!result.empty()) {
      result.Append(kSpaceCharacter);
    }
    result.Append(getValueName(autospace_id));
  }
  // When all longhands are initial values, it should be `normal`.
  DCHECK(!result.empty());
  return result.ToString();
}

String StylePropertySerializer::TextWrapValue() const {
  const CSSValue* mode_value =
      property_set_.GetPropertyCSSValue(GetCSSPropertyTextWrapMode());
  const CSSValue* style_value =
      property_set_.GetPropertyCSSValue(GetCSSPropertyTextWrapStyle());
  if (!mode_value || !style_value) {
    // If any longhands are missing, don't serialize as a shorthand.
    return g_empty_string;
  }

  // If `text-wrap-style` is initial, return `text-wrap-mode`.
  const TextWrapMode mode = ToTextWrapMode(mode_value);
  const TextWrapStyle style = ToTextWrapStyle(style_value);
  if (style == ComputedStyleInitialValues::InitialTextWrapStyle()) {
    return getValueName(PlatformEnumToCSSValueID(mode));
  }

  // Otherwise, if `text-wrap-mode` is initial, return `text-wrap-style`.
  if (mode == ComputedStyleInitialValues::InitialTextWrapMode()) {
    return getValueName(PlatformEnumToCSSValueID(style));
  }

  // If neither is initial, return a list.
  StringBuilder result;
  result.Append(getValueName(PlatformEnumToCSSValueID(mode)));
  result.Append(kSpaceCharacter);
  result.Append(getValueName(PlatformEnumToCSSValueID(style)));
  return result.ToString();
}

String StylePropertySerializer::WhiteSpaceValue() const {
  const CSSValue* collapse_value =
      property_set_.GetPropertyCSSValue(GetCSSPropertyWhiteSpaceCollapse());
  const CSSValue* wrap_value =
      property_set_.GetPropertyCSSValue(GetCSSPropertyTextWrapMode());
  if (!collapse_value || !wrap_value) {
    // If any longhands are missing, don't serialize as a shorthand.
    return g_empty_string;
  }

  // Check if longhands are one of pre-defined keywords of `white-space`.
  const WhiteSpaceCollapse collapse = ToWhiteSpaceCollapse(collapse_value);
  const TextWrapMode wrap = ToTextWrapMode(wrap_value);
  const EWhiteSpace whitespace = ToWhiteSpace(collapse, wrap);
  if (IsValidWhiteSpace(whitespace)) {
    return getValueName(PlatformEnumToCSSValueID(whitespace));
  }

  // Otherwise build a multi-value list.
  StringBuilder result;
  if (collapse != ComputedStyleInitialValues::InitialWhiteSpaceCollapse()) {
    result.Append(getValueName(PlatformEnumToCSSValueID(collapse)));
  }
  if (wrap != ComputedStyleInitialValues::InitialTextWrapMode()) {
    if (!result.empty()) {
      result.Append(kSpaceCharacter);
    }
    result.Append(getValueName(PlatformEnumToCSSValueID(wrap)));
  }
  // When all longhands are initial values, it should be `normal`, covered by
  // `IsValidWhiteSpace()` above.
  DCHECK(!result.empty());
  return result.ToString();
}

String StylePropertySerializer::ScrollStartValue() const {
  CHECK_EQ(scrollStartShorthand().length(), 2u);
  CHECK_EQ(scrollStartShorthand().properties()[0],
           &GetCSSPropertyScrollStartBlock());
  CHECK_EQ(scrollStartShorthand().properties()[1],
           &GetCSSPropertyScrollStartInline());

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  const CSSValue* block_value =
      property_set_.GetPropertyCSSValue(GetCSSPropertyScrollStartBlock());
  const CSSValue* inline_value =
      property_set_.GetPropertyCSSValue(GetCSSPropertyScrollStartInline());

  DCHECK(block_value);
  DCHECK(inline_value);

  list->Append(*block_value);

  if (const auto* ident_value = DynamicTo<CSSIdentifierValue>(inline_value);
      !ident_value || ident_value->GetValueID() != CSSValueID::kStart) {
    list->Append(*inline_value);
  }

  return list->CssText();
}

String StylePropertySerializer::PositionTryValue(
    const StylePropertyShorthand& shorthand) const {
  CHECK_EQ(shorthand.length(), 2u);
  CHECK_EQ(shorthand.properties()[0], &GetCSSPropertyPositionTryOrder());

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  const CSSValue* order_value =
      property_set_.GetPropertyCSSValue(GetCSSPropertyPositionTryOrder());
  const CSSValue* fallbacks_value =
      property_set_.GetPropertyCSSValue(*shorthand.properties()[1]);

  CHECK(order_value);
  CHECK(fallbacks_value);

  if (To<CSSIdentifierValue>(*order_value).GetValueID() !=
      CSSValueID::kNormal) {
    list->Append(*order_value);
  }
  list->Append(*fallbacks_value);
  return list->CssText();
}

}  // namespace blink
