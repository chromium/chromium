// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/container_node.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/platform/bindings/script_regexp.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using ContainerNodeTest = EditingTestBase;

TEST_F(ContainerNodeTest, CannotFindTextNodesWithoutText) {
  SetBodyContent(R"HTML(<body><span id="id"></span></body>)HTML");

  StaticNodeList* nodes = GetDocument().FindAllTextNodesMatchingRegex("(.)*");

  EXPECT_EQ(nodes->length(), 0U);
}

TEST_F(ContainerNodeTest, CanFindTextNodesWithBreakInBetween) {
  SetBodyContent(R"HTML(<body><span id="id"> Hello
      <span></span> world! </span></body>)HTML");

  StaticNodeList* nodes =
      GetDocument().FindAllTextNodesMatchingRegex("(.|\n)*");

  EXPECT_EQ(nodes->length(), 2U);
  EXPECT_EQ(nodes->item(0),
            GetDocument().getElementById(AtomicString("id"))->firstChild());
  EXPECT_EQ(nodes->item(1),
            GetDocument().getElementById(AtomicString("id"))->lastChild());
}

TEST_F(ContainerNodeTest, CanFindTextNodesWithCommentInBetween) {
  SetBodyContent(R"HTML(<body><span id="id"> Hello
      <!-- comment --> world! </span></body>)HTML");

  StaticNodeList* nodes =
      GetDocument().FindAllTextNodesMatchingRegex("(.|\n)*");

  EXPECT_EQ(nodes->length(), 2U);
  EXPECT_EQ(nodes->item(0),
            GetDocument().getElementById(AtomicString("id"))->firstChild());
  EXPECT_EQ(nodes->item(1),
            GetDocument().getElementById(AtomicString("id"))->lastChild());
}

TEST_F(ContainerNodeTest, CanFindTextNodeWithOnlyText) {
  SetBodyContent(
      R"HTML(<body><span id="id"> Find me please </span></body>)HTML");

  StaticNodeList* nodes = GetDocument().FindAllTextNodesMatchingRegex("(.)*");

  ASSERT_EQ(nodes->length(), 1U);
  EXPECT_EQ(nodes->item(0),
            GetDocument().getElementById(AtomicString("id"))->firstChild());
}

TEST_F(ContainerNodeTest, CannotFindTextNodesIfTheMatcherRejectsIt) {
  SetBodyContent(
      R"HTML(<body><span id="id"> Don't find me please </span></body>)HTML");

  StaticNodeList* nodes =
      GetDocument().FindAllTextNodesMatchingRegex("not present");

  EXPECT_EQ(nodes->length(), 0U);
}

TEST_F(ContainerNodeTest, CanFindAllTextNodes) {
  SetBodyContent(R"HTML(
      <body>
        <div id="id">
          <div>
            Text number 1
          </div>
          <div>
            Text number 2
            <div> Text number 3</div>
            Text number 4
          </div>
          <div>
            Text number 5
          </div>
        </div>
        <div>
          Text number 6
        </div>
      </body>
    )HTML");

  StaticNodeList* nodes = GetDocument().FindAllTextNodesMatchingRegex(
      "(.|\n)*(Text number)(.|\n)*");

  ASSERT_EQ(nodes->length(), 6U);
  EXPECT_EQ(To<Text>(nodes->item(0))->data(),
            String("\n            Text number 1\n          "));
  EXPECT_EQ(To<Text>(nodes->item(1))->data(),
            String("\n            Text number 2\n            "));
  EXPECT_EQ(To<Text>(nodes->item(2))->data(), String(" Text number 3"));
  EXPECT_EQ(To<Text>(nodes->item(3))->data(),
            String("\n            Text number 4\n          "));
  EXPECT_EQ(To<Text>(nodes->item(4))->data(),
            String("\n            Text number 5\n          "));
  EXPECT_EQ(To<Text>(nodes->item(5))->data(),
            String("\n          Text number 6\n        "));
}

TEST_F(ContainerNodeTest, CanFindOnlyTextNodesThatMatch) {
  SetBodyContent(R"HTML(
      <body>
        <div id="id">
          <div>
            Text number 1
          </div>
          <div>
            Text number 2
            <div id="id_1"> Text number 3 </div>
            <div id="id_2"> Text number 4 </div>
            Text number 5
          </div>
          <div>
            Text number 6
          </div>
        </div>
        <div>
          Text number
        </div>
      </body>
    )HTML");

  StaticNodeList* nodes = GetDocument().FindAllTextNodesMatchingRegex(
      "(\\s|\n)*(Text number 3|Text number 4)(\\s|\n)*");
  ASSERT_EQ(nodes->length(), 2U);
  EXPECT_EQ(nodes->item(0),
            GetDocument().getElementById(AtomicString("id_1"))->firstChild());
  EXPECT_EQ(nodes->item(1),
            GetDocument().getElementById(AtomicString("id_2"))->firstChild());
}

}  // namespace blink
