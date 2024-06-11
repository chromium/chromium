// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/node.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class FakeMediaControlElement : public HTMLDivElement {
 public:
  FakeMediaControlElement(Document& document) : HTMLDivElement(document) {}

  bool IsMediaControlElement() const override { return true; }
};

class FakeMediaControls : public HTMLDivElement {
 public:
  FakeMediaControls(Document& document) : HTMLDivElement(document) {}

  bool IsMediaControls() const override { return true; }
};

class NodeTest : public EditingTestBase {
 protected:
  LayoutObject* ReattachLayoutTreeForNode(Node& node) {
    node.SetForceReattachLayoutTree();
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
    GetDocument().GetStyleEngine().RecalcStyle();
    Node::AttachContext context;
    context.parent = LayoutTreeBuilderTraversal::ParentLayoutObject(node);
    GetDocument().GetStyleEngine().in_layout_tree_rebuild_ = true;
    node.ReattachLayoutTree(context);
    return context.previous_in_flow;
  }

  // Generate the following DOM structure and return the innermost <div>.
  //  + div#root
  //    + #shadow
  //      + test node
  //      |  + #shadow
  //      |    + div class="test"
  Node* InitializeUserAgentShadowTree(Element* test_node) {
    SetBodyContent("<div id=\"root\"></div>");
    Element* root = GetDocument().getElementById(AtomicString("root"));
    ShadowRoot& first_shadow = root->CreateUserAgentShadowRoot();

    first_shadow.AppendChild(test_node);
    ShadowRoot& second_shadow = test_node->CreateUserAgentShadowRoot();

    auto* class_div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
    class_div->setAttribute(html_names::kClassAttr, AtomicString("test"));
    second_shadow.AppendChild(class_div);
    return class_div;
  }
};

TEST_F(NodeTest, canStartSelection) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  Node* two = GetDocument().getElementById(AtomicString("two"));

  EXPECT_FALSE(one->CanStartSelection());
  EXPECT_FALSE(one->firstChild()->CanStartSelection());
  EXPECT_TRUE(two->CanStartSelection());
  EXPECT_TRUE(two->firstChild()->CanStartSelection());
}

TEST_F(NodeTest, canStartSelectionWithShadowDOM) {
  const char* body_content = "<div id=host><span id=one>one</span></div>";
  const char* shadow_content = "<a href='http://www.msn.com'><slot></slot></a>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  Node* one = GetDocument().getElementById(AtomicString("one"));

  EXPECT_FALSE(one->CanStartSelection());
  EXPECT_FALSE(one->firstChild()->CanStartSelection());
}

TEST_F(NodeTest, customElementState) {
  const char* body_content = "<div id=div></div>";
  SetBodyContent(body_content);
  Element* div = GetDocument().getElementById(AtomicString("div"));
  EXPECT_EQ(CustomElementState::kUncustomized, div->GetCustomElementState());
  EXPECT_TRUE(div->IsDefined());

  div->SetCustomElementState(CustomElementState::kUndefined);
  EXPECT_EQ(CustomElementState::kUndefined, div->GetCustomElementState());
  EXPECT_FALSE(div->IsDefined());

  div->SetCustomElementState(CustomElementState::kCustom);
  EXPECT_EQ(CustomElementState::kCustom, div->GetCustomElementState());
  EXPECT_TRUE(div->IsDefined());
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_TextRoot) {
  SetBodyContent("Text");
  Node* root = GetDocument().body()->firstChild();
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_InlineRoot) {
  SetBodyContent("<span id=root>Text <span></span></span>");
  Element* root = GetDocument().getElementById(AtomicString("root"));
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_BlockRoot) {
  SetBodyContent("<div id=root>Text <span></span></div>");
  Element* root = GetDocument().getElementById(AtomicString("root"));
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_FloatRoot) {
  SetBodyContent("<div id=root style='float:left'><span></span></div>");
  Element* root = GetDocument().getElementById(AtomicString("root"));
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_FALSE(previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_AbsoluteRoot) {
  SetBodyContent("<div id=root style='position:absolute'><span></span></div>");
  Element* root = GetDocument().getElementById(AtomicString("root"));
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_FALSE(previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Text) {
  SetBodyContent("<div id=root style='display:contents'>Text</div>");
  Element* root = GetDocument().getElementById(AtomicString("root"));
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->firstChild()->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Inline) {
  SetBodyContent("<div id=root style='display:contents'><span></span></div>");
  Element* root = GetDocument().getElementById(AtomicString("root"));
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->firstChild()->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Block) {
  SetBodyContent("<div id=root style='display:contents'><div></div></div>");
  Element* root = GetDocument().getElementById(AtomicString("root"));
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->firstChild()->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Float) {
  SetBodyContent(
      "<style>"
      "  #root { display:contents }"
      "  .float { float:left }"
      "</style>"
      "<div id=root><div class=float></div></div>");
  Element* root = GetDocument().getElementById(AtomicString("root"));
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_FALSE(previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_AbsolutePositioned) {
  SetBodyContent(
      "<style>"
      "  #root { display:contents }"
      "  .abs { position:absolute }"
      "</style>"
      "<div id=root><div class=abs></div></div>");
  Element* root = GetDocument().getElementById(AtomicString("root"));
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_FALSE(previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_SkipAbsolute) {
  SetBodyContent(
      "<style>"
      "  #root { display:contents }"
      "  .abs { position:absolute }"
      "</style>"
      "<div id=root>"
      "<div class=abs></div><span id=inline></span><div class=abs></div>"
      "</div>");
  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* span = GetDocument().getElementById(AtomicString("inline"));
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(span->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_SkipFloats) {
  SetBodyContent(
      "<style>"
      "  #root { display:contents }"
      "  .float { float:left }"
      "</style>"
      "<div id=root>"
      "<div class=float></div>"
      "<span id=inline></span>"
      "<div class=float></div>"
      "</div>");
  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* span = GetDocument().getElementById(AtomicString("inline"));
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(span->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_InsideDisplayContents) {
  SetBodyContent(
      "<style>"
      "  #root, .contents { display:contents }"
      "  .float { float:left }"
      "</style>"
      "<div id=root>"
      "<span></span><div class=contents><span id=inline></span></div>"
      "</div>");
  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* span = GetDocument().getElementById(AtomicString("inline"));
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(span->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Slotted) {
  SetBodyContent("<div id=host><span id=inline></span></div>");
  ShadowRoot& shadow_root =
      GetDocument()
          .getElementById(AtomicString("host"))
          ->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(
      "<div id=root style='display:contents'><span></span><slot></slot></div>");
  UpdateAllLifecyclePhasesForTest();

  Element* root = shadow_root.getElementById(AtomicString("root"));
  Element* span = GetDocument().getElementById(AtomicString("inline"));
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(span->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, HasMediaControlAncestor_Fail) {
  auto* node = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  EXPECT_FALSE(node->HasMediaControlAncestor());
  EXPECT_FALSE(InitializeUserAgentShadowTree(node)->HasMediaControlAncestor());
}

TEST_F(NodeTest, HasMediaControlAncestor_MediaControlElement) {
  FakeMediaControlElement* node =
      MakeGarbageCollected<FakeMediaControlElement>(GetDocument());
  EXPECT_TRUE(node->HasMediaControlAncestor());
  EXPECT_TRUE(InitializeUserAgentShadowTree(node)->HasMediaControlAncestor());
}

TEST_F(NodeTest, HasMediaControlAncestor_MediaControls) {
  FakeMediaControls* node =
      MakeGarbageCollected<FakeMediaControls>(GetDocument());
  EXPECT_TRUE(node->HasMediaControlAncestor());
  EXPECT_TRUE(InitializeUserAgentShadowTree(node)->HasMediaControlAncestor());
}

TEST_F(NodeTest, appendChildProcessingInstructionNoStyleRecalc) {
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
  auto* pi =
      MakeGarbageCollected<ProcessingInstruction>(GetDocument(), "A", "B");
  GetDocument().body()->appendChild(pi, ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
}

TEST_F(NodeTest, appendChildCommentNoStyleRecalc) {
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
  Comment* comment = Comment::Create(GetDocument(), "comment");
  GetDocument().body()->appendChild(comment, ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
}

TEST_F(NodeTest, MutationOutsideFlatTreeStyleDirty) {
  SetBodyContent("<div id=host><span id=nonslotted></span></div>");
  GetDocument()
      .getElementById(AtomicString("host"))
      ->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument()
      .getElementById(AtomicString("nonslotted"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("color:green"));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(NodeTest, SkipStyleDirtyHostChild) {
  SetBodyContent("<div id=host><span></span></div>");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<div style='display:none'><slot></slot></div>");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  // Check that we do not mark an element for style recalc when the element and
  // its flat tree parent are display:none.
  To<Element>(host->firstChild())
      ->setAttribute(html_names::kStyleAttr, AtomicString("color:green"));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(NodeTest, ContainsChild) {
  SetBodyContent("<div id=a><div id=b></div></div>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  EXPECT_TRUE(a->contains(b));
}

TEST_F(NodeTest, ContainsNoSibling) {
  SetBodyContent("<div id=a></div><div id=b></div>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  EXPECT_FALSE(a->contains(b));
}

TEST_F(NodeTest, ContainsPseudo) {
  SetBodyContent(
      "<style>#a::before{content:'aaa';}</style>"
      "<div id=a></div>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  PseudoElement* pseudo = a->GetPseudoElement(kPseudoIdBefore);
  ASSERT_TRUE(pseudo);
  EXPECT_TRUE(a->contains(pseudo));
}

TEST_F(NodeTest, SkipForceReattachDisplayNone) {
  SetBodyContent("<div id=host><span style='display:none'></span></div>");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<slot name='target'></slot>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = To<Element>(host->firstChild());
  span->setAttribute(html_names::kSlotAttr, AtomicString("target"));
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();

  // Node::FlatTreeParentChanged for a display:none could trigger style recalc,
  // but we should skip a forced re-attach for nodes with a null ComputedStyle.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_TRUE(span->NeedsStyleRecalc());
  EXPECT_FALSE(span->GetForceReattachLayoutTree());
}

TEST_F(NodeTest, UpdateChildDirtyAncestorsOnSlotAssignment) {
  SetBodyContent("<div id=host><span></span></div>");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(
      "<div><slot></slot></div><div id='child-dirty'><slot "
      "name='target'></slot></div>");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  auto* span = To<Element>(host->firstChild());
  auto* ancestor = shadow_root.getElementById(AtomicString("child-dirty"));

  // Make sure the span is dirty before the re-assignment.
  span->setAttribute(html_names::kStyleAttr, AtomicString("color:green"));
  EXPECT_FALSE(ancestor->ChildNeedsStyleRecalc());

  // Re-assign to second slot.
  span->setAttribute(html_names::kSlotAttr, AtomicString("target"));
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
  EXPECT_TRUE(ancestor->ChildNeedsStyleRecalc());
}

TEST_F(NodeTest, UpdateChildDirtySlotAfterRemoval) {
  SetBodyContent(R"HTML(
    <div id="host"><span style="display:contents"></span></div>
  )HTML");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<slot></slot>");
  UpdateAllLifecyclePhasesForTest();

  auto* span = To<Element>(host->firstChild());
  auto* slot = shadow_root.firstChild();

  // Make sure the span is dirty, and the slot marked child-dirty before the
  // removal.
  span->setAttribute(html_names::kStyleAttr, AtomicString("color:green"));
  EXPECT_TRUE(span->NeedsStyleRecalc());
  EXPECT_TRUE(slot->ChildNeedsStyleRecalc());
  EXPECT_TRUE(host->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsStyleRecalc());

  // The StyleRecalcRoot is now the span. Removing the span should clear the
  // root and the child-dirty bits on the ancestors.
  span->remove();

  EXPECT_FALSE(slot->ChildNeedsStyleRecalc());
  EXPECT_FALSE(host->ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().GetStyleEngine().NeedsStyleRecalc());
}

TEST_F(NodeTest, UpdateChildDirtyAfterSlotRemoval) {
  SetBodyContent(R"HTML(
    <div id="host"><span style="display:contents"></span></div>
  )HTML");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<div><slot></slot></div>");
  UpdateAllLifecyclePhasesForTest();

  auto* span = To<Element>(host->firstChild());
  auto* div = shadow_root.firstChild();
  auto* slot = div->firstChild();

  // Make sure the span is dirty, and the slot marked child-dirty before the
  // removal.
  span->setAttribute(html_names::kStyleAttr, AtomicString("color:green"));
  EXPECT_TRUE(span->NeedsStyleRecalc());
  EXPECT_TRUE(slot->ChildNeedsStyleRecalc());
  EXPECT_TRUE(div->ChildNeedsStyleRecalc());
  EXPECT_TRUE(host->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsStyleRecalc());

  // The StyleRecalcRoot is now the span. Removing the slot breaks the flat
  // tree ancestor chain so that the span is no longer in the flat tree. The
  // StyleRecalcRoot is cleared.
  slot->remove();

  EXPECT_FALSE(div->ChildNeedsStyleRecalc());
  EXPECT_FALSE(host->ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().GetStyleEngine().NeedsStyleRecalc());
}

TEST_F(NodeTest, UpdateChildDirtyAfterSlottingDirtyNode) {
  SetBodyContent("<div id=host><span></span></div>");

  auto* host = GetDocument().getElementById(AtomicString("host"));
  auto* span = To<Element>(host->firstChild());

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<div><slot name=x></slot></div>");
  UpdateAllLifecyclePhasesForTest();

  // Make sure the span is style dirty.
  span->setAttribute(html_names::kStyleAttr, AtomicString("color:green"));

  // Assign span to slot.
  span->setAttribute(html_names::kSlotAttr, AtomicString("x"));

  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();

  // Make sure shadow tree div and slot are marked with ChildNeedsStyleRecalc
  // when the dirty span is slotted in.
  EXPECT_TRUE(shadow_root.firstChild()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(shadow_root.firstChild()->firstChild()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(span->NeedsStyleRecalc());

  // This used to call a DCHECK failure. Make sure we don't regress.
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(NodeTest, ReassignStyleDirtyElementIntoSlotOutsideFlatTree) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div>
      <template shadowrootmode="open">
        <div>
          <slot name="s1"></slot>
        </div>
        <div>
          <template shadowrootmode="open">
            <div></div>
          </template>
          <slot name="s2"></slot>
        </div>
      </template>
      <span id="slotted" slot="s1"></span>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* slotted = GetDocument().getElementById(AtomicString("slotted"));

  // Starts with #slotted in the flat tree as a child of the s1 slot.
  EXPECT_TRUE(slotted->GetComputedStyle());

  // Mark #slotted dirty.
  slotted->SetInlineStyleProperty(CSSPropertyID::kColor, "orange");
  EXPECT_TRUE(slotted->NeedsStyleRecalc());

  // Mark for slot reassignment. The #s2 slot is outside the flat tree because
  // its parent is a shadow host with no slots in the shadow tree.
  slotted->setAttribute(html_names::kSlotAttr, AtomicString("s2"));

  // After doing the slot assignment, the #slotted element should no longer be
  // marked dirty and its ComputedStyle should be null because it's outside the
  // flat tree.
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
  EXPECT_FALSE(slotted->NeedsStyleRecalc());
  EXPECT_FALSE(slotted->GetComputedStyle());
}

TEST_F(NodeTest, FlatTreeParentForChildDirty) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div id="host">
      <template shadowrootmode="open">
        <slot id="slot1">
          <span id="fallback1"></span>
        </slot>
        <slot id="slot2">
          <span id="fallback2"></span>
        </slot>
      </template>
      <div id="slotted"></div>
      <div id="not_slotted" slot="notfound"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* host = GetDocument().getElementById(AtomicString("host"));
  Element* slotted = GetDocument().getElementById(AtomicString("slotted"));
  Element* not_slotted =
      GetDocument().getElementById(AtomicString("not_slotted"));

  ShadowRoot* shadow_root = host->GetShadowRoot();
  Element* slot1 = shadow_root->getElementById(AtomicString("slot1"));
  Element* slot2 = shadow_root->getElementById(AtomicString("slot2"));
  Element* fallback1 = shadow_root->getElementById(AtomicString("fallback1"));
  Element* fallback2 = shadow_root->getElementById(AtomicString("fallback2"));

  EXPECT_EQ(host->FlatTreeParentForChildDirty(), GetDocument().body());
  EXPECT_EQ(slot1->FlatTreeParentForChildDirty(), host);
  EXPECT_EQ(slot2->FlatTreeParentForChildDirty(), host);
  EXPECT_EQ(slotted->FlatTreeParentForChildDirty(), slot1);
  EXPECT_EQ(not_slotted->FlatTreeParentForChildDirty(), nullptr);
  EXPECT_EQ(fallback1->FlatTreeParentForChildDirty(), nullptr);
  EXPECT_EQ(fallback2->FlatTreeParentForChildDirty(), slot2);
}

}  // namespace blink
