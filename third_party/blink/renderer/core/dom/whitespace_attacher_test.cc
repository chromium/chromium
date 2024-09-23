// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"

#include <gtest/gtest.h>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
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
  GetDocument().body()->setInnerHTML("<div id=block></div> ");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById(AtomicString("block"));
  auto* text = To<Text>(div->nextSibling());
  EXPECT_FALSE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(text->CreateTextLayoutObject());

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, WhitespaceAfterReattachedInline) {
  GetDocument().body()->setInnerHTML("<span id=inline></span> ");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById(AtomicString("inline"));
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
  GetDocument().body()->setInnerHTML("<span id=inline></span> <!-- --> ");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById(AtomicString("inline"));
  auto* first_whitespace = To<Text>(span->nextSibling());
  auto* second_whitespace =
      To<Text>(first_whitespace->nextSibling()->nextSibling());
  EXPECT_TRUE(first_whitespace->GetLayoutObject());
  EXPECT_FALSE(second_whitespace->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Force LayoutText on the second whitespace to see that the reattach works.
  second_whitespace->SetLayoutObject(
      second_whitespace->CreateTextLayoutObject());

  WhitespaceAttacher attacher;
  attacher.DidVisitText(second_whitespace);
  EXPECT_TRUE(second_whitespace->GetLayoutObject());

  attacher.DidReattachText(first_whitespace);
  EXPECT_TRUE(first_whitespace->GetLayoutObject());
  EXPECT_FALSE(second_whitespace->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, VisitBlockAfterReattachedWhitespace) {
  GetDocument().body()->setInnerHTML("<div id=block></div> ");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById(AtomicString("block"));
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
  GetDocument().body()->setInnerHTML("<span id=inline></span> ");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById(AtomicString("inline"));
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
  GetDocument().body()->setInnerHTML("Text<!-- --> ");
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
  GetDocument().body()->setInnerHTML("<div id=block> </div>");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById(AtomicString("block"));
  auto* text = To<Text>(div->firstChild());
  EXPECT_FALSE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  {
    WhitespaceAttacher attacher;
    attacher.DidReattachText(text);
    EXPECT_FALSE(text->GetLayoutObject());

    // Force LayoutText to see that the reattach works.
    text->SetLayoutObject(text->CreateTextLayoutObject());
  }
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, ReattachWhitespaceInsideInlineExitingScope) {
  GetDocument().body()->setInnerHTML("<span id=inline> </span>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById(AtomicString("inline"));
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
  GetDocument().body()->setInnerHTML("<div id=host> </div>");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<div id=block></div><slot></slot>");
  UpdateAllLifecyclePhasesForTest();

  Element* div = shadow_root.getElementById(AtomicString("block"));
  auto* text = To<Text>(host->firstChild());
  EXPECT_FALSE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(text->CreateTextLayoutObject());

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  EXPECT_TRUE(text->GetLayoutObject());

  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, SlottedWhitespaceAfterReattachedInline) {
  GetDocument().body()->setInnerHTML("<div id=host> </div>");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<span id=inline></span><slot></slot>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = shadow_root.getElementById(AtomicString("inline"));
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
  GetDocument().body()->setInnerHTML(
      "<div id=block></div><span style='display:contents'> </span>");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById(AtomicString("block"));
  auto* contents = To<Element>(div->nextSibling());
  auto* text = To<Text>(contents->firstChild());
  EXPECT_FALSE(contents->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(text->CreateTextLayoutObject());

  WhitespaceAttacher attacher;
  attacher.DidVisitElement(contents);
  EXPECT_TRUE(text->GetLayoutObject());

  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest,
       WhitespaceInDisplayContentsAfterReattachedInline) {
  GetDocument().body()->setInnerHTML(
      "<span id=inline></span><span style='display:contents'> </span>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById(AtomicString("inline"));
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
  GetDocument().body()->setInnerHTML(
      "<div id=block></div><span style='display:contents'></span> ");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById(AtomicString("block"));
  auto* contents = To<Element>(div->nextSibling());
  auto* text = To<Text>(contents->nextSibling());
  EXPECT_FALSE(contents->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(text->CreateTextLayoutObject());

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  attacher.DidVisitElement(contents);
  EXPECT_TRUE(text->GetLayoutObject());

  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest,
       WhitespaceAfterDisplayContentsWithDisplayNoneChildAfterReattachedBlock) {
  GetDocument().body()->setInnerHTML(
      "<div id=block></div><span style='display:contents'>"
      "<span style='display:none'></span></span> ");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById(AtomicString("block"));
  auto* contents = To<Element>(div->nextSibling());
  auto* text = To<Text>(contents->nextSibling());
  EXPECT_FALSE(contents->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());

  AdvanceToRebuildLayoutTree();

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(text->CreateTextLayoutObject());

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  attacher.DidVisitElement(contents);
  EXPECT_TRUE(text->GetLayoutObject());

  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, WhitespaceDeepInsideDisplayContents) {
  GetDocument().body()->setInnerHTML(
      "<span id=inline></span><span style='display:contents'>"
      "<span style='display:none'></span>"
      "<span id=inner style='display:contents'> </span></span>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById(AtomicString("inline"));
  auto* contents = To<Element>(span->nextSibling());
  auto* text = To<Text>(
      GetDocument().getElementById(AtomicString("inner"))->firstChild());
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
  GetDocument().body()->setInnerHTML(
      "<span id=inline></span>"
      "<span style='display:contents'></span>"
      "<span style='display:contents'></span>"
      "<span style='display:contents'> </span>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById(AtomicString("inline"));
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
  GetDocument().body()->setInnerHTML("<div id=host> </div>");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(
      "<span id=inline></span>"
      "<div style='display:contents'><slot></slot></div>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = shadow_root.getElementById(AtomicString("inline"));
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
  GetDocument().body()->setInnerHTML("<span id=inline></span> ");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById(AtomicString("inline"));
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
  GetDocument().body()->setInnerHTML(
      "<span id=inline></span><div id=float style='float:right'></div> ");
  UpdateAllLifecyclePhasesForTest();

  Element* span = GetDocument().getElementById(AtomicString("inline"));
  ASSERT_TRUE(span);
  EXPECT_TRUE(span->GetLayoutObject());

  Element* floated = GetDocument().getElementById(AtomicString("float"));
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
  GetDocument().body()->setInnerHTML("<span> <!-- --> </span>");
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

TEST_F(WhitespaceAttacherTest, CreateSpaceForScrollMarkerGroup) {
  GetDocument().body()->setInnerHTML(
      "<span>x</span> <span id=test></span> <span>y</span>"
      "<style>"
      "#test { scroll-marker-group: before; overflow: auto; }"
      "#test::scroll-marker-group { background: green; display: inline-flex; "
      "width: 100px; height: 100px; }"
      "</style>");
  UpdateAllLifecyclePhasesForTest();

  Node* span = GetDocument().body()->firstChild();
  Node* first_space = LayoutTreeBuilderTraversal::NextLayoutSibling(*span);
  ASSERT_TRUE(first_space);
  EXPECT_TRUE(first_space->IsTextNode());
  EXPECT_TRUE(first_space->GetLayoutObject());

  Node* scroll_marker_group =
      LayoutTreeBuilderTraversal::NextLayoutSibling(*first_space);
  ASSERT_TRUE(scroll_marker_group);
  EXPECT_TRUE(scroll_marker_group->IsScrollMarkerGroupBeforePseudoElement());
  EXPECT_TRUE(scroll_marker_group->GetLayoutObject());

  Node* scroller =
      LayoutTreeBuilderTraversal::NextLayoutSibling(*scroll_marker_group);
  ASSERT_TRUE(scroller);

  Node* space2 = LayoutTreeBuilderTraversal::NextLayoutSibling(*scroller);
  ASSERT_TRUE(space2);
  EXPECT_TRUE(space2->IsTextNode());
  EXPECT_TRUE(space2->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, RemoveSpaceForScrollMarkerGroup) {
  GetDocument().body()->setInnerHTML(
      "<span>x</span> <span id=test></span> <span>y</span>"
      "<style>"
      "#test { scroll-marker-group: after; overflow: auto; }"
      "#test::scroll-marker-group { background: green; display: inline-flex; "
      "width: 100px; height: 100px; }"
      "</style>");
  UpdateAllLifecyclePhasesForTest();

  Node* span = GetDocument().body()->firstChild();
  Node* first_space = LayoutTreeBuilderTraversal::NextLayoutSibling(*span);
  ASSERT_TRUE(first_space);
  EXPECT_TRUE(first_space->IsTextNode());
  EXPECT_TRUE(first_space->GetLayoutObject());

  Node* scroller = LayoutTreeBuilderTraversal::NextLayoutSibling(*first_space);

  ASSERT_TRUE(scroller);
  Node* scroll_marker_group =
      LayoutTreeBuilderTraversal::NextLayoutSibling(*scroller);
  ASSERT_TRUE(scroll_marker_group);
  EXPECT_TRUE(scroll_marker_group->IsScrollMarkerGroupAfterPseudoElement());
  EXPECT_TRUE(scroll_marker_group->GetLayoutObject());

  Node* space2 =
      LayoutTreeBuilderTraversal::NextLayoutSibling(*scroll_marker_group);
  ASSERT_TRUE(space2);
  EXPECT_TRUE(space2->IsTextNode());
  EXPECT_TRUE(space2->GetLayoutObject());

  To<Element>(scroller)->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                                CSSValueID::kFlex);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(space2->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, RemoveInlineBeforeDisplayContentsWithSpace) {
  GetDocument().body()->setInnerHTML(
      "<style>div { display: contents }</style>"
      "<div><span id=inline></span></div>"
      "<div><div><div id=innerdiv> </div></div></div>text");
  UpdateAllLifecyclePhasesForTest();

  Node* span = GetDocument().getElementById(AtomicString("inline"));
  ASSERT_TRUE(span);

  Node* space =
      GetDocument().getElementById(AtomicString("innerdiv"))->firstChild();
  ASSERT_TRUE(space);
  EXPECT_TRUE(space->IsTextNode());
  EXPECT_TRUE(space->GetLayoutObject());

  span->remove();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(space->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, RemoveBlockBeforeSpace) {
  GetDocument().body()->setInnerHTML("A<div id=block></div> <span>B</span>");
  UpdateAllLifecyclePhasesForTest();

  Node* div = GetDocument().getElementById(AtomicString("block"));
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
