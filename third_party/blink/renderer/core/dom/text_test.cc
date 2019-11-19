// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/text.h"

#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/html_pre_element.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class TextTest : public EditingTestBase {};

TEST_F(TextTest, SetDataToChangeFirstLetterTextNode) {
  SetBodyContent(
      "<style>pre::first-letter {color:red;}</style><pre "
      "id=sample>a<span>b</span></pre>");

  Node* sample = GetDocument().getElementById("sample");
  auto* text = To<Text>(sample->firstChild());
  text->setData(" ");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(text->GetLayoutObject()->IsTextFragment());
}

TEST_F(TextTest, RemoveFirstLetterPseudoElementWhenNoLetter) {
  SetBodyContent("<style>*::first-letter{font:icon;}</style><pre>AB\n</pre>");

  Element* pre = GetDocument().QuerySelector("pre");
  auto* text = To<Text>(pre->firstChild());

  auto* range = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 2);
  range->deleteContents(ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(text->GetLayoutObject()->IsTextFragment());
}

TEST_F(TextTest, TextLayoutObjectIsNeeded_CannotHaveChildren) {
  SetBodyContent("<img id=image>");
  UpdateAllLifecyclePhasesForTest();

  Element* img = GetDocument().getElementById("image");
  ASSERT_TRUE(img);

  LayoutObject* img_layout = img->GetLayoutObject();
  ASSERT_TRUE(img_layout);
  const ComputedStyle& style = img_layout->StyleRef();

  Text* text = Text::Create(GetDocument(), "dummy");

  Node::AttachContext context;
  context.parent = img_layout;
  EXPECT_FALSE(text->TextLayoutObjectIsNeeded(context, style));

  context.use_previous_in_flow = true;
  EXPECT_FALSE(text->TextLayoutObjectIsNeeded(context, style));
}

TEST_F(TextTest, TextLayoutObjectIsNeeded_EditingText) {
  SetBodyContent("<span id=parent></span>");
  UpdateAllLifecyclePhasesForTest();

  Element* parent = GetDocument().getElementById("parent");
  ASSERT_TRUE(parent);

  LayoutObject* parent_layout = parent->GetLayoutObject();
  ASSERT_TRUE(parent_layout);
  const ComputedStyle& style = parent_layout->StyleRef();

  Text* text_empty = Text::CreateEditingText(GetDocument(), "");
  Text* text_whitespace = Text::CreateEditingText(GetDocument(), " ");
  Text* text = Text::CreateEditingText(GetDocument(), "dummy");

  Node::AttachContext context;
  context.parent = parent_layout;
  EXPECT_TRUE(text_empty->TextLayoutObjectIsNeeded(context, style));
  EXPECT_TRUE(text_whitespace->TextLayoutObjectIsNeeded(context, style));
  EXPECT_TRUE(text->TextLayoutObjectIsNeeded(context, style));

  context.use_previous_in_flow = true;
  EXPECT_TRUE(text_empty->TextLayoutObjectIsNeeded(context, style));
  EXPECT_TRUE(text_whitespace->TextLayoutObjectIsNeeded(context, style));
  EXPECT_TRUE(text->TextLayoutObjectIsNeeded(context, style));
}

TEST_F(TextTest, TextLayoutObjectIsNeeded_Empty) {
  SetBodyContent("<span id=parent></span>");
  UpdateAllLifecyclePhasesForTest();

  Element* parent = GetDocument().getElementById("parent");
  ASSERT_TRUE(parent);

  LayoutObject* parent_layout = parent->GetLayoutObject();
  ASSERT_TRUE(parent_layout);
  const ComputedStyle& style = parent_layout->StyleRef();

  Text* text = Text::Create(GetDocument(), "");

  Node::AttachContext context;
  context.parent = parent_layout;
  EXPECT_FALSE(text->TextLayoutObjectIsNeeded(context, style));
  context.use_previous_in_flow = true;
  EXPECT_FALSE(text->TextLayoutObjectIsNeeded(context, style));
}

TEST_F(TextTest, TextLayoutObjectIsNeeded_Whitespace) {
  SetBodyContent(
      "<div id=block></div>Ends with whitespace "
      "<span id=inline></span>Nospace<br id=br>");
  UpdateAllLifecyclePhasesForTest();

  LayoutObject* block =
      GetDocument().getElementById("block")->GetLayoutObject();
  LayoutObject* in_line =
      GetDocument().getElementById("inline")->GetLayoutObject();
  LayoutObject* space_at_end =
      GetDocument().getElementById("block")->nextSibling()->GetLayoutObject();
  LayoutObject* no_space =
      GetDocument().getElementById("inline")->nextSibling()->GetLayoutObject();
  LayoutObject* br = GetDocument().getElementById("br")->GetLayoutObject();
  ASSERT_TRUE(block);
  ASSERT_TRUE(in_line);
  ASSERT_TRUE(space_at_end);
  ASSERT_TRUE(no_space);
  ASSERT_TRUE(br);

  Text* whitespace = Text::Create(GetDocument(), "   ");
  Node::AttachContext context;
  context.parent = block;
  EXPECT_FALSE(
      whitespace->TextLayoutObjectIsNeeded(context, block->StyleRef()));
  context.parent = in_line;
  EXPECT_FALSE(
      whitespace->TextLayoutObjectIsNeeded(context, in_line->StyleRef()));

  context.use_previous_in_flow = true;
  context.parent = block;
  EXPECT_FALSE(
      whitespace->TextLayoutObjectIsNeeded(context, block->StyleRef()));
  context.parent = in_line;
  EXPECT_TRUE(
      whitespace->TextLayoutObjectIsNeeded(context, in_line->StyleRef()));

  context.previous_in_flow = in_line;
  context.parent = block;
  EXPECT_TRUE(whitespace->TextLayoutObjectIsNeeded(context, block->StyleRef()));
  context.parent = in_line;
  EXPECT_TRUE(
      whitespace->TextLayoutObjectIsNeeded(context, in_line->StyleRef()));

  context.previous_in_flow = space_at_end;
  context.parent = block;
  EXPECT_FALSE(
      whitespace->TextLayoutObjectIsNeeded(context, block->StyleRef()));
  context.parent = in_line;
  EXPECT_FALSE(
      whitespace->TextLayoutObjectIsNeeded(context, in_line->StyleRef()));

  context.previous_in_flow = no_space;
  context.parent = block;
  EXPECT_TRUE(whitespace->TextLayoutObjectIsNeeded(context, block->StyleRef()));
  context.parent = in_line;
  EXPECT_TRUE(
      whitespace->TextLayoutObjectIsNeeded(context, in_line->StyleRef()));

  context.previous_in_flow = block;
  context.parent = block;
  EXPECT_FALSE(
      whitespace->TextLayoutObjectIsNeeded(context, block->StyleRef()));
  context.parent = in_line;
  EXPECT_FALSE(
      whitespace->TextLayoutObjectIsNeeded(context, in_line->StyleRef()));

  context.previous_in_flow = br;
  context.parent = block;
  EXPECT_FALSE(
      whitespace->TextLayoutObjectIsNeeded(context, block->StyleRef()));
  context.parent = in_line;
  EXPECT_FALSE(
      whitespace->TextLayoutObjectIsNeeded(context, in_line->StyleRef()));
}

TEST_F(TextTest, TextLayoutObjectIsNeeded_PreserveNewLine) {
  SetBodyContent(R"HTML(
    <div id=pre style='white-space:pre'></div>
    <div id=pre-line style='white-space:pre-line'></div>
    <div id=pre-wrap style='white-space:pre-wrap'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Text* text = Text::Create(GetDocument(), " ");
  Node::AttachContext context;

  Element* pre = GetDocument().getElementById("pre");
  ASSERT_TRUE(pre);
  context.parent = pre->GetLayoutObject();
  ASSERT_TRUE(context.parent);
  const ComputedStyle& pre_style = context.parent->StyleRef();
  EXPECT_TRUE(text->TextLayoutObjectIsNeeded(context, pre_style));

  Element* pre_line = GetDocument().getElementById("pre-line");
  ASSERT_TRUE(pre_line);
  context.parent = pre_line->GetLayoutObject();
  ASSERT_TRUE(context.parent);
  const ComputedStyle& pre_line_style = context.parent->StyleRef();
  EXPECT_TRUE(text->TextLayoutObjectIsNeeded(context, pre_line_style));

  Element* pre_wrap = GetDocument().getElementById("pre-wrap");
  ASSERT_TRUE(pre_wrap);
  context.parent = pre_wrap->GetLayoutObject();
  ASSERT_TRUE(context.parent);
  const ComputedStyle& pre_wrap_style = context.parent->StyleRef();
  EXPECT_TRUE(text->TextLayoutObjectIsNeeded(context, pre_wrap_style));
}

}  // namespace blink
