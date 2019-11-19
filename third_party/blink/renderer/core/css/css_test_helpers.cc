// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_test_helpers.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/property_definition.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {
namespace css_test_helpers {

TestStyleSheet::~TestStyleSheet() = default;

TestStyleSheet::TestStyleSheet() {
  document_ = MakeGarbageCollected<Document>();
  TextPosition position;
  style_sheet_ = CSSStyleSheet::CreateInline(*document_, NullURL(), position,
                                             UTF8Encoding());
}

CSSRuleList* TestStyleSheet::CssRules() {
  DummyExceptionStateForTesting exception_state;
  CSSRuleList* result = style_sheet_->cssRules(exception_state);
  EXPECT_FALSE(exception_state.HadException());
  return result;
}

RuleSet& TestStyleSheet::GetRuleSet() {
  RuleSet& rule_set = style_sheet_->Contents()->EnsureRuleSet(
      MediaQueryEvaluator(), kRuleHasNoSpecialState);
  rule_set.CompactRulesIfNeeded();
  return rule_set;
}

void TestStyleSheet::AddCSSRules(const String& css_text, bool is_empty_sheet) {
  TextPosition position;
  unsigned sheet_length = style_sheet_->length();
  style_sheet_->Contents()->ParseStringAtPosition(css_text, position);
  if (!is_empty_sheet)
    ASSERT_GT(style_sheet_->length(), sheet_length);
  else
    ASSERT_EQ(style_sheet_->length(), sheet_length);
}

void RegisterProperty(Document& document,
                      const String& name,
                      const String& syntax,
                      const String& initial_value,
                      bool is_inherited) {
  DummyExceptionStateForTesting exception_state;
  PropertyDefinition* property_definition = PropertyDefinition::Create();
  property_definition->setName(name);
  property_definition->setSyntax(syntax);
  property_definition->setInitialValue(initial_value);
  property_definition->setInherits(is_inherited);
  PropertyRegistration::registerProperty(&document, property_definition,
                                         exception_state);
  ASSERT_FALSE(exception_state.HadException());
}

scoped_refptr<CSSVariableData> CreateVariableData(String s) {
  auto tokens = CSSTokenizer(s).TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  bool is_animation_tainted = false;
  bool needs_variable_resolution = false;
  return CSSVariableData::Create(range, is_animation_tainted,
                                 needs_variable_resolution, KURL(),
                                 WTF::TextEncoding());
}

const CSSValue* CreateCustomIdent(AtomicString s) {
  return MakeGarbageCollected<CSSCustomIdentValue>(s);
}

const CSSValue* ParseLonghand(Document& document,
                              const CSSProperty& property,
                              const String& value) {
  const auto* longhand = DynamicTo<Longhand>(property);
  if (!longhand)
    return nullptr;

  const auto* context = MakeGarbageCollected<CSSParserContext>(document);
  CSSParserLocalContext local_context;
  auto tokens = CSSTokenizer(value).TokenizeToEOF();
  CSSParserTokenRange range(tokens);

  return longhand->ParseSingleValue(range, *context, local_context);
}

}  // namespace css_test_helpers
}  // namespace blink
