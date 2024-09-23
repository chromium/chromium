// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/css_test_helpers.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_style_sheet_init.h"
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
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {
namespace css_test_helpers {

TestStyleSheet::~TestStyleSheet() = default;

TestStyleSheet::TestStyleSheet() {
  document_ = Document::CreateForTest(execution_context_.GetExecutionContext());
  style_sheet_ = CreateStyleSheet(*document_);
}

CSSRuleList* TestStyleSheet::CssRules() {
  DummyExceptionStateForTesting exception_state;
  CSSRuleList* result = style_sheet_->cssRules(exception_state);
  EXPECT_FALSE(exception_state.HadException());
  return result;
}

RuleSet& TestStyleSheet::GetRuleSet() {
  RuleSet& rule_set = style_sheet_->Contents()->EnsureRuleSet(
      MediaQueryEvaluator(document_->GetFrame()));
  rule_set.CompactRulesIfNeeded();
  return rule_set;
}

void TestStyleSheet::AddCSSRules(const String& css_text, bool is_empty_sheet) {
  unsigned sheet_length = style_sheet_->length();
  style_sheet_->Contents()->ParseString(css_text);
  if (!is_empty_sheet) {
    ASSERT_GT(style_sheet_->length(), sheet_length);
  } else {
    ASSERT_EQ(style_sheet_->length(), sheet_length);
  }
}

CSSStyleSheet* CreateStyleSheet(Document& document) {
  return CSSStyleSheet::CreateInline(
      document, NullURL(), TextPosition::MinimumPosition(), UTF8Encoding());
}

RuleSet* CreateRuleSet(Document& document, String text) {
  DummyExceptionStateForTesting exception_state;
  auto* init = CSSStyleSheetInit::Create();
  auto* media_query_evaluator =
      MakeGarbageCollected<MediaQueryEvaluator>(document.GetFrame());
  auto* sheet = CSSStyleSheet::Create(document, init, exception_state);
  sheet->replaceSync(text, exception_state);
  return &sheet->Contents()->EnsureRuleSet(*media_query_evaluator);
}

PropertyRegistration* CreatePropertyRegistration(const String& name,
                                                 String syntax,
                                                 const CSSValue* initial_value,
                                                 bool is_inherited) {
  auto syntax_definition = CSSSyntaxStringParser(syntax).Parse();
  DCHECK(syntax_definition);
  DCHECK(syntax_definition->IsUniversal() || initial_value);
  return MakeGarbageCollected<PropertyRegistration>(
      AtomicString(name), *syntax_definition, is_inherited, initial_value);
}

PropertyRegistration* CreateLengthRegistration(const String& name, int px) {
  const CSSValue* initial =
      CSSNumericLiteralValue::Create(px, CSSPrimitiveValue::UnitType::kPixels);
  return CreatePropertyRegistration(name, "<length>", initial,
                                    false /* is_inherited */);
}

void RegisterProperty(Document& document,
                      const String& name,
                      const String& syntax,
                      const std::optional<String>& initial_value,
                      bool is_inherited) {
  DummyExceptionStateForTesting exception_state;
  RegisterProperty(document, name, syntax, initial_value, is_inherited,
                   exception_state);
  ASSERT_FALSE(exception_state.HadException());
}

void RegisterProperty(Document& document,
                      const String& name,
                      const String& syntax,
                      const std::optional<String>& initial_value,
                      bool is_inherited,
                      ExceptionState& exception_state) {
  DCHECK(!initial_value || !initial_value.value().IsNull());
  PropertyDefinition* property_definition = PropertyDefinition::Create();
  property_definition->setName(name);
  property_definition->setSyntax(syntax);
  property_definition->setInherits(is_inherited);
  if (initial_value) {
    property_definition->setInitialValue(initial_value.value());
  }
  PropertyRegistration::registerProperty(document.GetExecutionContext(),
                                         property_definition, exception_state);
}

void DeclareProperty(Document& document,
                     const String& name,
                     const String& syntax,
                     const std::optional<String>& initial_value,
                     bool is_inherited) {
  StringBuilder builder;
  builder.Append("@property ");
  builder.Append(name);
  builder.Append(" { ");

  // syntax:
  builder.Append("syntax:\"");
  builder.Append(syntax);
  builder.Append("\";");

  // initial-value:
  if (initial_value.has_value()) {
    builder.Append("initial-value:");
    builder.Append(initial_value.value());
    builder.Append(";");
  }

  // inherits:
  builder.Append("inherits:");
  builder.Append(is_inherited ? "true" : "false");
  builder.Append(";");

  builder.Append(" }");

  auto* rule =
      DynamicTo<StyleRuleProperty>(ParseRule(document, builder.ToString()));
  if (!rule) {
    return;
  }
  auto* registration = PropertyRegistration::MaybeCreateForDeclaredProperty(
      document, AtomicString(name), *rule);
  if (!registration) {
    return;
  }
  document.EnsurePropertyRegistry().DeclareProperty(AtomicString(name),
                                                    *registration);
  document.GetStyleEngine().PropertyRegistryChanged();
}

CSSVariableData* CreateVariableData(String s) {
  bool is_animation_tainted = false;
  bool needs_variable_resolution = false;
  return CSSVariableData::Create(s, is_animation_tainted,
                                 needs_variable_resolution);
}

const CSSValue* CreateCustomIdent(const char* s) {
  return MakeGarbageCollected<CSSCustomIdentValue>(AtomicString(s));
}

const CSSValue* ParseLonghand(Document& document,
                              const CSSProperty& property,
                              const String& value) {
  const auto* longhand = DynamicTo<Longhand>(property);
  if (!longhand) {
    return nullptr;
  }

  const auto* context = MakeGarbageCollected<CSSParserContext>(document);
  CSSParserLocalContext local_context;

  CSSParserTokenStream stream(value);
  return longhand->ParseSingleValue(stream, *context, local_context);
}

const CSSPropertyValueSet* ParseDeclarationBlock(const String& block_text,
                                                 CSSParserMode mode) {
  auto* set = MakeGarbageCollected<MutableCSSPropertyValueSet>(mode);
  set->ParseDeclarationList(block_text, SecureContextMode::kSecureContext,
                            nullptr);
  return set;
}

StyleRuleBase* ParseRule(Document& document, String text) {
  auto* sheet = CSSStyleSheet::CreateInline(
      document, NullURL(), TextPosition::MinimumPosition(), UTF8Encoding());
  const auto* context = MakeGarbageCollected<CSSParserContext>(document);
  return CSSParser::ParseRule(context, sheet->Contents(), CSSNestingType::kNone,
                              /*parent_rule_for_nesting=*/nullptr, text);
}

const CSSValue* ParseValue(Document& document, String syntax, String value) {
  auto syntax_definition = CSSSyntaxStringParser(syntax).Parse();
  if (!syntax_definition.has_value()) {
    return nullptr;
  }
  const auto* context = MakeGarbageCollected<CSSParserContext>(document);
  return syntax_definition->Parse(value, *context,
                                  /* is_animation_tainted */ false);
}

CSSSelectorList* ParseSelectorList(const String& string) {
  return ParseSelectorList(string, CSSNestingType::kNone,
                           /*parent_rule_for_nesting=*/nullptr,
                           /*is_within_scope=*/false);
}

CSSSelectorList* ParseSelectorList(const String& string,
                                   CSSNestingType nesting_type,
                                   const StyleRule* parent_rule_for_nesting,
                                   bool is_within_scope) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  CSSParserTokenStream stream(string);
  HeapVector<CSSSelector> arena;
  base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
      stream, context, nesting_type, parent_rule_for_nesting, is_within_scope,
      /* semicolon_aborts_nested_selector */ false, sheet, arena);
  return CSSSelectorList::AdoptSelectorVector(vector);
}

}  // namespace css_test_helpers
}  // namespace blink
