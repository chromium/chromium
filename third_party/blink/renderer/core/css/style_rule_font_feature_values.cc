// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule_font_feature_values.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

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
                                       const Vector<uint32_t>& features) {
  feature_aliases_.Set(alias, features);
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
    AtomicString alias) const {
  return ResolveInternal(stylistic_, alias);
}

Vector<uint32_t> FontFeatureValuesStorage::ResolveStyleset(
    AtomicString alias) const {
  return ResolveInternal(styleset_, alias);
}

Vector<uint32_t> FontFeatureValuesStorage::ResolveCharacterVariant(
    AtomicString alias) const {
  return ResolveInternal(character_variant_, alias);
}

Vector<uint32_t> FontFeatureValuesStorage::ResolveSwash(
    AtomicString alias) const {
  return ResolveInternal(swash_, alias);
}

Vector<uint32_t> FontFeatureValuesStorage::ResolveOrnaments(
    AtomicString alias) const {
  return ResolveInternal(ornaments_, alias);
}
Vector<uint32_t> FontFeatureValuesStorage::ResolveAnnotation(
    AtomicString alias) const {
  return ResolveInternal(annotation_, alias);
}

void FontFeatureValuesStorage::FuseUpdate(
    const FontFeatureValuesStorage& other) {
  auto merge_maps = [](FontFeatureAliases& own,
                       const FontFeatureAliases& other) {
    for (auto entry : other) {
      own.Set(entry.key, entry.value);
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
    AtomicString alias) {
  auto find_result = aliases.find(alias);
  if (find_result == aliases.end())
    return {};
  return find_result->value;
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
    families.Append(families_[i]);
    if (i < families_.size() - 1)
      families.Append(", ");
  }
  return families.ReleaseString();
}

void StyleRuleFontFeatureValues::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
