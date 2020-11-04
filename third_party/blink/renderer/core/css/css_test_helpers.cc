// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_test_helpers.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_property_definition.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
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
  document_ = Document::CreateForTest();
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
      MediaQueryEvaluator(document_->GetFrame()), kRuleHasNoSpecialState);
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

PropertyRegistration* CreatePropertyRegistration(const String& name) {
  auto syntax_definition = CSSSyntaxStringParser("*").Parse();
  DCHECK(syntax_definition);
  return MakeGarbageCollected<PropertyRegistration>(
      AtomicString(name), *syntax_definition, false /* inherits */,
      nullptr /* initial */, nullptr /* initial_variable_data */);
}

PropertyRegistration* CreateLengthRegistration(const String& name, int px) {
  auto syntax_definition = CSSSyntaxStringParser("<length>").Parse();
  DCHECK(syntax_definition);
  const CSSValue* initial =
      CSSNumericLiteralValue::Create(px, CSSPrimitiveValue::UnitType::kPixels);
  return MakeGarbageCollected<PropertyRegistration>(
      AtomicString(name), *syntax_definition, false /* inherits */, initial,
      CreateVariableData(initial->CssText()));
}

void RegisterProperty(Document& document,
                      const String& name,
                      const String& syntax,
                      const base::Optional<String>& initial_value,
                      bool is_inherited) {
  DCHECK(!initial_value || !initial_value.value().IsNull());
  DummyExceptionStateForTesting exception_state;
  PropertyDefinition* property_definition = PropertyDefinition::Create();
  property_definition->setName(name);
  property_definition->setSyntax(syntax);
  property_definition->setInherits(is_inherited);
  if (initial_value)
    property_definition->setInitialValue(initial_value.value());
  PropertyRegistration::registerProperty(document.GetExecutionContext(),
                                         property_definition, exception_state);
  ASSERT_FALSE(exception_state.HadException());
}

scoped_refptr<CSSVariableData> CreateVariableData(String s) {
  CSSTokenizer tokenizer(s);
  auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  bool is_animation_tainted = false;
  bool needs_variable_resolution = false;
  return CSSVariableData::Create({range, StringView(s)}, is_animation_tainted,
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

const CSSPropertyValueSet* ParseDeclarationBlock(const String& block_text,
                                                 CSSParserMode mode) {
  auto* set = MakeGarbageCollected<MutableCSSPropertyValueSet>(mode);
  set->ParseDeclarationList(block_text, SecureContextMode::kSecureContext,
                            nullptr);
  return set;
}

StyleRuleBase* ParseRule(Document& document, String text) {
  TextPosition position;
  auto* sheet = CSSStyleSheet::CreateInline(document, NullURL(), position,
                                            UTF8Encoding());
  const auto* context = MakeGarbageCollected<CSSParserContext>(document);
  return CSSParser::ParseRule(context, sheet->Contents(), text);
}

const CSSValue* ParseValue(Document& document, String syntax, String value) {
  auto syntax_definition = CSSSyntaxStringParser(syntax).Parse();
  if (!syntax_definition.has_value())
    return nullptr;
  const auto* context = MakeGarbageCollected<CSSParserContext>(document);
  CSSTokenizer tokenizer(value);
  auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  return syntax_definition->Parse(range, *context,
                                  /* is_animation_tainted */ false);
}

CSSSelectorList ParseSelectorList(const String& string) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  CSSTokenizer tokenizer(string);
  const auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  return CSSSelectorParser::ParseSelector(range, context, sheet);
}

}  // namespace css_test_helpers
}  // namespace blink
