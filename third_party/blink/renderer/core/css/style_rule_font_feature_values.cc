// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule_font_feature_values.h"
#include "third_party/blink/renderer/core/css/cascade_layer.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#include <limits>

namespace blink {

StyleRuleFontFeature::StyleRuleFontFeature(
    StyleRuleFontFeature::FeatureType type)
    : StyleRuleBase(kFontFeature), type_(type) {}

StyleRuleFontFeature::StyleRuleFontFeature(const StyleRuleFontFeature&) =
    default;
StyleRuleFontFeature::~StyleRuleFontFeature() = default;

void StyleRuleFontFeature::TraceAfterDispatch(blink::Visitor* visitor) const {
  StyleRuleBase::TraceAfterDispatch(visitor);
}

void StyleRuleFontFeature::UpdateAlias(AtomicString alias,
                                       Vector<uint32_t> features) {
  feature_aliases_.Set(
      alias, FeatureIndicesWithPriority{std::move(features),
                                        std::numeric_limits<uint16_t>::max()});
}

void StyleRuleFontFeature::OverrideAliasesIn(FontFeatureAliases& destination) {
  for (const auto& hash_entry : feature_aliases_) {
    destination.Set(hash_entry.key, hash_entry.value);
  }
}

FontFeatureValuesStorage::FontFeatureValuesStorage(
    FontFeatureAliases stylistic,
    FontFeatureAliases styleset,
    FontFeatureAliases character_variant,
    FontFeatureAliases swash,
    FontFeatureAliases ornaments,
    FontFeatureAliases annotation)
    : stylistic_(stylistic),
      styleset_(styleset),
      character_variant_(character_variant),
      swash_(swash),
      ornaments_(ornaments),
      annotation_(annotation) {}

Vector<uint32_t> FontFeatureValuesStorage::ResolveStylistic(
    const AtomicString& alias) const {
  return ResolveInternal(stylistic_, alias);
}

Vector<uint32_t> FontFeatureValuesStorage::ResolveStyleset(
    const AtomicString& alias) const {
  return ResolveInternal(styleset_, alias);
}

Vector<uint32_t> FontFeatureValuesStorage::ResolveCharacterVariant(
    const AtomicString& alias) const {
  return ResolveInternal(character_variant_, alias);
}

Vector<uint32_t> FontFeatureValuesStorage::ResolveSwash(
    const AtomicString& alias) const {
  return ResolveInternal(swash_, alias);
}

Vector<uint32_t> FontFeatureValuesStorage::ResolveOrnaments(
    const AtomicString& alias) const {
  return ResolveInternal(ornaments_, alias);
}
Vector<uint32_t> FontFeatureValuesStorage::ResolveAnnotation(
    const AtomicString& alias) const {
  return ResolveInternal(annotation_, alias);
}

void FontFeatureValuesStorage::SetLayerOrder(uint16_t layer_order) {
  auto set_layer_order = [layer_order](FontFeatureAliases& aliases) {
    for (auto& entry : aliases) {
      entry.value.layer_order = layer_order;
    }
  };

  set_layer_order(stylistic_);
  set_layer_order(styleset_);
  set_layer_order(character_variant_);
  set_layer_order(swash_);
  set_layer_order(ornaments_);
  set_layer_order(annotation_);
}

void FontFeatureValuesStorage::FuseUpdate(const FontFeatureValuesStorage& other,
                                          unsigned other_layer_order) {
  auto merge_maps = [other_layer_order](FontFeatureAliases& own,
                                        const FontFeatureAliases& other) {
    for (auto& entry : other) {
      FeatureIndicesWithPriority entry_updated_order(entry.value);
      entry_updated_order.layer_order = other_layer_order;
      auto insert_result = own.insert(entry.key, entry_updated_order);
      if (!insert_result.is_new_entry) {
        unsigned existing_layer_order =
            insert_result.stored_value->value.layer_order;
        if (other_layer_order >= existing_layer_order) {
          insert_result.stored_value->value = entry_updated_order;
        }
      }
    }
  };

  merge_maps(stylistic_, other.stylistic_);
  merge_maps(styleset_, other.styleset_);
  merge_maps(character_variant_, other.character_variant_);
  merge_maps(swash_, other.swash_);
  merge_maps(ornaments_, other.ornaments_);
  merge_maps(annotation_, other.annotation_);
}

/* static */
Vector<uint32_t> FontFeatureValuesStorage::ResolveInternal(
    const FontFeatureAliases& aliases,
    const AtomicString& alias) {
  auto find_result = aliases.find(alias);
  if (find_result == aliases.end()) {
    return {};
  }
  return find_result->value.indices;
}

StyleRuleFontFeatureValues::StyleRuleFontFeatureValues(
    Vector<AtomicString> families,
    FontFeatureAliases stylistic,
    FontFeatureAliases styleset,
    FontFeatureAliases character_variant,
    FontFeatureAliases swash,
    FontFeatureAliases ornaments,
    FontFeatureAliases annotation)
    : StyleRuleBase(kFontFeatureValues),
      families_(std::move(families)),
      feature_values_storage_(stylistic,
                              styleset,
                              character_variant,
                              swash,
                              ornaments,
                              annotation) {}

StyleRuleFontFeatureValues::StyleRuleFontFeatureValues(
    const StyleRuleFontFeatureValues&) = default;

StyleRuleFontFeatureValues::~StyleRuleFontFeatureValues() = default;

void StyleRuleFontFeatureValues::SetFamilies(Vector<AtomicString> families) {
  families_ = std::move(families);
}

String StyleRuleFontFeatureValues::FamilyAsString() const {
  StringBuilder families;
  for (wtf_size_t i = 0; i < families_.size(); ++i) {
    families.Append(SerializeFontFamily(families_[i]));
    if (i < families_.size() - 1) {
      families.Append(", ");
    }
  }
  return families.ReleaseString();
}

void StyleRuleFontFeatureValues::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  StyleRuleBase::TraceAfterDispatch(visitor);
  visitor->Trace(layer_);
}

}  // namespace blink
