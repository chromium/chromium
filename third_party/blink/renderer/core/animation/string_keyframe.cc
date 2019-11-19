// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/string_keyframe.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_keyframe_shorthand_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

// Returns handle for the given CSSProperty.
// |value| is required only for custom properties.
PropertyHandle ToPropertyHandle(const CSSProperty& property,
                                const CSSValue* value) {
  if (property.IDEquals(CSSPropertyID::kVariable)) {
    return PropertyHandle(To<CSSCustomPropertyDeclaration>(*value).GetName());
  } else {
    return PropertyHandle(property, false);
  }
}

const CSSValue* GetOrCreateCSSValueFrom(
    const CSSProperty& property,
    const MutableCSSPropertyValueSet& property_value_set) {
  DCHECK_NE(property.PropertyID(), CSSPropertyID::kInvalid);
  DCHECK_NE(property.PropertyID(), CSSPropertyID::kVariable);
  if (!property.IsShorthand())
    return property_value_set.GetPropertyCSSValue(property.PropertyID());

  // For shorthands create a special wrapper value, |CSSKeyframeShorthandValue|,
  // which can be used to correctly serialize it given longhands that are
  // present in this set.
  return MakeGarbageCollected<CSSKeyframeShorthandValue>(
      property.PropertyID(), property_value_set.ImmutableCopyIfNeeded());
}

}  // namespace

StringKeyframe::StringKeyframe(const StringKeyframe& copy_from)
    : Keyframe(copy_from.offset_, copy_from.composite_, copy_from.easing_),
      input_properties_(copy_from.input_properties_),
      css_property_map_(copy_from.css_property_map_->MutableCopy()),
      presentation_attribute_map_(
          copy_from.presentation_attribute_map_->MutableCopy()),
      svg_attribute_map_(copy_from.svg_attribute_map_) {}

MutableCSSPropertyValueSet::SetResult StringKeyframe::SetCSSPropertyValue(
    const AtomicString& property_name,
    const String& value,
    SecureContextMode secure_context_mode,
    StyleSheetContents* style_sheet_contents) {
  bool is_animation_tainted = true;
  MutableCSSPropertyValueSet::SetResult result = css_property_map_->SetProperty(
      property_name, value, false, secure_context_mode, style_sheet_contents,
      is_animation_tainted);

  const CSSValue* parsed_value =
      css_property_map_->GetPropertyCSSValue(property_name);

  if (result.did_parse && parsed_value) {
    // Per specification we only keep properties around which are parsable.
    input_properties_.Set(PropertyHandle(property_name), *parsed_value);
  }

  return result;
}

MutableCSSPropertyValueSet::SetResult StringKeyframe::SetCSSPropertyValue(
    CSSPropertyID property_id,
    const String& value,
    SecureContextMode secure_context_mode,
    StyleSheetContents* style_sheet_contents) {
  DCHECK_NE(property_id, CSSPropertyID::kInvalid);
  DCHECK_NE(property_id, CSSPropertyID::kVariable);
  const CSSProperty& property = CSSProperty::Get(property_id);

  if (CSSAnimations::IsAnimationAffectingProperty(property)) {
    bool did_parse = true;
    bool did_change = false;
    return MutableCSSPropertyValueSet::SetResult{did_parse, did_change};
  }

  // Use a temporary set for shorthands so that its longhands are stored
  // separately and can later be used to construct a special shorthand value.
  bool use_temporary_set = property.IsShorthand();

  auto* property_value_set =
      use_temporary_set ? MakeGarbageCollected<MutableCSSPropertyValueSet>(
                              css_property_map_->CssParserMode())
                        : css_property_map_.Get();

  MutableCSSPropertyValueSet::SetResult result =
      property_value_set->SetProperty(
          property_id, value, false, secure_context_mode, style_sheet_contents);

  const CSSValue* parsed_value =
      GetOrCreateCSSValueFrom(property, *property_value_set);
  if (result.did_parse && parsed_value) {
    // Per specification we only keep properties around which are parsable.
    input_properties_.Set(PropertyHandle(property), parsed_value);
  }

  if (use_temporary_set)
    css_property_map_->MergeAndOverrideOnConflict(property_value_set);

  return result;
}

void StringKeyframe::SetCSSPropertyValue(const CSSProperty& property,
                                         const CSSValue& value) {
  DCHECK_NE(property.PropertyID(), CSSPropertyID::kInvalid);
  DCHECK(!CSSAnimations::IsAnimationAffectingProperty(property));
  input_properties_.Set(ToPropertyHandle(property, &value), value);
  css_property_map_->SetProperty(property.PropertyID(), value, false);
}

void StringKeyframe::SetPresentationAttributeValue(
    const CSSProperty& property,
    const String& value,
    SecureContextMode secure_context_mode,
    StyleSheetContents* style_sheet_contents) {
  DCHECK_NE(property.PropertyID(), CSSPropertyID::kInvalid);
  if (!CSSAnimations::IsAnimationAffectingProperty(property)) {
    presentation_attribute_map_->SetProperty(property.PropertyID(), value,
                                             false, secure_context_mode,
                                             style_sheet_contents);
  }
}

void StringKeyframe::SetSVGAttributeValue(const QualifiedName& attribute_name,
                                          const String& value) {
  svg_attribute_map_.Set(&attribute_name, value);
}

PropertyHandleSet StringKeyframe::Properties() const {
  // This is not used in time-critical code, so we probably don't need to
  // worry about caching this result.
  PropertyHandleSet properties;
  for (unsigned i = 0; i < css_property_map_->PropertyCount(); ++i) {
    CSSPropertyValueSet::PropertyReference property_reference =
        css_property_map_->PropertyAt(i);
    const CSSProperty& property = property_reference.Property();
    DCHECK(!property.IsShorthand())
        << "Web Animations: Encountered unexpanded shorthand CSS property ("
        << static_cast<int>(property.PropertyID()) << ").";
    properties.insert(ToPropertyHandle(property, &property_reference.Value()));
  }

  for (unsigned i = 0; i < presentation_attribute_map_->PropertyCount(); ++i) {
    properties.insert(PropertyHandle(
        presentation_attribute_map_->PropertyAt(i).Property(), true));
  }

  for (auto* const key : svg_attribute_map_.Keys())
    properties.insert(PropertyHandle(*key));

  return properties;
}

bool StringKeyframe::HasCssProperty() const {
  PropertyHandleSet properties = Properties();
  for (const PropertyHandle& property : properties) {
    if (property.IsCSSProperty())
      return true;
  }
  return false;
}

void StringKeyframe::AddKeyframePropertiesToV8Object(
    V8ObjectBuilder& object_builder) const {
  Keyframe::AddKeyframePropertiesToV8Object(object_builder);
  for (const auto& entry : input_properties_) {
    const PropertyHandle& property_handle = entry.key;
    const CSSValue* property_value = entry.value;
    String property_name =
        AnimationInputHelpers::PropertyHandleToKeyframeAttribute(
            property_handle);

    object_builder.Add(property_name, property_value->CssText());
  }

  // Legacy code path for SVG and Presentation attributes.
  //
  // TODO(816956): Move these to input_properties_ and remove this. Note that
  // this code path is not well tested given that removing it didn't cause any
  // test failures.
  for (const PropertyHandle& property : Properties()) {
    if (property.IsCSSProperty())
      continue;

    String property_name =
        AnimationInputHelpers::PropertyHandleToKeyframeAttribute(property);
    String property_value;
    if (property.IsPresentationAttribute()) {
      const auto& attribute = property.PresentationAttribute();
      property_value = PresentationAttributeValue(attribute).CssText();
    } else {
      DCHECK(property.IsSVGAttribute());
      property_value = SvgPropertyValue(property.SvgAttribute());
    }
    object_builder.Add(property_name, property_value);
  }
}

void StringKeyframe::Trace(Visitor* visitor) {
  visitor->Trace(input_properties_);
  visitor->Trace(css_property_map_);
  visitor->Trace(presentation_attribute_map_);
  Keyframe::Trace(visitor);
}

Keyframe* StringKeyframe::Clone() const {
  return MakeGarbageCollected<StringKeyframe>(*this);
}

Keyframe::PropertySpecificKeyframe*
StringKeyframe::CreatePropertySpecificKeyframe(
    const PropertyHandle& property,
    EffectModel::CompositeOperation effect_composite,
    double offset) const {
  EffectModel::CompositeOperation composite =
      composite_.value_or(effect_composite);
  if (property.IsCSSProperty()) {
    return MakeGarbageCollected<CSSPropertySpecificKeyframe>(
        offset, &Easing(), &CssPropertyValue(property), composite);
  }

  if (property.IsPresentationAttribute()) {
    return MakeGarbageCollected<CSSPropertySpecificKeyframe>(
        offset, &Easing(),
        &PresentationAttributeValue(property.PresentationAttribute()),
        composite);
  }

  DCHECK(property.IsSVGAttribute());
  return MakeGarbageCollected<SVGPropertySpecificKeyframe>(
      offset, &Easing(), SvgPropertyValue(property.SvgAttribute()), composite);
}

bool StringKeyframe::CSSPropertySpecificKeyframe::
    PopulateCompositorKeyframeValue(const PropertyHandle& property,
                                    Element& element,
                                    const ComputedStyle& base_style,
                                    const ComputedStyle* parent_style) const {
  compositor_keyframe_value_cache_ =
      StyleResolver::CreateCompositorKeyframeValueSnapshot(
          element, base_style, parent_style, property, value_.Get());
  return true;
}

Keyframe::PropertySpecificKeyframe*
StringKeyframe::CSSPropertySpecificKeyframe::NeutralKeyframe(
    double offset,
    scoped_refptr<TimingFunction> easing) const {
  return MakeGarbageCollected<CSSPropertySpecificKeyframe>(
      offset, std::move(easing), nullptr, EffectModel::kCompositeAdd);
}

void StringKeyframe::CSSPropertySpecificKeyframe::Trace(Visitor* visitor) {
  visitor->Trace(value_);
  visitor->Trace(compositor_keyframe_value_cache_);
  Keyframe::PropertySpecificKeyframe::Trace(visitor);
}

Keyframe::PropertySpecificKeyframe*
StringKeyframe::CSSPropertySpecificKeyframe::CloneWithOffset(
    double offset) const {
  auto* clone = MakeGarbageCollected<CSSPropertySpecificKeyframe>(
      offset, easing_, value_.Get(), composite_);
  clone->compositor_keyframe_value_cache_ = compositor_keyframe_value_cache_;
  return clone;
}

Keyframe::PropertySpecificKeyframe*
SVGPropertySpecificKeyframe::CloneWithOffset(double offset) const {
  return MakeGarbageCollected<SVGPropertySpecificKeyframe>(offset, easing_,
                                                           value_, composite_);
}

Keyframe::PropertySpecificKeyframe*
SVGPropertySpecificKeyframe::NeutralKeyframe(
    double offset,
    scoped_refptr<TimingFunction> easing) const {
  return MakeGarbageCollected<SVGPropertySpecificKeyframe>(
      offset, std::move(easing), String(), EffectModel::kCompositeAdd);
}

}  // namespace blink
