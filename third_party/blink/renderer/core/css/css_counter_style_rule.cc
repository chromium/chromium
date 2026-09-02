// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_counter_style_rule.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/css/style_rule_css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
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
  SerializeIdentifier(name(), result);
  result.Append(" {");

  // Note: The exact serialization isn't well specified.
  AppendDescriptorIfNotEmpty(result, "system", system());
  AppendDescriptorIfNotEmpty(result, "symbols", symbols());
  AppendDescriptorIfNotEmpty(result, "additive-symbols", additiveSymbols());
  AppendDescriptorIfNotEmpty(result, "negative", negative());
  AppendDescriptorIfNotEmpty(result, "prefix", prefix());
  AppendDescriptorIfNotEmpty(result, "suffix", suffix());
  AppendDescriptorIfNotEmpty(result, "pad", pad());
  AppendDescriptorIfNotEmpty(result, "range", range());
  AppendDescriptorIfNotEmpty(result, "fallback", fallback());
  AppendDescriptorIfNotEmpty(result, "speak-as", speakAs());

  result.Append(" }");
  return result.ReleaseString();
}

void CSSCounterStyleRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  counter_style_rule_ = To<StyleRuleCounterStyle>(rule);
  if (counter_style_cssom_wrapper_) {
    counter_style_cssom_wrapper_->Reattach(counter_style_rule_->Properties());
  }
}

String CSSCounterStyleRule::name() const {
  return counter_style_rule_->GetName();
}

String CSSCounterStyleRule::system() const {
  return CSSValue::CssTextOrEmptyString(counter_style_rule_->GetSystem());
}

String CSSCounterStyleRule::symbols() const {
  return CSSValue::CssTextOrEmptyString(counter_style_rule_->GetSymbols());
}

String CSSCounterStyleRule::additiveSymbols() const {
  return CSSValue::CssTextOrEmptyString(
      counter_style_rule_->GetAdditiveSymbols());
}

String CSSCounterStyleRule::negative() const {
  return CSSValue::CssTextOrEmptyString(counter_style_rule_->GetNegative());
}

String CSSCounterStyleRule::prefix() const {
  return CSSValue::CssTextOrEmptyString(counter_style_rule_->GetPrefix());
}

String CSSCounterStyleRule::suffix() const {
  return CSSValue::CssTextOrEmptyString(counter_style_rule_->GetSuffix());
}

String CSSCounterStyleRule::range() const {
  return CSSValue::CssTextOrEmptyString(counter_style_rule_->GetRange());
}

String CSSCounterStyleRule::pad() const {
  return CSSValue::CssTextOrEmptyString(counter_style_rule_->GetPad());
}

String CSSCounterStyleRule::speakAs() const {
  return CSSValue::CssTextOrEmptyString(counter_style_rule_->GetSpeakAs());
}

String CSSCounterStyleRule::fallback() const {
  return CSSValue::CssTextOrEmptyString(counter_style_rule_->GetFallback());
}

void CSSCounterStyleRule::SetterInternal(
    const ExecutionContext* execution_context,
    AtRuleDescriptorID descriptor_id,
    const String& text) {
  CSSStyleSheet* style_sheet = parentStyleSheet();
  auto& context = *MakeGarbageCollected<CSSParserContext>(
      ParserContext(execution_context->GetSecureContextMode()), style_sheet);
  CSSParserTokenStream stream(text);
  CSSValue* new_value = AtRuleDescriptorParser::ParseAtCounterStyleDescriptor(
      descriptor_id, stream, context);
  if (!new_value ||
      !counter_style_rule_->NewValueInvalidOrEqual(descriptor_id, new_value)) {
    return;
  }

  // TODO(xiaochengh): RuleMutationScope causes all rules of the tree scope to
  // be re-collected and the entire CounterStyleMap rebuilt, while we only need
  // to dirty one CounterStyle. Try to improve.
  CSSStyleSheet::RuleMutationScope rule_mutation_scope(this);

  counter_style_rule_->SetDescriptorValue(descriptor_id, new_value);
  if (Document* document = style_sheet->OwnerDocument()) {
    document->GetStyleEngine().MarkCounterStylesNeedUpdate();
  }
}

void CSSCounterStyleRule::setName(const ExecutionContext* execution_context,
                                  const String& text) {
  CSSStyleSheet* style_sheet = parentStyleSheet();
  auto& context = *MakeGarbageCollected<CSSParserContext>(
      ParserContext(execution_context->GetSecureContextMode()), style_sheet);
  CSSParserTokenStream stream(text);
  AtomicString name =
      css_parsing_utils::ConsumeCounterStyleNameInPrelude(stream, context);
  if (!name || name == counter_style_rule_->GetName() || !stream.AtEnd()) {
    return;
  }

  // Changing name may affect cascade result, which requires re-collecting all
  // the rules and re-constructing the CounterStyleMap to handle.
  CSSStyleSheet::RuleMutationScope rule_mutation_scope(this);

  counter_style_rule_->SetName(name);
  if (Document* document = style_sheet->OwnerDocument()) {
    document->GetStyleEngine().MarkCounterStylesNeedUpdate();
  }
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

CSSStyleDeclaration* CSSCounterStyleRule::Style() {
  if (!counter_style_cssom_wrapper_) {
    counter_style_cssom_wrapper_ =
        MakeGarbageCollected<StyleRuleCSSStyleDeclaration>(
            counter_style_rule_->Properties(), this);
  }
  return counter_style_cssom_wrapper_;
}

CSSStyleDeclaration* CSSCounterStyleRule::MutableStyleForInspector() {
  // We cannot keep this wrapper around, because we need to request a new one
  // so that the inner style can invalidate layout.
  return MakeGarbageCollected<StyleRuleCSSStyleDeclaration>(
      counter_style_rule_->MutableStyleForInspector(), this);
}

void CSSCounterStyleRule::Trace(Visitor* visitor) const {
  visitor->Trace(counter_style_rule_);
  visitor->Trace(counter_style_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
