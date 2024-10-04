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

TEST_F(ContainerNodeTest, HasOnlyTextReturnsFalseForEmptySpan) {
  SetBodyContent(R"HTML(<body><span id="id"></span></body>)HTML");

  EXPECT_FALSE(GetDocument().getElementById(AtomicString("id"))->HasOnlyText());
}

TEST_F(ContainerNodeTest, HasOnlyTextReturnsFalseForNonTextChild) {
  SetBodyContent(R"HTML(
    <body><div id="id"><div>Nested</div></div></body>
  )HTML");

  EXPECT_FALSE(GetDocument().getElementById(AtomicString("id"))->HasOnlyText());
}

TEST_F(ContainerNodeTest, HasOnlyTextReturnsTrueForSomeText) {
  SetBodyContent(R"HTML(<body><p id="id"> Here is some text </p></body>)HTML");

  EXPECT_TRUE(GetDocument().getElementById(AtomicString("id"))->HasOnlyText());
}

TEST_F(ContainerNodeTest, HasOnlyTextIgnoresComments) {
  SetBodyContent(R"HTML(
    <body>
      <p id="id"> Here is some text
        <!-- This is a comment that should be ignored. -->
      </p>
    </body>
  )HTML");

  EXPECT_TRUE(GetDocument().getElementById(AtomicString("id"))->HasOnlyText());
}

TEST_F(ContainerNodeTest, CannotFindTextInElementWithoutDescendants) {
  SetBodyContent(R"HTML(<body><span id="id"></span></body>)HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString("anything"), [](const String&) { return true; });

  EXPECT_TRUE(text.empty());
}

TEST_F(ContainerNodeTest, CannotFindTextNodesWithoutText) {
  SetBodyContent(R"HTML(<body><span id="id"></span></body>)HTML");

  StaticNodeList* nodes = GetDocument().FindAllTextNodesMatchingRegex("(.)*");

  EXPECT_EQ(nodes->length(), 0U);
}

TEST_F(ContainerNodeTest, CannotFindTextInElementWithNonTextDescendants) {
  SetBodyContent(R"HTML(<body><span id="id"> Hello
      <span></span> world! </span></body>)HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString("Hello"), [](const String&) { return true; });

  EXPECT_TRUE(text.empty());
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

TEST_F(ContainerNodeTest, CannotFindTextInElementWithoutMatchingSubtring) {
  SetBodyContent(R"HTML(<body><span id="id"> Hello </span></body>)HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString("Goodbye"), [](const String&) { return true; });

  EXPECT_TRUE(text.empty());
}

TEST_F(ContainerNodeTest, CanFindTextInElementWithOnlyTextDescendants) {
  SetBodyContent(
      R"HTML(<body><span id="id"> Find me please </span></body>)HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString("me"), [](const String&) { return true; });

  EXPECT_EQ(String(" Find me please "), text);
}

TEST_F(ContainerNodeTest, CanFindTextNodeWithOnlyText) {
  SetBodyContent(
      R"HTML(<body><span id="id"> Find me please </span></body>)HTML");

  StaticNodeList* nodes = GetDocument().FindAllTextNodesMatchingRegex("(.)*");

  ASSERT_EQ(nodes->length(), 1U);
  EXPECT_EQ(nodes->item(0),
            GetDocument().getElementById(AtomicString("id"))->firstChild());
}

TEST_F(ContainerNodeTest, CannotFindTextIfTheValidatorRejectsIt) {
  SetBodyContent(
      R"HTML(<body><span id="id"> Find me please </span></body>)HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString("me"), [](const String&) { return false; });

  EXPECT_TRUE(text.empty());
}

TEST_F(ContainerNodeTest, CannotFindTextNodesIfTheMatcherRejectsIt) {
  SetBodyContent(
      R"HTML(<body><span id="id"> Don't find me please </span></body>)HTML");

  StaticNodeList* nodes =
      GetDocument().FindAllTextNodesMatchingRegex("not present");

  EXPECT_EQ(nodes->length(), 0U);
}

TEST_F(ContainerNodeTest, CanFindTextInElementWithManyDescendants) {
  SetBodyContent(R"HTML(
      <body>
        <div id="id">
          <div>
            No need to find this
          </div>
          <div>
            Something something here
            <div> Find me please </div>
            also over here
          </div>
          <div>
            And more information here
          </div>
        </div>
        <div>
          Hi
        </div>
      </body>
    )HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString(" me "), [](const String&) { return true; });

  EXPECT_EQ(String(" Find me please "), text);
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

TEST_F(ContainerNodeTest, FindTextInElementWithFirstMatch) {
  SetBodyContent(R"HTML(
      <body><div id="id">
        <div> Text match #1 </div>
        <div> Text match #2 </div>
      </div></body>
    )HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString(" match "), [](const String&) { return true; });

  EXPECT_EQ(String(" Text match #1 "), text);
}

TEST_F(ContainerNodeTest, FindTextInElementWithValidatorApprovingTheSecond) {
  SetBodyContent(R"HTML(
      <body><div id="id">
        <div> Text match #1 </div>
        <div> Text match #2 </div>
      </div></body>
    )HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString(" match "), [](const String& potential_match) {
        return potential_match == " Text match #2 ";
      });

  EXPECT_EQ(String(" Text match #2 "), text);
}

TEST_F(ContainerNodeTest, FindTextInElementWithSubstringIgnoresComments) {
  SetBodyContent(R"HTML(
    <body>
      <p id="id"> Before comment, <!-- The comment. --> after comment. </p>
    </body>
  )HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString("comment"), [](const String&) { return true; });

  EXPECT_EQ(String(" Before comment,  after comment. "), text);
}

TEST_F(ContainerNodeTest, FindTextInElementWithSubstringIgnoresAsciiCase) {
  SetBodyContent(R"HTML(
    <body>
      <p id="id"> MaGiC RaInBoW. </p>
    </body>
  )HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString("magic"), [](const String&) { return true; });

  EXPECT_EQ(String(" MaGiC RaInBoW. "), text);
}

TEST_F(ContainerNodeTest, CanFindTextInReadonlyTextInputElement) {
  SetBodyContent(R"HTML(
    <body>
      <p><input id="id" type="text" readonly="" value=" MaGiC RaInBoW. "></p>
    </body>
  )HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString("magic"), [](const String&) { return true; });

  EXPECT_EQ(String(" MaGiC RaInBoW. "), text);
}

TEST_F(ContainerNodeTest, CannotFindTextInNonReadonlyTextInputElement) {
  SetBodyContent(R"HTML(
    <body>
      <p><input id="id" type="text" value=" MaGiC RaInBoW. "></p>
    </body>
  )HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString("magic"), [](const String&) { return true; });

  EXPECT_TRUE(text.empty());
}

TEST_F(ContainerNodeTest, CannotFindTextInNonTextInputElement) {
  SetBodyContent(R"HTML(
    <body>
      <p><input id="id" type="url" readonly="" value=" MaGiC RaInBoW. "></p>
    </body>
  )HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString("magic"), [](const String&) { return true; });

  EXPECT_TRUE(text.empty());
}

TEST_F(ContainerNodeTest, FindTextInTheValueOfTheReadonlyInputFirst) {
  SetBodyContent(R"HTML(
    <body>
      <p><input id="id" type="text" readonly="" value="lookup value">lookup
        text children</input></p>
    </body>
  )HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString("lookup"), [](const String&) { return true; });

  EXPECT_EQ(String("lookup value"), text);
}

TEST_F(ContainerNodeTest, FindTextInTheValueOfTheReadonlyInputWithTypeTEXT) {
  SetBodyContent(R"HTML(
    <body>
      <p><input id="id" type="TEXT" readonly="" value="lookup value"></p>
    </body>
  )HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString("lookup"), [](const String&) { return true; });

  EXPECT_EQ(String("lookup value"), text);
}

TEST_F(ContainerNodeTest, CanFindTextInTextarea) {
  SetBodyContent(R"HTML(
    <body>
      <p><textarea id="id">lookup text children</textarea></p>
    </body>
  )HTML");

  String text = GetDocument().FindTextInElementWith(
      AtomicString("lookup"), [](const String&) { return true; });

  EXPECT_EQ(String("lookup text children"), text);
}

}  // namespace blink
