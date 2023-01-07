// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_keyframe_effect_model.h"

#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"
#include "third_party/blink/renderer/core/animation/animation_utils.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"

namespace blink {

namespace {

using MissingPropertyValueMap = HashMap<String, String>;

void ResolveUnderlyingPropertyValues(Element& element,
                                     const PropertyHandleSet& properties,
                                     MissingPropertyValueMap& map) {
  // The element's computed style may be null if the element has been removed
  // form the DOM tree.
  if (!element.GetComputedStyle())
    return;

  // TODO(crbug.com/1069235): Should sample the underlying animation.
  ActiveInterpolationsMap empty_interpolations_map;
  AnimationUtils::ForEachInterpolatedPropertyValue(
      &element, properties, empty_interpolations_map,
      WTF::BindRepeating(
          [](MissingPropertyValueMap* map, PropertyHandle property,
             const CSSValue* value) {
            if (property.IsCSSProperty()) {
              String property_name =
                  AnimationInputHelpers::PropertyHandleToKeyframeAttribute(
                      property);
              map->Set(property_name, value->CssText());
            }
          },
          WTF::Unretained(&map)));
}

void AddMissingProperties(const MissingPropertyValueMap& property_map,
                          const PropertyHandleSet& all_properties,
                          const PropertyHandleSet& keyframe_properties,
                          StringKeyframe* keyframe) {
  for (const auto& property : all_properties) {
    // At present, custom properties are to be excluded from the keyframes.
    // https://github.com/w3c/csswg-drafts/issues/5126.
    if (property.IsCSSCustomProperty())
      continue;

    if (keyframe_properties.Contains(property))
      continue;

    String property_name =
        AnimationInputHelpers::PropertyHandleToKeyframeAttribute(property);
    if (property_map.Contains(property_name)) {
      const String& value = property_map.at(property_name);
      keyframe->SetCSSPropertyValue(property.GetCSSProperty().PropertyID(),
                                    value, SecureContextMode::kInsecureContext,
                                    nullptr);
    }
  }
}

void ResolveComputedValues(Element* element, StringKeyframe* keyframe) {
  DCHECK(element);
  // Styles are flushed when getKeyframes is called on a CSS animation.
  // The element's computed style may be null if detached from the DOM tree.
  if (!element->GetComputedStyle())
    return;

  for (const auto& property : keyframe->Properties()) {
    if (property.IsCSSCustomProperty()) {
      // At present, custom properties are to be excluded from the keyframes.
      // https://github.com/w3c/csswg-drafts/issues/5126.
      // TODO(csswg/issues/5126): Revisit once issue regarding inclusion of
      // custom properties is resolved. Perhaps registered should likely be
      // included since they can be animated in Blink. Pruning unregistered
      // variables seems justifiable.
      keyframe->RemoveCustomCSSProperty(property);
    } else if (property.IsCSSProperty()) {
      const CSSValue& value = keyframe->CssPropertyValue(property);
      const CSSPropertyName property_name = property.GetCSSPropertyName();
      const CSSValue* computed_value =
          StyleResolver::ComputeValue(element, property_name, value);
      if (computed_value)
        keyframe->SetCSSPropertyValue(property_name, *computed_value);
    }
  }
}

}  // namespace

KeyframeEffectModelBase::KeyframeVector
CssKeyframeEffectModel::GetComputedKeyframes(Element* element) {
  const KeyframeEffectModelBase::KeyframeVector& keyframes = GetFrames();
  if (!element)
    return keyframes;

  KeyframeEffectModelBase::KeyframeVector computed_keyframes;

  // Lazy resolution of values for missing properties.
  PropertyHandleSet all_properties = Properties();
  PropertyHandleSet from_properties;
  PropertyHandleSet to_properties;

  Vector<double> computed_offsets =
      KeyframeEffectModelBase::GetComputedOffsets(keyframes);
  computed_keyframes.ReserveInitialCapacity(keyframes.size());
  for (wtf_size_t i = 0; i < keyframes.size(); i++) {
    Keyframe* keyframe = keyframes[i];
    // TODO(crbug.com/1070627): Use computed values, prune variable references,
    // and convert logical properties to physical properties.
    StringKeyframe* computed_keyframe = To<StringKeyframe>(keyframe->Clone());
    ResolveComputedValues(element, computed_keyframe);
    computed_keyframes.push_back(computed_keyframe);
    double offset = computed_offsets[i];
    if (offset == 0) {
      for (const auto& property : computed_keyframe->Properties()) {
        from_properties.insert(property);
      }
    } else if (offset == 1) {
      for (const auto& property : computed_keyframe->Properties()) {
        to_properties.insert(property);
      }
    }
  }

  // Add missing properties from the bounding keyframes.
  MissingPropertyValueMap missing_property_value_map;
  if (from_properties.size() < all_properties.size() ||
      to_properties.size() < all_properties.size()) {
    ResolveUnderlyingPropertyValues(*element, all_properties,
                                    missing_property_value_map);
  }
  if (from_properties.size() < all_properties.size() &&
      !computed_keyframes.empty()) {
    AddMissingProperties(
        missing_property_value_map, all_properties, from_properties,
        DynamicTo<StringKeyframe>(computed_keyframes[0].Get()));
  }
  if (to_properties.size() < all_properties.size() &&
      !computed_keyframes.empty()) {
    wtf_size_t index = keyframes.size() - 1;
    AddMissingProperties(
        missing_property_value_map, all_properties, to_properties,
        DynamicTo<StringKeyframe>(computed_keyframes[index].Get()));
  }
  return computed_keyframes;
}

}  // namespace blink
