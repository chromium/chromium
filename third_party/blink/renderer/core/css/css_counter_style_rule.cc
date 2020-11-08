// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_counter_style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

CSSCounterStyleRule::CSSCounterStyleRule(
    StyleRuleCounterStyle* counter_style_rule,
    CSSStyleSheet* sheet)
    : CSSRule(sheet), counter_style_rule_(counter_style_rule) {}

CSSCounterStyleRule::~CSSCounterStyleRule() = default;

String CSSCounterStyleRule::cssText() const {
  return String();
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
