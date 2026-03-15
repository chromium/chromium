// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>

#include "base/containers/span.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class HTMLTreeBuilderAdoptionAgencyTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().SetURL(KURL("https://example.test"));
  }

  // Parse HTML string in the document body
  void ParseHTMLString(const char* html_string) {
    auto& document = To<HTMLDocument>(GetDocument());
    Element* body = document.body();
    ASSERT_TRUE(body) << "Document should have a body element";

    DummyExceptionStateForTesting exception_state;
    body->SetInnerHTMLWithoutTrustedTypes(html_string, exception_state);
    ASSERT_FALSE(exception_state.HadException())
        << "Exception during innerHTML parsing";
  }

  // Debug helper to build the DOM tree structure as a string
  static void BuildDOMTree(Element* element, int depth, std::string& output) {
    if (!element) {
      return;
    }
    std::string indent(depth * 2, ' ');
    base::StrAppend(&output, {indent, element->tagName().Utf8(), "\n"});
    for (Node* child = element->firstChild(); child;
         child = child->nextSibling()) {
      if (auto* child_element = DynamicTo<Element>(child)) {
        BuildDOMTree(child_element, depth + 1, output);
      } else if (child->getNodeType() == Node::kTextNode) {
        std::string text_indent((depth + 1) * 2, ' ');
        base::StrAppend(&text_indent, {"#text: ", child->nodeValue().Utf8()});
        base::StrAppend(&output, {text_indent, "\n"});
      }
    }
  }

  std::string DumpDOMTree() {
    std::string result;
    BuildDOMTree(GetDocument().body(), 0, result);
    return result;
  }
};

// Test case 1: Complex adoption agency case (webkit02)
// Input: <b><em><foo><foo><foo><aside></b>
// Bug: <em> is being reconstructed inside <aside> when it shouldn't be
TEST_F(HTMLTreeBuilderAdoptionAgencyTest, AdoptionAgency_B_Em_Foo_Aside) {
  ParseHTMLString("<b><em><foo><foo><foo><aside></b></em>");
  EXPECT_EQ(DumpDOMTree(),
            "BODY\n"
            "  B\n"
            "    EM\n"
            "      FOO\n"
            "        FOO\n"
            "          FOO\n"
            "  ASIDE\n"
            "    B\n");
}

// Test case 2: Complex adoption agency case
// Input: <b><i><s><u><em><div></b><p>X</p>
// Bug: The text node with "X" is ignored if we don't regenerate correctly the
// bookmark after removing elements from the formatting element list.
TEST_F(HTMLTreeBuilderAdoptionAgencyTest, AdoptionAgency_textNodeLost) {
  ParseHTMLString("<b><i><s><u><em><div></b><p>X</p>");
  EXPECT_EQ(DumpDOMTree(),
            "BODY\n"
            "  B\n"
            "    I\n"
            "      S\n"
            "        U\n"
            "          EM\n"
            "  S\n"
            "    U\n"
            "      EM\n"
            "        DIV\n"
            "          B\n"
            "          P\n"
            "            #text: X\n");
}

}  // namespace blink
