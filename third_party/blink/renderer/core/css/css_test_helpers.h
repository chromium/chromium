// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TEST_HELPERS_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Document;
class CSSStyleSheet;
class CSSVariableData;
class CSSValue;
class CSSProperty;
class PropertyRegistration;

namespace css_test_helpers {

// Example usage:
//
// css_test_helpers::TestStyleSheet sheet;
// sheet.addCSSRule("body { color: red} #a { position: absolute }");
// RuleSet& ruleSet = sheet.ruleSet();
// ... examine RuleSet to find the rule and test properties on it.
class TestStyleSheet {
  STACK_ALLOCATED();

 public:
  TestStyleSheet();
  ~TestStyleSheet();

  const Document& GetDocument() { return *document_; }

  void AddCSSRules(const String& rule_text, bool is_empty_sheet = false);
  RuleSet& GetRuleSet();
  CSSRuleList* CssRules();

 private:
  Persistent<Document> document_;
  Persistent<CSSStyleSheet> style_sheet_;
};

// Create a PropertyRegistration for the given name. The syntax, initial value,
// and inherited status are all undefined.
PropertyRegistration* CreatePropertyRegistration(const String& name);

// Create a non-inherited PropertyRegistration with syntax <length>, and the
// given value in pixels as the initial value.
PropertyRegistration* CreateLengthRegistration(const String& name, int px);

void RegisterProperty(Document& document,
                      const String& name,
                      const String& syntax,
                      const base::Optional<String>& initial_value,
                      bool is_inherited);

scoped_refptr<CSSVariableData> CreateVariableData(String);
const CSSValue* CreateCustomIdent(AtomicString);
const CSSValue* ParseLonghand(Document& document,
                              const CSSProperty&,
                              const String& value);
const CSSPropertyValueSet* ParseDeclarationBlock(
    const String& block_text,
    CSSParserMode mode = kHTMLStandardMode);
StyleRuleBase* ParseRule(Document& document, String text);

// Parse a value according to syntax defined by:
// https://drafts.css-houdini.org/css-properties-values-api-1/#syntax-strings
const CSSValue* ParseValue(Document&, String syntax, String value);

}  // namespace css_test_helpers
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TEST_HELPERS_H_
