// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_font_palette_values_rule.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule_css_style_declaration.h"
#include "third_party/blink/renderer/core/css/style_rule_font_palette_values.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSFontPaletteValuesRule::CSSFontPaletteValuesRule(
    StyleRuleFontPaletteValues* font_palette_values_rule,
    CSSStyleSheet* sheet)
    : CSSRule(sheet), font_palette_values_rule_(font_palette_values_rule) {}

CSSFontPaletteValuesRule::~CSSFontPaletteValuesRule() = default;

String CSSFontPaletteValuesRule::cssText() const {
  StringBuilder result;
  result.Append("@font-palette-values ");
  SerializeIdentifier(name(), result);
  result.Append(" {");

  AppendDescriptorIfNotEmpty(result, "font-family", fontFamily());
  AppendDescriptorIfNotEmpty(result, "base-palette", basePalette());
  AppendDescriptorIfNotEmpty(result, "override-colors", overrideColors());

  result.Append(" }");
  return result.ReleaseString();
}

void CSSFontPaletteValuesRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  font_palette_values_rule_ = To<StyleRuleFontPaletteValues>(rule);
}

String CSSFontPaletteValuesRule::name() const {
  return font_palette_values_rule_->GetName();
}

String CSSFontPaletteValuesRule::fontFamily() const {
  return CSSValue::CssTextOrEmptyString(
      font_palette_values_rule_->GetFontFamily());
}

String CSSFontPaletteValuesRule::basePalette() const {
  return CSSValue::CssTextOrEmptyString(
      font_palette_values_rule_->GetBasePalette());
}

String CSSFontPaletteValuesRule::overrideColors() const {
  return CSSValue::CssTextOrEmptyString(
      font_palette_values_rule_->GetOverrideColors());
}

StyleRuleFontPaletteValues* CSSFontPaletteValuesRule::FontPaletteValues()
    const {
  return font_palette_values_rule_.Get();
}

CSSStyleDeclaration* CSSFontPaletteValuesRule::Style() {
  if (!font_palette_values_cssom_wrapper_) {
    font_palette_values_cssom_wrapper_ =
        MakeGarbageCollected<StyleRuleCSSStyleDeclaration>(
            font_palette_values_rule_->MutableProperties(), this);
  }
  return font_palette_values_cssom_wrapper_.Get();
}

void CSSFontPaletteValuesRule::Trace(Visitor* visitor) const {
  visitor->Trace(font_palette_values_rule_);
  visitor->Trace(font_palette_values_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
