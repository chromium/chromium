// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_counter_style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSCounterStyleRule::CSSCounterStyleRule(
    StyleRuleCounterStyle* counter_style_rule,
    CSSStyleSheet* sheet)
    : CSSRule(sheet), counter_style_rule_(counter_style_rule) {}

CSSCounterStyleRule::~CSSCounterStyleRule() = default;

String CSSCounterStyleRule::cssText() const {
  StringBuilder result;
  result.Append("@counter-style ");
  result.Append(name());
  result.Append(" {");

  // Note: The exact serialization isn't well specified.
  String system_text = system();
  if (system_text.length()) {
    result.Append(" system: ");
    result.Append(system_text);
    result.Append(";");
  }

  String symbols_text = symbols();
  if (symbols_text.length()) {
    result.Append(" symbols: ");
    result.Append(symbols_text);
    result.Append(";");
  }

  String additive_symbols_text = additiveSymbols();
  if (additive_symbols_text.length()) {
    result.Append(" additive-symbols: ");
    result.Append(additive_symbols_text);
    result.Append(";");
  }

  String negative_text = negative();
  if (negative_text.length()) {
    result.Append(" negative: ");
    result.Append(negative_text);
    result.Append(";");
  }

  String prefix_text = prefix();
  if (prefix_text.length()) {
    result.Append(" prefix: ");
    result.Append(prefix_text);
    result.Append(";");
  }

  String suffix_text = suffix();
  if (suffix_text.length()) {
    result.Append(" suffix: ");
    result.Append(suffix_text);
    result.Append(";");
  }

  String pad_text = pad();
  if (pad_text.length()) {
    result.Append(" pad: ");
    result.Append(pad_text);
    result.Append(";");
  }

  String range_text = range();
  if (range_text.length()) {
    result.Append(" range: ");
    result.Append(range_text);
    result.Append(";");
  }

  String fallback_text = fallback();
  if (fallback_text.length()) {
    result.Append(" fallback: ");
    result.Append(fallback_text);
    result.Append(";");
  }

  String speak_as_text = speakAs();
  if (speak_as_text.length()) {
    result.Append(" speak-as: ");
    result.Append(speak_as_text);
    result.Append(";");
  }

  result.Append(" }");
  return result.ToString();
}

void CSSCounterStyleRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  counter_style_rule_ = To<StyleRuleCounterStyle>(rule);
}

String CSSCounterStyleRule::name() const {
  return counter_style_rule_->GetName();
}

String CSSCounterStyleRule::system() const {
  if (const CSSValue* value = counter_style_rule_->GetSystem())
    return value->CssText();
  return String();
}

String CSSCounterStyleRule::symbols() const {
  if (const CSSValue* value = counter_style_rule_->GetSymbols())
    return value->CssText();
  return String();
}

String CSSCounterStyleRule::additiveSymbols() const {
  if (const CSSValue* value = counter_style_rule_->GetAdditiveSymbols())
    return value->CssText();
  return String();
}

String CSSCounterStyleRule::negative() const {
  if (const CSSValue* value = counter_style_rule_->GetNegative())
    return value->CssText();
  return String();
}

String CSSCounterStyleRule::prefix() const {
  if (const CSSValue* value = counter_style_rule_->GetPrefix())
    return value->CssText();
  return String();
}

String CSSCounterStyleRule::suffix() const {
  if (const CSSValue* value = counter_style_rule_->GetSuffix())
    return value->CssText();
  return String();
}

String CSSCounterStyleRule::range() const {
  if (const CSSValue* value = counter_style_rule_->GetRange())
    return value->CssText();
  return String();
}

String CSSCounterStyleRule::pad() const {
  if (const CSSValue* value = counter_style_rule_->GetPad())
    return value->CssText();
  return String();
}

String CSSCounterStyleRule::speakAs() const {
  if (const CSSValue* value = counter_style_rule_->GetSpeakAs())
    return value->CssText();
  return String();
}

String CSSCounterStyleRule::fallback() const {
  if (const CSSValue* value = counter_style_rule_->GetFallback())
    return value->CssText();
  return String();
}

void CSSCounterStyleRule::setName(const String&) {
  // TODO(crbug.com/687225): Implement
}

void CSSCounterStyleRule::setSystem(const String&) {
  // TODO(crbug.com/687225): Implement
}

void CSSCounterStyleRule::setSymbols(const String&) {
  // TODO(crbug.com/687225): Implement
}

void CSSCounterStyleRule::setAdditiveSymbols(const String&) {
  // TODO(crbug.com/687225): Implement
}

void CSSCounterStyleRule::setNegative(const String&) {
  // TODO(crbug.com/687225): Implement
}

void CSSCounterStyleRule::setPrefix(const String&) {
  // TODO(crbug.com/687225): Implement
}

void CSSCounterStyleRule::setSuffix(const String&) {
  // TODO(crbug.com/687225): Implement
}

void CSSCounterStyleRule::setRange(const String&) {
  // TODO(crbug.com/687225): Implement
}

void CSSCounterStyleRule::setPad(const String&) {
  // TODO(crbug.com/687225): Implement
}

void CSSCounterStyleRule::setSpeakAs(const String&) {
  // TODO(crbug.com/687225): Implement
}

void CSSCounterStyleRule::setFallback(const String&) {
  // TODO(crbug.com/687225): Implement
}

void CSSCounterStyleRule::Trace(Visitor* visitor) const {
  visitor->Trace(counter_style_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
