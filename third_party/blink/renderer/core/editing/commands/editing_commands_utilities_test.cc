// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"

#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class EditingCommandsUtilitiesTest : public EditingTestBase {
 protected:
  void MakeDocumentEmpty();
};

void EditingCommandsUtilitiesTest::MakeDocumentEmpty() {
  while (GetDocument().firstChild())
    GetDocument().RemoveChild(GetDocument().firstChild());
}

TEST_F(EditingCommandsUtilitiesTest, AreaIdenticalElements) {
  SetBodyContent(
      "<style>li:nth-child(even) { -webkit-user-modify: read-write; "
      "}</style><ul><li>first item</li><li>second item</li><li "
      "class=foo>third</li><li>fourth</li></ul>");
  StaticElementList* items =
      GetDocument().QuerySelectorAll("li", ASSERT_NO_EXCEPTION);
  DCHECK_EQ(items->length(), 4u);

  EXPECT_FALSE(AreIdenticalElements(*items->item(0)->firstChild(),
                                    *items->item(1)->firstChild()))
      << "Can't merge non-elements.  e.g. Text nodes";

  // Compare a LI and a UL.
  EXPECT_FALSE(
      AreIdenticalElements(*items->item(0), *items->item(0)->parentNode()))
      << "Can't merge different tag names.";

  EXPECT_FALSE(AreIdenticalElements(*items->item(0), *items->item(2)))
      << "Can't merge a element with no attributes and another element with an "
         "attribute.";

  // We can't use contenteditable attribute to make editability difference
  // because the hasEquivalentAttributes check is done earier.
  EXPECT_FALSE(AreIdenticalElements(*items->item(0), *items->item(1)))
      << "Can't merge non-editable nodes.";

  EXPECT_TRUE(AreIdenticalElements(*items->item(1), *items->item(3)));
}

TEST_F(EditingCommandsUtilitiesTest, TidyUpHTMLStructureFromBody) {
  auto* body = MakeGarbageCollected<HTMLBodyElement>(GetDocument());
  MakeDocumentEmpty();
  GetDocument().setDesignMode("on");
  GetDocument().AppendChild(body);
  TidyUpHTMLStructure(GetDocument());

  EXPECT_TRUE(IsA<HTMLHtmlElement>(GetDocument().documentElement()));
  EXPECT_EQ(body, GetDocument().body());
  EXPECT_EQ(GetDocument().documentElement(), body->parentNode());
}

TEST_F(EditingCommandsUtilitiesTest, TidyUpHTMLStructureFromDiv) {
  auto* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  MakeDocumentEmpty();
  GetDocument().setDesignMode("on");
  GetDocument().AppendChild(div);
  TidyUpHTMLStructure(GetDocument());

  EXPECT_TRUE(IsA<HTMLHtmlElement>(GetDocument().documentElement()));
  EXPECT_TRUE(IsA<HTMLBodyElement>(GetDocument().body()));
  EXPECT_EQ(GetDocument().body(), div->parentNode());
}

TEST_F(EditingCommandsUtilitiesTest, TidyUpHTMLStructureFromHead) {
  auto* head = MakeGarbageCollected<HTMLHeadElement>(GetDocument());
  MakeDocumentEmpty();
  GetDocument().setDesignMode("on");
  GetDocument().AppendChild(head);
  TidyUpHTMLStructure(GetDocument());

  EXPECT_TRUE(IsA<HTMLHtmlElement>(GetDocument().documentElement()));
  EXPECT_TRUE(IsA<HTMLBodyElement>(GetDocument().body()));
  EXPECT_EQ(GetDocument().documentElement(), head->parentNode());
}

}  // namespace blink
