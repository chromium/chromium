// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_font_feature_values_rule.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSFontFeatureValuesRule::CSSFontFeatureValuesRule(
    StyleRuleFontFeatureValues* font_feature_values_rule,
    CSSStyleSheet* parent)
    : CSSRule(parent), font_feature_values_rule_(font_feature_values_rule) {}

CSSFontFeatureValuesRule::~CSSFontFeatureValuesRule() = default;

void CSSFontFeatureValuesRule::setFontFamily(const String& font_family) {
  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  Vector<String> families;
  font_family.Split(",", families);

  Vector<AtomicString> filtered_families;

  for (auto family : families) {
    String stripped = family.StripWhiteSpace();
    if (!stripped.empty()) {
      filtered_families.push_back(AtomicString(stripped));
    }
  }

  font_feature_values_rule_->SetFamilies(std::move(filtered_families));
}

String CSSFontFeatureValuesRule::fontFamily() {
  return font_feature_values_rule_->FamilyAsString();
}

CSSFontFeatureValuesMap* CSSFontFeatureValuesRule::annotation() {
  return MakeGarbageCollected<CSSFontFeatureValuesMap>(
      this, font_feature_values_rule_,
      font_feature_values_rule_->GetAnnotation());
}
CSSFontFeatureValuesMap* CSSFontFeatureValuesRule::ornaments() {
  return MakeGarbageCollected<CSSFontFeatureValuesMap>(
      this, font_feature_values_rule_,
      font_feature_values_rule_->GetOrnaments());
}
CSSFontFeatureValuesMap* CSSFontFeatureValuesRule::stylistic() {
  return MakeGarbageCollected<CSSFontFeatureValuesMap>(
      this, font_feature_values_rule_,
      font_feature_values_rule_->GetStylistic());
}
CSSFontFeatureValuesMap* CSSFontFeatureValuesRule::swash() {
  return MakeGarbageCollected<CSSFontFeatureValuesMap>(
      this, font_feature_values_rule_, font_feature_values_rule_->GetSwash());
}
CSSFontFeatureValuesMap* CSSFontFeatureValuesRule::characterVariant() {
  return MakeGarbageCollected<CSSFontFeatureValuesMap>(
      this, font_feature_values_rule_,
      font_feature_values_rule_->GetCharacterVariant());
}
CSSFontFeatureValuesMap* CSSFontFeatureValuesRule::styleset() {
  return MakeGarbageCollected<CSSFontFeatureValuesMap>(
      this, font_feature_values_rule_,
      font_feature_values_rule_->GetStyleset());
}

String CSSFontFeatureValuesRule::cssText() const {
  StringBuilder result;
  result.Append("@font-feature-values ");
  DCHECK(font_feature_values_rule_);
  result.Append(font_feature_values_rule_->FamilyAsString());
  result.Append(" { ");
  auto append_category = [&result](String rule_name,
                                   FontFeatureAliases* aliases) {
    DCHECK(aliases);
    if (aliases->size()) {
      result.Append("@");
      result.Append(rule_name);
      result.Append(" { ");
      for (auto& alias : *aliases) {
        // In CSS parsing of @font-feature-values an alias is only
        // appended if numbers are specified. In CSSOM
        // (CSSFontFeatureValuesMap::set) an empty or type-incompatible
        // argument is coerced into a number 0 and appended.
        DCHECK_GT(alias.value.indices.size(), 0u);
        SerializeIdentifier(alias.key, result);
        result.Append(":");
        for (uint32_t value : alias.value.indices) {
          result.Append(' ');
          result.AppendNumber(value);
        }
        result.Append("; ");
      }
      result.Append("} ");
    }
  };
  append_category("annotation", font_feature_values_rule_->GetAnnotation());
  append_category("ornaments", font_feature_values_rule_->GetOrnaments());
  append_category("stylistic", font_feature_values_rule_->GetStylistic());
  append_category("swash", font_feature_values_rule_->GetSwash());
  append_category("character-variant",
                  font_feature_values_rule_->GetCharacterVariant());
  append_category("styleset", font_feature_values_rule_->GetStyleset());
  result.Append("}");
  return result.ToString();
}

void CSSFontFeatureValuesRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  font_feature_values_rule_ = To<StyleRuleFontFeatureValues>(rule);
}

void CSSFontFeatureValuesRule::Trace(blink::Visitor* visitor) const {
  visitor->Trace(font_feature_values_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
