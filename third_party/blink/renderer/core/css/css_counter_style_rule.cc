// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_counter_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/dom/document.h"
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

void CSSCounterStyleRule::SetterInternal(
    const ExecutionContext* execution_context,
    AtRuleDescriptorID descriptor_id,
    const String& text) {
  CSSStyleSheet* style_sheet = parentStyleSheet();
  auto& context = *MakeGarbageCollected<CSSParserContext>(
      ParserContext(execution_context->GetSecureContextMode()), style_sheet);
  CSSTokenizer tokenizer(text);
  auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange token_range(tokens);
  CSSValue* new_value = AtRuleDescriptorParser::ParseAtCounterStyleDescriptor(
      descriptor_id, token_range, context);
  if (!new_value)
    return;

  switch (descriptor_id) {
    case AtRuleDescriptorID::System:
      if (!counter_style_rule_->SetSystem(new_value))
        return;
      break;
    case AtRuleDescriptorID::Negative:
      if (!counter_style_rule_->SetNegative(new_value))
        return;
      break;
    case AtRuleDescriptorID::Prefix:
      if (!counter_style_rule_->SetPrefix(new_value))
        return;
      break;
    case AtRuleDescriptorID::Suffix:
      if (!counter_style_rule_->SetSuffix(new_value))
        return;
      break;
    case AtRuleDescriptorID::Range:
      if (!counter_style_rule_->SetRange(new_value))
        return;
      break;
    case AtRuleDescriptorID::Pad:
      if (!counter_style_rule_->SetPad(new_value))
        return;
      break;
    case AtRuleDescriptorID::Fallback:
      if (!counter_style_rule_->SetFallback(new_value))
        return;
      break;
    case AtRuleDescriptorID::Symbols:
      if (!counter_style_rule_->SetSymbols(new_value))
        return;
      break;
    case AtRuleDescriptorID::AdditiveSymbols:
      if (!counter_style_rule_->SetAdditiveSymbols(new_value))
        return;
      break;
    case AtRuleDescriptorID::SpeakAs:
      if (!counter_style_rule_->SetSpeakAs(new_value))
        return;
      break;
    default:
      NOTREACHED();
      return;
  }

  if (Document* document = style_sheet->OwnerDocument())
    document->GetStyleEngine().MarkCounterStylesNeedUpdate();
}

void CSSCounterStyleRule::setName(const String&) {
  // TODO(crbug.com/687225): Implement
}

void CSSCounterStyleRule::setSystem(const ExecutionContext* execution_context,
                                    const String& text) {
  SetterInternal(execution_context, AtRuleDescriptorID::System, text);
}

void CSSCounterStyleRule::setSymbols(const ExecutionContext* execution_context,
                                     const String& text) {
  SetterInternal(execution_context, AtRuleDescriptorID::Symbols, text);
}

void CSSCounterStyleRule::setAdditiveSymbols(
    const ExecutionContext* execution_context,
    const String& text) {
  SetterInternal(execution_context, AtRuleDescriptorID::AdditiveSymbols, text);
}

void CSSCounterStyleRule::setNegative(const ExecutionContext* execution_context,
                                      const String& text) {
  SetterInternal(execution_context, AtRuleDescriptorID::Negative, text);
}

void CSSCounterStyleRule::setPrefix(const ExecutionContext* execution_context,
                                    const String& text) {
  SetterInternal(execution_context, AtRuleDescriptorID::Prefix, text);
}

void CSSCounterStyleRule::setSuffix(const ExecutionContext* execution_context,
                                    const String& text) {
  SetterInternal(execution_context, AtRuleDescriptorID::Suffix, text);
}

void CSSCounterStyleRule::setRange(const ExecutionContext* execution_context,
                                   const String& text) {
  SetterInternal(execution_context, AtRuleDescriptorID::Range, text);
}

void CSSCounterStyleRule::setPad(const ExecutionContext* execution_context,
                                 const String& text) {
  SetterInternal(execution_context, AtRuleDescriptorID::Pad, text);
}

void CSSCounterStyleRule::setSpeakAs(const ExecutionContext* execution_context,
                                     const String& text) {
  SetterInternal(execution_context, AtRuleDescriptorID::SpeakAs, text);
}

void CSSCounterStyleRule::setFallback(const ExecutionContext* execution_context,
                                      const String& text) {
  SetterInternal(execution_context, AtRuleDescriptorID::Fallback, text);
}

void CSSCounterStyleRule::Trace(Visitor* visitor) const {
  visitor->Trace(counter_style_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
