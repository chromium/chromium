// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"

#include <gtest/gtest.h>
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/shadow_root_init.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class WhitespaceAttacherTest : public PageTestBase {
 protected:
  void AdvanceToRebuildLayoutTree() {
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
    GetDocument().GetStyleEngine().in_layout_tree_rebuild_ = true;
  }
};

TEST_F(WhitespaceAttacherTest, WhitespaceAfterReattachedBlock) {
  GetDocument().body()->SetInnerHTMLFromString("<div id=block></div> ");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("block");
  auto* text = To<Text>(div->nextSibling());
  EXPECT_FALSE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(text->CreateTextLayoutObject(
      GetDocument().body()->ComputedStyleRef(), LegacyLayout::kAuto));

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, WhitespaceAfterReattachedInline) {
  GetDocument().body()->SetInnerHTMLFromString("<span id=inline></span> ");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById("inline");
  auto* text = To<Text>(span->nextSibling());
  EXPECT_TRUE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  attacher.DidReattachElement(span, span->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, WhitespaceAfterReattachedWhitespace) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<span id=inline></span> <!-- --> ");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById("inline");
  auto* first_whitespace = To<Text>(span->nextSibling());
  auto* second_whitespace =
      To<Text>(first_whitespace->nextSibling()->nextSibling());
  EXPECT_TRUE(first_whitespace->GetLayoutObject());
  EXPECT_FALSE(second_whitespace->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Force LayoutText on the second whitespace to see that the reattach works.
  second_whitespace->SetLayoutObject(second_whitespace->CreateTextLayoutObject(
      GetDocument().body()->ComputedStyleRef(), LegacyLayout::kAuto));

  WhitespaceAttacher attacher;
  attacher.DidVisitText(second_whitespace);
  EXPECT_TRUE(second_whitespace->GetLayoutObject());

  attacher.DidReattachText(first_whitespace);
  EXPECT_TRUE(first_whitespace->GetLayoutObject());
  EXPECT_FALSE(second_whitespace->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, VisitBlockAfterReattachedWhitespace) {
  GetDocument().body()->SetInnerHTMLFromString("<div id=block></div> ");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("block");
  auto* text = To<Text>(div->nextSibling());
  EXPECT_FALSE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  WhitespaceAttacher attacher;
  attacher.DidReattachText(text);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidVisitElement(div);
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, VisitInlineAfterReattachedWhitespace) {
  GetDocument().body()->SetInnerHTMLFromString("<span id=inline></span> ");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById("inline");
  auto* text = To<Text>(span->nextSibling());
  EXPECT_TRUE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidReattachText(text);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidVisitElement(span);
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, VisitTextAfterReattachedWhitespace) {
  GetDocument().body()->SetInnerHTMLFromString("Text<!-- --> ");
  UpdateAllLifecyclePhasesForTest();

  auto* text = To<Text>(GetDocument().body()->firstChild());
  auto* whitespace = To<Text>(text->nextSibling()->nextSibling());
  EXPECT_TRUE(text->GetLayoutObject());
  EXPECT_TRUE(whitespace->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Clear LayoutText to see that the reattach works.
  whitespace->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidReattachText(whitespace);
  EXPECT_FALSE(whitespace->GetLayoutObject());

  attacher.DidVisitText(text);
  EXPECT_TRUE(text->GetLayoutObject());
  EXPECT_TRUE(whitespace->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, ReattachWhitespaceInsideBlockExitingScope) {
  GetDocument().body()->SetInnerHTMLFromString("<div id=block> </div>");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("block");
  auto* text = To<Text>(div->firstChild());
  EXPECT_FALSE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  {
    WhitespaceAttacher attacher;
    attacher.DidReattachText(text);
    EXPECT_FALSE(text->GetLayoutObject());

    // Force LayoutText to see that the reattach works.
    text->SetLayoutObject(text->CreateTextLayoutObject(div->ComputedStyleRef(),
                                                       LegacyLayout::kAuto));
  }
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, ReattachWhitespaceInsideInlineExitingScope) {
  GetDocument().body()->SetInnerHTMLFromString("<span id=inline> </span>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById("inline");
  auto* text = To<Text>(span->firstChild());
  EXPECT_TRUE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  {
    WhitespaceAttacher attacher;
    attacher.DidReattachText(text);
    EXPECT_FALSE(text->GetLayoutObject());
  }
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, SlottedWhitespaceAfterReattachedBlock) {
  GetDocument().body()->SetInnerHTMLFromString("<div id=host> </div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.SetInnerHTMLFromString("<div id=block></div><slot></slot>");
  UpdateAllLifecyclePhasesForTest();

  Element* div = shadow_root.getElementById("block");
  auto* text = To<Text>(host->firstChild());
  EXPECT_FALSE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(text->CreateTextLayoutObject(host->ComputedStyleRef(),
                                                     LegacyLayout::kAuto));

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  EXPECT_TRUE(text->GetLayoutObject());

  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, SlottedWhitespaceAfterReattachedInline) {
  GetDocument().body()->SetInnerHTMLFromString("<div id=host> </div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.SetInnerHTMLFromString("<span id=inline></span><slot></slot>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = shadow_root.getElementById("inline");
  auto* text = To<Text>(host->firstChild());
  EXPECT_TRUE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidReattachElement(span, span->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest,
       WhitespaceInDisplayContentsAfterReattachedBlock) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<div id=block></div><span style='display:contents'> </span>");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("block");
  auto* contents = To<Element>(div->nextSibling());
  auto* text = To<Text>(contents->firstChild());
  EXPECT_FALSE(contents->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(text->CreateTextLayoutObject(
      contents->ComputedStyleRef(), LegacyLayout::kAuto));

  WhitespaceAttacher attacher;
  attacher.DidVisitElement(contents);
  EXPECT_TRUE(text->GetLayoutObject());

  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest,
       WhitespaceInDisplayContentsAfterReattachedInline) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<span id=inline></span><span style='display:contents'> </span>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById("inline");
  auto* contents = To<Element>(span->nextSibling());
  auto* text = To<Text>(contents->firstChild());
  EXPECT_FALSE(contents->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidVisitElement(contents);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidReattachElement(span, span->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest,
       WhitespaceAfterEmptyDisplayContentsAfterReattachedBlock) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<div id=block></div><span style='display:contents'></span> ");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("block");
  auto* contents = To<Element>(div->nextSibling());
  auto* text = To<Text>(contents->nextSibling());
  EXPECT_FALSE(contents->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(text->CreateTextLayoutObject(
      contents->ComputedStyleRef(), LegacyLayout::kAuto));

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  attacher.DidVisitElement(contents);
  EXPECT_TRUE(text->GetLayoutObject());

  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest,
       WhitespaceAfterDisplayContentsWithDisplayNoneChildAfterReattachedBlock) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<div id=block></div><span style='display:contents'>"
      "<span style='display:none'></span></span> ");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("block");
  auto* contents = To<Element>(div->nextSibling());
  auto* text = To<Text>(contents->nextSibling());
  EXPECT_FALSE(contents->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(text->CreateTextLayoutObject(
      contents->ComputedStyleRef(), LegacyLayout::kAuto));

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  attacher.DidVisitElement(contents);
  EXPECT_TRUE(text->GetLayoutObject());

  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, WhitespaceDeepInsideDisplayContents) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<span id=inline></span><span style='display:contents'>"
      "<span style='display:none'></span>"
      "<span id=inner style='display:contents'> </span></span>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById("inline");
  auto* contents = To<Element>(span->nextSibling());
  auto* text = To<Text>(GetDocument().getElementById("inner")->firstChild());
  EXPECT_TRUE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidVisitElement(contents);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidReattachElement(span, span->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, MultipleDisplayContents) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<span id=inline></span>"
      "<span style='display:contents'></span>"
      "<span style='display:contents'></span>"
      "<span style='display:contents'> </span>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById("inline");
  auto* first_contents = To<Element>(span->nextSibling());
  auto* second_contents = To<Element>(first_contents->nextSibling());
  auto* last_contents = To<Element>(second_contents->nextSibling());
  auto* text = To<Text>(last_contents->firstChild());
  EXPECT_TRUE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidVisitElement(last_contents);
  attacher.DidVisitElement(second_contents);
  attacher.DidVisitElement(first_contents);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidReattachElement(span, span->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, SlottedWhitespaceInsideDisplayContents) {
  GetDocument().body()->SetInnerHTMLFromString("<div id=host> </div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.SetInnerHTMLFromString(
      "<span id=inline></span>"
      "<div style='display:contents'><slot></slot></div>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = shadow_root.getElementById("inline");
  auto* contents = To<Element>(span->nextSibling());
  auto* text = To<Text>(host->firstChild());
  EXPECT_TRUE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidVisitElement(contents);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidReattachElement(span, span->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, RemoveInlineBeforeSpace) {
  GetDocument().body()->SetInnerHTMLFromString("<span id=inline></span> ");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById("inline");
  ASSERT_TRUE(span);
  EXPECT_TRUE(span->GetLayoutObject());

  Node* text = span->nextSibling();
  ASSERT_TRUE(text);
  EXPECT_TRUE(text->IsTextNode());
  EXPECT_TRUE(text->GetLayoutObject());

  span->remove();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(text->previousSibling());
  EXPECT_TRUE(text->IsTextNode());
  EXPECT_FALSE(text->nextSibling());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, RemoveInlineBeforeOutOfFlowBeforeSpace) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<span id=inline></span><div id=float style='float:right'></div> ");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById("inline");
  ASSERT_TRUE(span);
  EXPECT_TRUE(span->GetLayoutObject());

  Element* floated = GetDocument().getElementById("float");
  ASSERT_TRUE(floated);
  EXPECT_TRUE(floated->GetLayoutObject());

  Node* text = floated->nextSibling();
  ASSERT_TRUE(text);
  EXPECT_TRUE(text->IsTextNode());
  EXPECT_TRUE(text->GetLayoutObject());

  span->remove();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(text->IsTextNode());
  EXPECT_FALSE(text->nextSibling());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, RemoveSpaceBeforeSpace) {
  GetDocument().body()->SetInnerHTMLFromString("<span> <!-- --> </span>");
  UpdateAllLifecyclePhasesForTest();

  Node* span = GetDocument().body()->firstChild();
  ASSERT_TRUE(span);

  Node* space1 = span->firstChild();
  ASSERT_TRUE(space1);
  EXPECT_TRUE(space1->IsTextNode());
  EXPECT_TRUE(space1->GetLayoutObject());

  Node* space2 = span->lastChild();
  ASSERT_TRUE(space2);
  EXPECT_TRUE(space2->IsTextNode());
  EXPECT_FALSE(space2->GetLayoutObject());

  space1->remove();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(space2->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, RemoveInlineBeforeDisplayContentsWithSpace) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style>div { display: contents }</style>"
      "<div><span id=inline></span></div>"
      "<div><div><div id=innerdiv> </div></div></div>text");
  UpdateAllLifecyclePhasesForTest();

  Node* span = GetDocument().getElementById("inline");
  ASSERT_TRUE(span);

  Node* space = GetDocument().getElementById("innerdiv")->firstChild();
  ASSERT_TRUE(space);
  EXPECT_TRUE(space->IsTextNode());
  EXPECT_TRUE(space->GetLayoutObject());

  span->remove();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(space->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, RemoveBlockBeforeSpace) {
  GetDocument().body()->SetInnerHTMLFromString(
      "A<div id=block></div> <span>B</span>");
  UpdateAllLifecyclePhasesForTest();

  Node* div = GetDocument().getElementById("block");
  ASSERT_TRUE(div);

  Node* space = div->nextSibling();
  ASSERT_TRUE(space);
  EXPECT_TRUE(space->IsTextNode());
  EXPECT_FALSE(space->GetLayoutObject());

  div->remove();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(space->GetLayoutObject());
}

}  // namespace blink
