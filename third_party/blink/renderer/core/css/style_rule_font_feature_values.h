// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_FONT_FEATURE_VALUES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_FONT_FEATURE_VALUES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

using FontFeatureAliases = HashMap<AtomicString, Vector<uint32_t>>;

class CORE_EXPORT StyleRuleFontFeature : public StyleRuleBase {
 public:
  enum class FeatureType {
    kStylistic,
    kStyleset,
    kCharacterVariant,
    kSwash,
    kOrnaments,
    kAnnotation
  };

  explicit StyleRuleFontFeature(FeatureType);
  StyleRuleFontFeature(const StyleRuleFontFeature&);
  ~StyleRuleFontFeature();

  void UpdateAlias(AtomicString alias, const Vector<uint32_t>& features);

  void OverrideAliasesIn(FontFeatureAliases& destination);

  FeatureType GetFeatureType() { return type_; }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  FeatureType type_;
  FontFeatureAliases feature_aliases_;
};

template <>
struct DowncastTraits<StyleRuleFontFeature> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsFontFeatureRule();
  }
};

class CORE_EXPORT StyleRuleFontFeatureValues : public StyleRuleBase {
 public:
  StyleRuleFontFeatureValues(Vector<AtomicString> families,
                             FontFeatureAliases stylistic,
                             FontFeatureAliases styleset,
                             FontFeatureAliases character_variant,
                             FontFeatureAliases swash,
                             FontFeatureAliases ornaments,
                             FontFeatureAliases annotation);
  StyleRuleFontFeatureValues(const StyleRuleFontFeatureValues&);
  ~StyleRuleFontFeatureValues();

  const Vector<AtomicString>& GetFamilies() const { return families_; }
  String FamilyAsString() const;

  void SetFamilies(Vector<AtomicString>);

  StyleRuleFontFeatureValues* Copy() const {
    return MakeGarbageCollected<StyleRuleFontFeatureValues>(*this);
  }

  Vector<uint32_t> ResolveStylistic(AtomicString);
  Vector<uint32_t> ResolveStyleset(AtomicString);
  Vector<uint32_t> ResolveCharacterVariant(AtomicString);
  Vector<uint32_t> ResolveSwash(AtomicString);
  Vector<uint32_t> ResolveOrnaments(AtomicString);
  Vector<uint32_t> ResolveAnnotation(AtomicString);

  // Accessors needed for cssom implementation.
  FontFeatureAliases* GetStylistic() { return &stylistic_; }
  FontFeatureAliases* GetStyleset() { return &styleset_; }
  FontFeatureAliases* GetCharacterVariant() { return &character_variant_; }
  FontFeatureAliases* GetSwash() { return &swash_; }
  FontFeatureAliases* GetOrnaments() { return &ornaments_; }
  FontFeatureAliases* GetAnnotation() { return &annotation_; }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  Vector<uint32_t> ResolveInternal(const FontFeatureAliases&, AtomicString);
  // TODO(https://crbug.com/716567): Only styleset and character variant take
  // two values for each alias, the others take 1 value. Consider reducing
  // storage here.
  Vector<AtomicString> families_;
  FontFeatureAliases stylistic_;
  FontFeatureAliases styleset_;
  FontFeatureAliases character_variant_;
  FontFeatureAliases swash_;
  FontFeatureAliases ornaments_;
  FontFeatureAliases annotation_;
};

template <>
struct DowncastTraits<StyleRuleFontFeatureValues> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsFontFeatureValuesRule();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_FONT_FEATURE_VALUES_H_
