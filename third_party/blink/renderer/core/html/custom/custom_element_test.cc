// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element.h"

#include <ios>
#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

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

static void TestIsPotentialCustomElementNameChar(UChar32 c, bool expected) {
  LChar str8[] = "a-X";
  UChar str16[] = {'a', '-', 'X', '\0', '\0'};
  AtomicString str;
  if (c <= 0xFF) {
    str8[2] = c;
    str = str8;
  } else {
    size_t i = 2;
    U16_APPEND_UNSAFE(str16, i, c);
    str16[i] = 0;
    str = str16;
  }
  TestIsPotentialCustomElementName(str, expected);
}

TEST(CustomElementTest, TestIsValidNamePotentialCustomElementName) {
  struct {
    bool expected;
    AtomicString str;
  } tests[] = {
      {false, ""},
      {false, "a"},
      {false, "A"},

      {false, "A-"},
      {false, "0-"},

      {true, "a-"},
      {true, "a-a"},
      {true, "aa-"},
      {true, "aa-a"},
      {true, reinterpret_cast<const UChar*>(
                 u"aa-\x6F22\x5B57")},  // Two CJK Unified Ideographs
      {true, reinterpret_cast<const UChar*>(
                 u"aa-\xD840\xDC0B")},  // Surrogate pair U+2000B

      {false, "a-A"},
      {false, "a-Z"},
  };
  for (auto test : tests)
    TestIsPotentialCustomElementName(test.str, test.expected);
}

TEST(CustomElementTest, TestIsValidNamePotentialCustomElementNameChar) {
  struct {
    UChar32 from, to;
  } ranges[] = {
      // "-" | "." need to merge to test -1/+1.
      {'-', '.'},
      {'0', '9'},
      {'_', '_'},
      {'a', 'z'},
      {0xB7, 0xB7},
      {0xC0, 0xD6},
      {0xD8, 0xF6},
      // [#xF8-#x2FF] | [#x300-#x37D] need to merge to test -1/+1.
      {0xF8, 0x37D},
      {0x37F, 0x1FFF},
      {0x200C, 0x200D},
      {0x203F, 0x2040},
      {0x2070, 0x218F},
      {0x2C00, 0x2FEF},
      {0x3001, 0xD7FF},
      {0xF900, 0xFDCF},
      {0xFDF0, 0xFFFD},
      {0x10000, 0xEFFFF},
  };
  for (auto range : ranges) {
    TestIsPotentialCustomElementNameChar(range.from - 1, false);
    for (UChar32 c = range.from; c <= range.to; ++c)
      TestIsPotentialCustomElementNameChar(c, true);
    TestIsPotentialCustomElementNameChar(range.to + 1, false);
  }
}

TEST(CustomElementTest, TestIsValidNamePotentialCustomElementName8BitChar) {
  // isPotentialCustomElementName8BitChar must match
  // isPotentialCustomElementNameChar, so we just test it returns
  // the same result throughout its range.
  for (UChar ch = 0x0; ch <= 0xff; ++ch) {
    EXPECT_EQ(Character::IsPotentialCustomElementName8BitChar(ch),
              Character::IsPotentialCustomElementNameChar(ch))
        << "isPotentialCustomElementName8BitChar must agree with "
        << "isPotentialCustomElementNameChar: 0x" << std::hex << ch;
  }
}

TEST(CustomElementTest, TestIsValidNamePotentialCustomElementNameCharFalse) {
  struct {
    UChar32 from, to;
  } ranges[] = {
      {'A', 'Z'},
  };
  for (auto range : ranges) {
    for (UChar32 c = range.from; c <= range.to; ++c)
      TestIsPotentialCustomElementNameChar(c, false);
  }
}

TEST(CustomElementTest, TestIsValidNameHyphenContainingElementNames) {
  EXPECT_TRUE(CustomElement::IsValidName("valid-name"));

  EXPECT_FALSE(CustomElement::IsValidName("annotation-xml"));
  EXPECT_FALSE(CustomElement::IsValidName("color-profile"));
  EXPECT_FALSE(CustomElement::IsValidName("font-face"));
  EXPECT_FALSE(CustomElement::IsValidName("font-face-src"));
  EXPECT_FALSE(CustomElement::IsValidName("font-face-uri"));
  EXPECT_FALSE(CustomElement::IsValidName("font-face-format"));
  EXPECT_FALSE(CustomElement::IsValidName("font-face-name"));
  EXPECT_FALSE(CustomElement::IsValidName("missing-glyph"));
}

TEST(CustomElementTest, TestIsValidNameEmbedderNames) {
  CustomElement::AddEmbedderCustomElementName("embeddercustomelement");

  EXPECT_FALSE(CustomElement::IsValidName("embeddercustomelement", false));
  EXPECT_TRUE(CustomElement::IsValidName("embeddercustomelement", true));
}

TEST(CustomElementTest, StateByParser) {
  const char* body_content =
      "<div id=div></div>"
      "<a-a id=v1v0></a-a>"
      "<font-face id=v0></font-face>";
  auto page_holder = std::make_unique<DummyPageHolder>();
  Document& document = page_holder->GetDocument();
  document.body()->SetInnerHTMLFromString(String::FromUTF8(body_content));

  struct {
    const char* id;
    CustomElementState state;
    Element::V0CustomElementState v0state;
  } parser_data[] = {
      {"div", CustomElementState::kUncustomized, Element::kV0NotCustomElement},
      {"v1v0", CustomElementState::kUndefined, Element::kV0WaitingForUpgrade},
      {"v0", CustomElementState::kUncustomized, Element::kV0WaitingForUpgrade},
  };
  for (const auto& data : parser_data) {
    Element* element = document.getElementById(data.id);
    EXPECT_EQ(data.state, element->GetCustomElementState()) << data.id;
    EXPECT_EQ(data.v0state, element->GetV0CustomElementState()) << data.id;
  }
}

TEST(CustomElementTest, StateByCreateElement) {
  struct {
    const char* name;
    CustomElementState state;
    Element::V0CustomElementState v0state;
  } create_element_data[] = {
      {"div", CustomElementState::kUncustomized, Element::kV0NotCustomElement},
      {"a-a", CustomElementState::kUndefined, Element::kV0WaitingForUpgrade},
      // TODO(pdr): <font-face> should be V0NotCustomElement as per the spec,
      // but was regressed to be V0WaitingForUpgrade in
      // http://crrev.com/656913006
      {"font-face", CustomElementState::kUncustomized,
       Element::kV0WaitingForUpgrade},
      {"_-X", CustomElementState::kUncustomized, Element::kV0WaitingForUpgrade},
  };
  auto page_holder = std::make_unique<DummyPageHolder>();
  Document& document = page_holder->GetDocument();
  for (const auto& data : create_element_data) {
    Element* element = document.CreateElementForBinding(data.name);
    EXPECT_EQ(data.state, element->GetCustomElementState()) << data.name;
    EXPECT_EQ(data.v0state, element->GetV0CustomElementState()) << data.name;

    element = document.createElementNS(html_names::xhtmlNamespaceURI, data.name,
                                       ASSERT_NO_EXCEPTION);
    EXPECT_EQ(data.state, element->GetCustomElementState()) << data.name;
    EXPECT_EQ(data.v0state, element->GetV0CustomElementState()) << data.name;

    element = document.createElementNS(svg_names::kNamespaceURI, data.name,
                                       ASSERT_NO_EXCEPTION);
    EXPECT_EQ(CustomElementState::kUncustomized,
              element->GetCustomElementState())
        << data.name;
    EXPECT_EQ(data.v0state, element->GetV0CustomElementState()) << data.name;
  }
}

TEST(CustomElementTest,
     CreateElement_TagNameCaseHandlingCreatingCustomElement) {
  // register a definition
  auto holder(std::make_unique<DummyPageHolder>());
  ScriptState* script_state = ToScriptStateForMainWorld(&holder->GetFrame());
  CustomElementRegistry* registry =
      holder->GetFrame().DomWindow()->customElements();
  NonThrowableExceptionState should_not_throw;
  {
    CEReactionsScope reactions;
    TestCustomElementDefinitionBuilder builder;
    registry->DefineInternal(script_state, "a-a", builder,
                             ElementDefinitionOptions::Create(),
                             should_not_throw);
  }
  CustomElementDefinition* definition =
      registry->DefinitionFor(CustomElementDescriptor("a-a", "a-a"));
  EXPECT_NE(nullptr, definition) << "a-a should be registered";

  // create an element with an uppercase tag name
  Document& document = holder->GetDocument();
  EXPECT_TRUE(document.IsHTMLDocument())
      << "this test requires a HTML document";
  Element* element = document.CreateElementForBinding("A-A", should_not_throw);
  EXPECT_EQ(definition, element->GetCustomElementDefinition());
}

}  // namespace blink
