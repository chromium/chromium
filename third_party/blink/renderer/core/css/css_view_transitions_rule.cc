// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_view_transitions_rule.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_view_transitions.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSViewTransitionsRule::CSSViewTransitionsRule(
    StyleRuleViewTransitions* initial_rule,
    CSSStyleSheet* parent)
    : CSSRule(parent), view_transitions_rule_(initial_rule) {}

String CSSViewTransitionsRule::cssText() const {
  StringBuilder result;

  result.Append("@view-transitions { ");

  String navigation_trigger = navigationTrigger();
  if (!navigation_trigger.empty()) {
    result.Append("navigation-trigger: ");
    result.Append(navigation_trigger);
    result.Append("; ");
  }

  result.Append("}");

  return result.ReleaseString();
}

String CSSViewTransitionsRule::navigationTrigger() const {
  if (const CSSValue* value = view_transitions_rule_->GetNavigationTrigger()) {
    return value->CssText();
  }

  return String();
}

void CSSViewTransitionsRule::setNavigationTrigger(
    const ExecutionContext* execution_context,
    const String& text) {
  CSSStyleSheet* style_sheet = parentStyleSheet();
  auto& context = *MakeGarbageCollected<CSSParserContext>(
      ParserContext(execution_context->GetSecureContextMode()), style_sheet);
  CSSTokenizer tokenizer(text);
  auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange token_range(tokens);
  AtRuleDescriptorID descriptor_id = AtRuleDescriptorID::NavigationTrigger;
  CSSValue* new_value =
      AtRuleDescriptorParser::ParseAtViewTransitionsDescriptor(
          descriptor_id, token_range, context);
  if (!new_value) {
    return;
  }

  const auto* id = DynamicTo<CSSIdentifierValue>(new_value);
  if (!id || (id->GetValueID() != CSSValueID::kCrossDocumentSameOrigin &&
              id->GetValueID() != CSSValueID::kNone)) {
    return;
  }

  view_transitions_rule_->SetNavigationTrigger(new_value);

  if (Document* document = style_sheet->OwnerDocument()) {
    document->GetStyleEngine().UpdateViewTransitionsOptIn();
  }
}

void CSSViewTransitionsRule::Reattach(StyleRuleBase* rule) {
  CHECK(rule);
  view_transitions_rule_ = To<StyleRuleViewTransitions>(rule);
}

void CSSViewTransitionsRule::Trace(Visitor* visitor) const {
  visitor->Trace(view_transitions_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
