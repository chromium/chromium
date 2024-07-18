// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/string_keyframe.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/css/css_keyframe_shorthand_value.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

bool IsLogicalProperty(CSSPropertyID property_id) {
  const CSSProperty& property = CSSProperty::Get(property_id);
  const CSSProperty& resolved_property = property.ResolveDirectionAwareProperty(
      {WritingMode::kHorizontalTb, TextDirection::kLtr});
  return resolved_property.PropertyID() != property_id;
}

MutableCSSPropertyValueSet* CreateCssPropertyValueSet() {
  return MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
}

}  // namespace

using PropertyResolver = StringKeyframe::PropertyResolver;

StringKeyframe::StringKeyframe(const StringKeyframe& copy_from)
    : Keyframe(copy_from.offset_,
               copy_from.timeline_offset_,
               copy_from.composite_,
               copy_from.easing_),
      tree_scope_(copy_from.tree_scope_),
      input_properties_(copy_from.input_properties_),
      presentation_attribute_map_(
          copy_from.presentation_attribute_map_->MutableCopy()),
      svg_attribute_map_(copy_from.svg_attribute_map_),
      has_logical_property_(copy_from.has_logical_property_),
      writing_direction_(copy_from.writing_direction_) {
  if (copy_from.css_property_map_)
    css_property_map_ = copy_from.css_property_map_->MutableCopy();
}

MutableCSSPropertyValueSet::SetResult StringKeyframe::SetCSSPropertyValue(
    const AtomicString& custom_property_name,
    const String& value,
    SecureContextMode secure_context_mode,
    StyleSheetContents* style_sheet_contents) {
  bool is_animation_tainted = true;

  auto* property_map = CreateCssPropertyValueSet();
  MutableCSSPropertyValueSet::SetResult result =
      property_map->ParseAndSetCustomProperty(
          custom_property_name, value, false, secure_context_mode,
          style_sheet_contents, is_animation_tainted);

  const CSSValue* parsed_value =
      property_map->GetPropertyCSSValue(custom_property_name);

  if (result != MutableCSSPropertyValueSet::kParseError && parsed_value) {
    // Per specification we only keep properties around which are parsable.
    input_properties_.Set(PropertyHandle(custom_property_name),
                          MakeGarbageCollected<PropertyResolver>(
                              CSSPropertyID::kVariable, *parsed_value));
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
    return MutableCSSPropertyValueSet::kUnchanged;
  }

  auto* property_value_set = CreateCssPropertyValueSet();
  MutableCSSPropertyValueSet::SetResult result =
      property_value_set->ParseAndSetProperty(
          property_id, value, false, secure_context_mode, style_sheet_contents);

  // TODO(crbug.com/1132078): Add flag to CSSProperty to track if it is for a
  // logical style.
  bool is_logical = false;
  if (property.IsShorthand()) {
    // Logical shorthands to not directly map to physical shorthands. Determine
    // if the shorthand is for a logical property by checking the first
    // longhand.
    if (property_value_set->PropertyCount()) {
      CSSPropertyValueSet::PropertyReference reference =
          property_value_set->PropertyAt(0);
      if (IsLogicalProperty(reference.Id()))
        is_logical = true;
    }
  } else {
    is_logical = IsLogicalProperty(property_id);
  }
  if (is_logical)
    has_logical_property_ = true;

  if (result != MutableCSSPropertyValueSet::kParseError) {
    // Per specification we only keep properties around which are parsable.
    auto* resolver = MakeGarbageCollected<PropertyResolver>(
        property, property_value_set, is_logical);
    if (resolver->IsValid()) {
      input_properties_.Set(PropertyHandle(property), resolver);
      InvalidateCssPropertyMap();
    }
  }

  return result;
}

void StringKeyframe::SetCSSPropertyValue(const CSSPropertyName& name,
                                         const CSSValue& value) {
  CSSPropertyID property_id = name.Id();
  DCHECK_NE(property_id, CSSPropertyID::kInvalid);
#if DCHECK_IS_ON()
  if (property_id != CSSPropertyID::kVariable) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    DCHECK(!CSSAnimations::IsAnimationAffectingProperty(property));
    DCHECK(!property.IsShorthand());
  }
#endif  // DCHECK_IS_ON()
  DCHECK(!IsLogicalProperty(property_id));
  input_properties_.Set(
      PropertyHandle(name),
      MakeGarbageCollected<PropertyResolver>(property_id, value));
  InvalidateCssPropertyMap();
}

void StringKeyframe::RemoveCustomCSSProperty(const PropertyHandle& property) {
  DCHECK(property.IsCSSCustomProperty());
  if (css_property_map_)
    css_property_map_->RemoveProperty(property.CustomPropertyName());
  input_properties_.erase(property);
}

void StringKeyframe::SetPresentationAttributeValue(
    const CSSProperty& property,
    const String& value,
    SecureContextMode secure_context_mode,
    StyleSheetContents* style_sheet_contents) {
  DCHECK_NE(property.PropertyID(), CSSPropertyID::kInvalid);
  if (!CSSAnimations::IsAnimationAffectingProperty(property)) {
    presentation_attribute_map_->ParseAndSetProperty(
        property.PropertyID(), value, false, secure_context_mode,
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
  EnsureCssPropertyMap();
  PropertyHandleSet properties;

  for (unsigned i = 0; i < css_property_map_->PropertyCount(); ++i) {
    CSSPropertyValueSet::PropertyReference property_reference =
        css_property_map_->PropertyAt(i);
    const CSSPropertyName& name = property_reference.Name();
    DCHECK(!name.IsCustomProperty() ||
           !CSSProperty::Get(name.Id()).IsShorthand())
        << "Web Animations: Encountered unexpanded shorthand CSS property ("
        << static_cast<int>(name.Id()) << ").";
    properties.insert(PropertyHandle(name));
  }

  for (unsigned i = 0; i < presentation_attribute_map_->PropertyCount(); ++i) {
    properties.insert(PropertyHandle(
        CSSProperty::Get(presentation_attribute_map_->PropertyAt(i).Id()),
        true));
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
    V8ObjectBuilder& object_builder,
    Element* element) const {
  Keyframe::AddKeyframePropertiesToV8Object(object_builder, element);
  for (const auto& entry : input_properties_) {
    const PropertyHandle& property_handle = entry.key;
    const CSSValue* property_value = entry.value->CssValue();
    String property_name =
        AnimationInputHelpers::PropertyHandleToKeyframeAttribute(
            property_handle);

    object_builder.AddString(property_name, property_value->CssText());
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
    object_builder.AddString(property_name, property_value);
  }
}

void StringKeyframe::Trace(Visitor* visitor) const {
  visitor->Trace(tree_scope_);
  visitor->Trace(input_properties_);
  visitor->Trace(css_property_map_);
  visitor->Trace(presentation_attribute_map_);
  Keyframe::Trace(visitor);
}

Keyframe* StringKeyframe::Clone() const {
  return MakeGarbageCollected<StringKeyframe>(*this);
}

bool StringKeyframe::SetLogicalPropertyResolutionContext(
    WritingDirectionMode writing_direction) {
  if (writing_direction != writing_direction_) {
    writing_direction_ = writing_direction;
    if (has_logical_property_) {
      // force a rebuild of the property map on the next property fetch.
      InvalidateCssPropertyMap();
      return true;
    }
  }
  return false;
}

void StringKeyframe::EnsureCssPropertyMap() const {
  if (css_property_map_)
    return;

  css_property_map_ =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);

  bool requires_sorting = false;
  HeapVector<Member<PropertyResolver>> resolvers;
  for (const auto& entry : input_properties_) {
    const PropertyHandle& property_handle = entry.key;
    if (!property_handle.IsCSSProperty())
      continue;

    if (property_handle.IsCSSCustomProperty()) {
      CSSPropertyName property_name(property_handle.CustomPropertyName());
      const CSSValue* value = entry.value->CssValue();
      css_property_map_->SetLonghandProperty(
          CSSPropertyValue(property_name, *value));
    } else {
      PropertyResolver* resolver = entry.value;
      if (resolver->IsLogical() || resolver->IsShorthand())
        requires_sorting = true;
      resolvers.push_back(resolver);
    }
  }

  if (requires_sorting) {
    std::stable_sort(resolvers.begin(), resolvers.end(),
                     PropertyResolver::HasLowerPriority);
  }

  for (const auto& resolver : resolvers) {
    resolver->AppendTo(css_property_map_, writing_direction_);
  }
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
          element, base_style, parent_style, property, value_.Get(), offset_);
  return true;
}

bool StringKeyframe::CSSPropertySpecificKeyframe::IsRevert() const {
  return value_ && value_->IsRevertValue();
}

bool StringKeyframe::CSSPropertySpecificKeyframe::IsRevertLayer() const {
  return value_ && value_->IsRevertLayerValue();
}

Keyframe::PropertySpecificKeyframe*
StringKeyframe::CSSPropertySpecificKeyframe::NeutralKeyframe(
    double offset,
    scoped_refptr<TimingFunction> easing) const {
  return MakeGarbageCollected<CSSPropertySpecificKeyframe>(
      offset, std::move(easing), nullptr, EffectModel::kCompositeAdd);
}

void StringKeyframe::CSSPropertySpecificKeyframe::Trace(
    Visitor* visitor) const {
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

// ----- Property Resolver -----

PropertyResolver::PropertyResolver(CSSPropertyID property_id,
                                   const CSSValue& css_value)
    : property_id_(property_id), css_value_(css_value) {}

PropertyResolver::PropertyResolver(
    const CSSProperty& property,
    const MutableCSSPropertyValueSet* property_value_set,
    bool is_logical)
    : property_id_(property.PropertyID()), is_logical_(is_logical) {
  DCHECK_NE(property_id_, CSSPropertyID::kInvalid);
  DCHECK_NE(property_id_, CSSPropertyID::kVariable);
  if (!property.IsShorthand())
    css_value_ = property_value_set->GetPropertyCSSValue(property_id_);
  else
    css_property_value_set_ = property_value_set->ImmutableCopyIfNeeded();
}

bool PropertyResolver::IsValid() const {
  return css_value_ || css_property_value_set_;
}

const CSSValue* PropertyResolver::CssValue() {
  DCHECK(IsValid());

  if (css_value_)
    return css_value_.Get();

  // For shorthands create a special wrapper value, |CSSKeyframeShorthandValue|,
  // which can be used to correctly serialize it given longhands that are
  // present in this set.
  css_value_ = MakeGarbageCollected<CSSKeyframeShorthandValue>(
      property_id_, css_property_value_set_);
  return css_value_.Get();
}

void PropertyResolver::AppendTo(MutableCSSPropertyValueSet* property_value_set,
                                WritingDirectionMode writing_direction) {
  DCHECK(property_id_ != CSSPropertyID::kInvalid);
  DCHECK(property_id_ != CSSPropertyID::kVariable);

  if (css_property_value_set_) {
    // Shorthand property. Extract longhands from css_property_value_set_.
    if (is_logical_) {
      // Walk set of properties converting each property name to its
      // corresponding physical property.
      for (unsigned i = 0; i < css_property_value_set_->PropertyCount(); i++) {
        CSSPropertyValueSet::PropertyReference reference =
            css_property_value_set_->PropertyAt(i);
        SetProperty(property_value_set, reference.Id(), reference.Value(),
                    writing_direction);
      }
    } else {
      property_value_set->MergeAndOverrideOnConflict(css_property_value_set_);
    }
  } else {
    SetProperty(property_value_set, property_id_, *css_value_,
                writing_direction);
  }
}

void PropertyResolver::SetProperty(
    MutableCSSPropertyValueSet* property_value_set,
    CSSPropertyID property_id,
    const CSSValue& value,
    WritingDirectionMode writing_direction) {
  const CSSProperty& physical_property =
      CSSProperty::Get(property_id)
          .ResolveDirectionAwareProperty(writing_direction);
  property_value_set->SetProperty(physical_property.PropertyID(), value);
}

void PropertyResolver::Trace(Visitor* visitor) const {
  visitor->Trace(css_value_);
  visitor->Trace(css_property_value_set_);
}

// static
bool PropertyResolver::HasLowerPriority(PropertyResolver* first,
                                        PropertyResolver* second) {
  // Longhand properties take precedence over shorthand properties.
  if (first->IsShorthand() != second->IsShorthand())
    return first->IsShorthand();

  // Physical properties take precedence over logical properties.
  if (first->IsLogical() != second->IsLogical())
    return first->IsLogical();

  // Two shorthands with overlapping longhand properties are sorted based
  // on the number of longhand properties in their expansions.
  if (first->IsShorthand())
    return first->ExpansionCount() > second->ExpansionCount();

  return false;
}

}  // namespace blink
