// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element.h"

#include <array>
#include <ios>
#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element_definition_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

static void TestIsPotentialCustomElementName(const AtomicString& str,
                                             bool expected) {
  if (expected) {
    EXPECT_TRUE(CustomElement::IsValidName(str))
        << str << " should be a valid custom element name.";
  } else {
    EXPECT_FALSE(CustomElement::IsValidName(str))
        << str << " should NOT be a valid custom element name.";
  }
}

TEST(CustomElementTest, TestIsValidNamePotentialCustomElementName) {
  test::TaskEnvironment task_environment;
  struct {
    bool expected;
    AtomicString str;
  } tests[] = {
      {false, g_empty_atom},
      {false, AtomicString("a")},
      {false, AtomicString("A")},

      {false, AtomicString("A-")},
      {false, AtomicString("0-")},

      {true, AtomicString("a-")},
      {true, AtomicString("a-a")},
      {true, AtomicString("aa-")},
      {true, AtomicString("aa-a")},
      {true, AtomicString(reinterpret_cast<const UChar*>(
                 u"aa-\x6F22\x5B57"))},  // Two CJK Unified Ideographs
      {true, AtomicString(reinterpret_cast<const UChar*>(
                 u"aa-\xD840\xDC0B"))},  // Surrogate pair U+2000B

      {false, AtomicString("a-A")},
      {false, AtomicString("a-Z")},
  };
  for (auto test : tests)
    TestIsPotentialCustomElementName(test.str, test.expected);
}

TEST(CustomElementTest, TestIsValidNameHyphenContainingElementNames) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(CustomElement::IsValidName(AtomicString("valid-name")));

  EXPECT_FALSE(CustomElement::IsValidName(AtomicString("annotation-xml")));
  EXPECT_FALSE(CustomElement::IsValidName(AtomicString("color-profile")));
  EXPECT_FALSE(CustomElement::IsValidName(AtomicString("font-face")));
  EXPECT_FALSE(CustomElement::IsValidName(AtomicString("font-face-src")));
  EXPECT_FALSE(CustomElement::IsValidName(AtomicString("font-face-uri")));
  EXPECT_FALSE(CustomElement::IsValidName(AtomicString("font-face-format")));
  EXPECT_FALSE(CustomElement::IsValidName(AtomicString("font-face-name")));
  EXPECT_FALSE(CustomElement::IsValidName(AtomicString("missing-glyph")));
}

TEST(CustomElementTest, TestIsValidNameEmbedderNames) {
  test::TaskEnvironment task_environment;
  CustomElement::AddEmbedderCustomElementName(
      AtomicString("embeddercustomelement"));

  EXPECT_FALSE(
      CustomElement::IsValidName(AtomicString("embeddercustomelement"), false));
  EXPECT_TRUE(
      CustomElement::IsValidName(AtomicString("embeddercustomelement"), true));
}

TEST(CustomElementTest, StateByParser) {
  test::TaskEnvironment task_environment;
  const char* body_content =
      "<div id=div></div>"
      "<a-a id=v1v0></a-a>"
      "<font-face id=v0></font-face>";
  auto page_holder = std::make_unique<DummyPageHolder>();
  Document& document = page_holder->GetDocument();
  document.body()->SetInnerHTMLWithoutTrustedTypes(
      String::FromUTF8(body_content));

  struct {
    const char* id;
    CustomElementState state;
  } parser_data[] = {
      {"div", CustomElementState::kUncustomized},
      {"v1v0", CustomElementState::kUndefined},
      {"v0", CustomElementState::kUncustomized},
  };
  for (const auto& data : parser_data) {
    Element* element = document.getElementById(AtomicString(data.id));
    EXPECT_EQ(data.state, element->GetCustomElementState()) << data.id;
  }
}

TEST(CustomElementTest, StateByCreateElement) {
  test::TaskEnvironment task_environment;
  struct {
    const char* name;
    CustomElementState state;
  } create_element_data[] = {
      {"div", CustomElementState::kUncustomized},
      {"a-a", CustomElementState::kUndefined},
      {"font-face", CustomElementState::kUncustomized},
      {"_-X", CustomElementState::kUncustomized},
  };
  auto page_holder = std::make_unique<DummyPageHolder>();
  Document& document = page_holder->GetDocument();
  for (const auto& data : create_element_data) {
    Element* element =
        document.CreateElementForBinding(AtomicString(data.name));
    EXPECT_EQ(data.state, element->GetCustomElementState()) << data.name;

    element =
        document.createElementNS(html_names::xhtmlNamespaceURI,
                                 AtomicString(data.name), ASSERT_NO_EXCEPTION);
    EXPECT_EQ(data.state, element->GetCustomElementState()) << data.name;

    element = document.createElementNS(
        svg_names::kNamespaceURI, AtomicString(data.name), ASSERT_NO_EXCEPTION);
    EXPECT_EQ(CustomElementState::kUncustomized,
              element->GetCustomElementState())
        << data.name;
  }
}

TEST(CustomElementTest,
     CreateElement_TagNameCaseHandlingCreatingCustomElement) {
  test::TaskEnvironment task_environment;
  CustomElementTestingScope scope;
  // register a definition
  ScriptState* script_state = scope.GetScriptState();
  CustomElementRegistry* registry =
      scope.GetFrame().DomWindow()->customElements();
  NonThrowableExceptionState should_not_throw;
  {
    CEReactionsScope reactions(scope.GetIsolate());
    TestCustomElementDefinitionBuilder builder;
    registry->DefineInternal(script_state, AtomicString("a-a"), builder,
                             ElementDefinitionOptions::Create(),
                             should_not_throw);
  }
  CustomElementDefinition* definition = registry->DefinitionFor(
      CustomElementDescriptor(AtomicString("a-a"), AtomicString("a-a")));
  EXPECT_NE(nullptr, definition) << "a-a should be registered";

  // create an element with an uppercase tag name
  Document& document = scope.GetDocument();
  EXPECT_TRUE(IsA<HTMLDocument>(document))
      << "this test requires a HTML document";
  Element* element =
      document.CreateElementForBinding(AtomicString("A-A"), should_not_throw);
  EXPECT_EQ(definition, element->GetCustomElementDefinition());
}

}  // namespace blink
