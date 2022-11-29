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
      stylistic_(stylistic),
      styleset_(styleset),
      character_variant_(character_variant),
      swash_(swash),
      ornaments_(ornaments),
      annotation_(annotation) {}

StyleRuleFontFeatureValues::StyleRuleFontFeatureValues(
    const StyleRuleFontFeatureValues&) = default;

StyleRuleFontFeatureValues::~StyleRuleFontFeatureValues() = default;

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

Vector<uint32_t> StyleRuleFontFeatureValues::ResolveStylistic(
    AtomicString alias) {
  return ResolveInternal(stylistic_, alias);
}

Vector<uint32_t> StyleRuleFontFeatureValues::ResolveStyleset(
    AtomicString alias) {
  return ResolveInternal(styleset_, alias);
}

Vector<uint32_t> StyleRuleFontFeatureValues::ResolveCharacterVariant(
    AtomicString alias) {
  return ResolveInternal(character_variant_, alias);
}

Vector<uint32_t> StyleRuleFontFeatureValues::ResolveSwash(AtomicString alias) {
  return ResolveInternal(swash_, alias);
}

Vector<uint32_t> StyleRuleFontFeatureValues::ResolveOrnaments(
    AtomicString alias) {
  return ResolveInternal(ornaments_, alias);
}
Vector<uint32_t> StyleRuleFontFeatureValues::ResolveAnnotation(
    AtomicString alias) {
  return ResolveInternal(annotation_, alias);
}

Vector<uint32_t> StyleRuleFontFeatureValues::ResolveInternal(
    const FontFeatureAliases& aliases,
    AtomicString alias) {
  auto find_result = aliases.find(alias);
  if (find_result == aliases.end())
    return {};
  return find_result->value;
}

}  // namespace blink
