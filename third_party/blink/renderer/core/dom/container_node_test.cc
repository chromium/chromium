// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/container_node.h"

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
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

  String text = GetDocument().FindTextInElementWith(AtomicString("anything"));

  EXPECT_TRUE(text.empty());
}

TEST_F(ContainerNodeTest, CannotFindTextInElementWithNonTextDescendants) {
  SetBodyContent(R"HTML(<body><span id="id"> Hello
      <span></span> world! </span></body>)HTML");

  String text = GetDocument().FindTextInElementWith(AtomicString("Hello"));

  EXPECT_TRUE(text.empty());
}

TEST_F(ContainerNodeTest, CannotFindTextInElementWithoutMatchingSubtring) {
  SetBodyContent(R"HTML(<body><span id="id"> Hello </span></body>)HTML");

  String text = GetDocument().FindTextInElementWith(AtomicString("Goodbye"));

  EXPECT_TRUE(text.empty());
}

TEST_F(ContainerNodeTest, CanFindTextInElementWithOnlyTextDescendants) {
  SetBodyContent(
      R"HTML(<body><span id="id"> Find me please </span></body>)HTML");

  String text = GetDocument().FindTextInElementWith(AtomicString("me"));

  EXPECT_EQ(String(" Find me please "), text);
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

  String text = GetDocument().FindTextInElementWith(AtomicString(" me "));

  EXPECT_EQ(String(" Find me please "), text);
}

TEST_F(ContainerNodeTest, FindTextInElementWithFirstMatch) {
  SetBodyContent(R"HTML(
      <body><div id="id">
        <div> Text match #1 </div>
        <div> Text match #2 </div>
      </div></body>
    )HTML");

  String text = GetDocument().FindTextInElementWith(AtomicString(" match "));

  EXPECT_EQ(String(" Text match #1 "), text);
}

TEST_F(ContainerNodeTest, FindTextInElementWithSubstringIgnoresComments) {
  SetBodyContent(R"HTML(
    <body>
      <p id="id"> Before comment, <!-- The comment. --> after comment. </p>
    </body>
  )HTML");

  String text = GetDocument().FindTextInElementWith(AtomicString("comment"));

  EXPECT_EQ(String(" Before comment,  after comment. "), text);
}

}  // namespace blink
