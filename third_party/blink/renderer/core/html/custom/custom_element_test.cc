// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/html/custom/custom_element.h"

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

static void TestIsPotentialCustomElementNameChar(UChar32 c, bool expected) {
  LChar str8[] = "a-X";
  UChar str16[] = {'a', '-', 'X', '\0', '\0'};
  AtomicString str;
  if (c <= 0xFF) {
    str8[2] = c;
    str = AtomicString(str8);
  } else {
    size_t i = 2;
    U16_APPEND_UNSAFE(str16, i, c);
    str16[i] = 0;
    str = AtomicString(str16);
  }
  TestIsPotentialCustomElementName(str, expected);
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

TEST(CustomElementTest, TestIsValidNamePotentialCustomElementNameChar) {
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
  // isPotentialCustomElementName8BitChar must match
  // isPotentialCustomElementNameChar, so we just test it returns
  // the same result throughout its range.
  for (UChar ch = 0x0; ch <= 0xff; ++ch) {
    EXPECT_EQ(Character::IsPotentialCustomElementName8BitChar(ch),
              Character::IsPotentialCustomElementNameChar(ch))
        << "isPotentialCustomElementName8BitChar must agree with "
        << "isPotentialCustomElementNameChar: 0x" << std::hex
        << static_cast<uint16_t>(ch);
  }
}

TEST(CustomElementTest, TestIsValidNamePotentialCustomElementNameCharFalse) {
  test::TaskEnvironment task_environment;
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
  document.body()->setInnerHTML(String::FromUTF8(body_content));

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
    CEReactionsScope reactions;
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
