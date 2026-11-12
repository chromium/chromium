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
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_recalc_forbidden_scope.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
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
    Element* root = GetElementById("root");
    ShadowRoot& first_shadow = root->CreateUserAgentShadowRoot();

    first_shadow.AppendChild(test_node);
    ShadowRoot& second_shadow = test_node->CreateUserAgentShadowRoot();

    auto* class_div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
    class_div->setAttribute(html_names::kClassAttr, AtomicString("test"));
    second_shadow.AppendChild(class_div);
    return class_div;
  }

  Node* GetStyleRecalcRoot() const {
    return GetDocument().GetStyleEngine().style_recalc_root_.GetRootNode();
  }
};

TEST_F(NodeTest, canStartSelection) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetElementById("one");
  Node* two = GetElementById("two");

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
  Node* one = GetElementById("one");

  EXPECT_FALSE(one->CanStartSelection());
  EXPECT_FALSE(one->firstChild()->CanStartSelection());
}

TEST_F(NodeTest, customElementState) {
  const char* body_content = "<div id=div></div>";
  SetBodyContent(body_content);
  Element* div = GetElementById("div");
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
  Element* root = GetElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_BlockRoot) {
  SetBodyContent("<div id=root>Text <span></span></div>");
  Element* root = GetElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_FloatRoot) {
  SetBodyContent("<div id=root style='float:left'><span></span></div>");
  Element* root = GetElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_FALSE(previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_AbsoluteRoot) {
  SetBodyContent("<div id=root style='position:absolute'><span></span></div>");
  Element* root = GetElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_FALSE(previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Text) {
  SetBodyContent("<div id=root style='display:contents'>Text</div>");
  Element* root = GetElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->firstChild()->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Inline) {
  SetBodyContent("<div id=root style='display:contents'><span></span></div>");
  Element* root = GetElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->firstChild()->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Block) {
  SetBodyContent("<div id=root style='display:contents'><div></div></div>");
  Element* root = GetElementById("root");
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
  Element* root = GetElementById("root");
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
  Element* root = GetElementById("root");
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
  Element* root = GetElementById("root");
  Element* span = GetElementById("inline");
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
  Element* root = GetElementById("root");
  Element* span = GetElementById("inline");
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
  Element* root = GetElementById("root");
  Element* span = GetElementById("inline");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(span->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Slotted) {
  SetBodyContent("<div id=host><span id=inline></span></div>");
  ShadowRoot& shadow_root =
      GetElementById("host")->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<div id=root style='display:contents'><span></span><slot></slot></div>");
  UpdateAllLifecyclePhasesForTest();

  Element* root = shadow_root.getElementById(AtomicString("root"));
  Element* span = GetElementById("inline");
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
  GetElementById("host")->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  GetElementById("nonslotted")
      ->setAttribute(html_names::kStyleAttr, AtomicString("color:green"));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(NodeTest, SkipStyleDirtyHostChild) {
  SetBodyContent("<div id=host><span></span></div>");
  Element* host = GetElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<div style='display:none'><slot></slot></div>");
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
  Element* a = GetElementById("a");
  Element* b = GetElementById("b");
  EXPECT_TRUE(a->contains(b));
}

TEST_F(NodeTest, ContainsNoSibling) {
  SetBodyContent("<div id=a></div><div id=b></div>");
  Element* a = GetElementById("a");
  Element* b = GetElementById("b");
  EXPECT_FALSE(a->contains(b));
}

TEST_F(NodeTest, ContainsPseudo) {
  SetBodyContent(
      "<style>#a::before{content:'aaa';}</style>"
      "<div id=a></div>");
  Element* a = GetElementById("a");
  PseudoElement* pseudo = a->GetPseudoElement(kPseudoIdBefore);
  ASSERT_TRUE(pseudo);
  EXPECT_TRUE(a->contains(pseudo));
}

TEST_F(NodeTest, SkipForceReattachDisplayNone) {
  SetBodyContent("<div id=host><span style='display:none'></span></div>");
  Element* host = GetElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes("<slot name='target'></slot>");
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
  Element* host = GetElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
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
  Element* host = GetElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes("<slot></slot>");
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
  Element* host = GetElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes("<div><slot></slot></div>");
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

  auto* host = GetElementById("host");
  auto* span = To<Element>(host->firstChild());

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<div><slot name=x></slot></div>");
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
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
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

  Element* slotted = GetElementById("slotted");

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
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
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

  Element* host = GetElementById("host");
  Element* slotted = GetElementById("slotted");
  Element* not_slotted = GetElementById("not_slotted");

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

TEST_F(NodeTest, moveBefore_DetachAfterSlotReassignment) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <!DOCTYPE html>
    <div id="host">
      <template shadowRootMode="open"><slot></slot></template>
    </div>
    <span id="span" slot="foo"></span>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* host = GetElementById("host");
  Element* span = GetElementById("span");

  host->moveBefore(span, nullptr, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(span->GetComputedStyle());
  EXPECT_TRUE(span->NeedsStyleRecalc());

  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
  EXPECT_FALSE(span->GetComputedStyle());
  EXPECT_FALSE(span->NeedsStyleRecalc());
}

TEST_F(NodeTest, moveBefore_PreserveComputedStyleBetweenTrees) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <!DOCTYPE html>
    <div id="host">
      <template shadowRootMode="open"><slot name="foo"></slot></template>
    </div>
    <span id="span" slot="foo"></span>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* host = GetElementById("host");
  Element* span = GetElementById("span");

  const ComputedStyle* old_computed_style = span->GetComputedStyle();

  host->moveBefore(span, nullptr, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(span->GetComputedStyle(), old_computed_style);

  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
  EXPECT_EQ(span->GetComputedStyle(), old_computed_style);
  EXPECT_TRUE(span->NeedsStyleRecalc());
}

TEST_F(NodeTest, moveBefore_ShadowDomInvalidationAndDetach) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <!DOCTYPE html>
    <div id="visible_host">
      <template shadowrootmode="open">
        <style>#child1 { color: rgb(0, 128, 0); }</style>
      </template>
    </div>
    <div id="hidden_host" style="display: none">
      <template shadowrootmode="open"></template>
    </div>
    <div id="slotless_host">
      <template shadowrootmode="open"><div>Shadow content</div></template>
    </div>
    <div id="slotted_host"><template shadowrootmode="open"><slot id="active_slot"></slot></template><span id="slotted_span">Assigned</span></div>
    <div id="newly_slotted_host">
      <template shadowrootmode="open"><slot id="newly_active_slot"></slot></template>
    </div>
    <div id="manual_host">
      <template shadowrootmode="open" shadowrootslotassignment="manual">
        <slot id="manual_slot"></slot>
      </template>
    </div>
    <div id="roundtrip_host"><template shadowrootmode="open"><slot id="roundtrip_slot"></slot></template><div id="roundtrip_item">Hello</div></div>
    <div id="other"></div>
    <div id="item1"><span id="child1">Item 1</span></div>
    <div id="item2">Item 2</div>
    <div id="item3">Item 3</div>
    <div id="item4">Item 4</div>
    <div id="item5">Item 5</div>
    <div id="item6">Item 6</div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* visible_host =
      GetDocument().getElementById(AtomicString("visible_host"));
  Element* hidden_host =
      GetDocument().getElementById(AtomicString("hidden_host"));
  Element* slotless_host =
      GetDocument().getElementById(AtomicString("slotless_host"));
  Element* active_slot = GetDocument()
                             .getElementById(AtomicString("slotted_host"))
                             ->GetShadowRoot()
                             ->getElementById(AtomicString("active_slot"));
  Element* slotted_span =
      GetDocument().getElementById(AtomicString("slotted_span"));
  Element* newly_slotted_host =
      GetDocument().getElementById(AtomicString("newly_slotted_host"));
  Element* newly_active_slot =
      newly_slotted_host->GetShadowRoot()->getElementById(
          AtomicString("newly_active_slot"));
  Element* manual_host =
      GetDocument().getElementById(AtomicString("manual_host"));
  Element* roundtrip_host =
      GetDocument().getElementById(AtomicString("roundtrip_host"));
  Element* roundtrip_slot = roundtrip_host->GetShadowRoot()->getElementById(
      AtomicString("roundtrip_slot"));
  Element* roundtrip_item =
      GetDocument().getElementById(AtomicString("roundtrip_item"));
  Element* other = GetDocument().getElementById(AtomicString("other"));
  Element* item1 = GetDocument().getElementById(AtomicString("item1"));
  Element* child1 = GetDocument().getElementById(AtomicString("child1"));
  Element* item2 = GetDocument().getElementById(AtomicString("item2"));
  Element* item3 = GetDocument().getElementById(AtomicString("item3"));
  Element* item4 = GetDocument().getElementById(AtomicString("item4"));
  Element* item5 = GetDocument().getElementById(AtomicString("item5"));
  Element* item6 = GetDocument().getElementById(AtomicString("item6"));

  // 1. Moving into a visible ShadowRoot marks subtree style recalc (even if
  // the moved element already had kLocalStyleChange) and force-reattach, and
  // reattaches under the shadow host (crbug.com/575200203).
  item1->SetInlineStyleProperty(CSSPropertyID::kColor, "blue");
  EXPECT_EQ(item1->GetStyleChangeType(), kLocalStyleChange);
  const ComputedStyle* old_style = item1->GetComputedStyle();
  const ComputedStyle* old_child_style = child1->GetComputedStyle();
  visible_host->GetShadowRoot()->moveBefore(item1, nullptr,
                                            ASSERT_NO_EXCEPTION);
  EXPECT_EQ(item1->GetComputedStyle(), old_style);
  EXPECT_EQ(item1->GetStyleChangeType(), kSubtreeStyleChange);
  EXPECT_TRUE(item1->GetForceReattachLayoutTree());

  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(item1->GetLayoutObject());
  EXPECT_EQ(item1->GetLayoutObject()->Parent(),
            visible_host->GetLayoutObject());
  EXPECT_NE(child1->GetComputedStyle(), old_child_style);

  // Moving into a display:none host's ShadowRoot while another element is the
  // style recalc root detaches layout and style without crashing in
  // StyleRecalcRoot::FlatTreePositionChanged.
  item2->SetInlineStyleProperty(CSSPropertyID::kColor, "red");
  hidden_host->GetShadowRoot()->moveBefore(item1, nullptr, ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(item1->GetComputedStyle());
  EXPECT_FALSE(item1->GetLayoutObject());

  // 2. Moving into a slotless shadow host detaches layout and style
  // (crbug.com/576136338, crbug.com/575200203).
  slotless_host->moveBefore(item2, nullptr, ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(item2->GetComputedStyle());
  EXPECT_FALSE(item2->GetLayoutObject());

  // 3. Moving into a <slot> that already has assigned nodes detaches layout
  // and style upon slot assignment recalc (crbug.com/576136338).
  active_slot->moveBefore(item3, nullptr, ASSERT_NO_EXCEPTION);
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
  EXPECT_FALSE(item3->GetComputedStyle());
  EXPECT_FALSE(item3->GetLayoutObject());

  // 4. Moving into an empty <slot> while a host child is pending assignment
  // detaches layout and style upon slot assignment recalc
  // (crbug.com/577332970).
  newly_slotted_host->AppendChild(
      GetDocument().CreateRawElement(html_names::kSpanTag));
  newly_active_slot->moveBefore(item4, nullptr, ASSERT_NO_EXCEPTION);
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
  EXPECT_FALSE(item4->GetComputedStyle());
  EXPECT_FALSE(item4->GetLayoutObject());

  // 5. Moving an unslotted element into a manual-slotting shadow host with a
  // <slot> detaches layout and style upon slot assignment recalc.
  manual_host->moveBefore(item5, nullptr, ASSERT_NO_EXCEPTION);
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
  EXPECT_FALSE(item5->GetComputedStyle());
  EXPECT_FALSE(item5->GetLayoutObject());

  // 6. Moving into a <slot> whose assigned node was removed in the same task
  // preserves ComputedStyle and LayoutObject instead of detaching.
  const ComputedStyle* old_item6_style = item6->GetComputedStyle();
  const LayoutObject* old_item6_layout = item6->GetLayoutObject();
  slotted_span->remove();
  active_slot->moveBefore(item6, nullptr, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(item6->GetComputedStyle(), old_item6_style);
  EXPECT_EQ(item6->GetLayoutObject(), old_item6_layout);
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
  EXPECT_EQ(item6->GetComputedStyle(), old_item6_style);
  EXPECT_EQ(item6->GetLayoutObject(), old_item6_layout);
  EXPECT_TRUE(item6->NeedsStyleRecalc());
  EXPECT_TRUE(active_slot->ChildNeedsStyleRecalc());

  UpdateAllLifecyclePhasesForTest();

  // 7. Moving a slotted child out of its shadow host, mutating style, and
  // moving it back into the same shadow host (or moving between assigned and
  // fallback positions of the same slot) in the same task invalidates style.
  const ComputedStyle* old_roundtrip_style = roundtrip_item->GetComputedStyle();
  other->moveBefore(roundtrip_item, nullptr, ASSERT_NO_EXCEPTION);
  roundtrip_item->SetInlineStyleProperty(CSSPropertyID::kColor, "red");
  roundtrip_host->moveBefore(roundtrip_item, nullptr, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(roundtrip_item->GetComputedStyle());
  EXPECT_NE(roundtrip_item->GetComputedStyle(), old_roundtrip_style);

  const ComputedStyle* assigned_style = roundtrip_item->GetComputedStyle();
  roundtrip_item->SetInlineStyleProperty(CSSPropertyID::kColor, "green");
  roundtrip_slot->moveBefore(roundtrip_item, nullptr, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(roundtrip_item->GetComputedStyle());
  EXPECT_NE(roundtrip_item->GetComputedStyle(), assigned_style);

  const ComputedStyle* fallback_style = roundtrip_item->GetComputedStyle();
  roundtrip_item->SetInlineStyleProperty(CSSPropertyID::kColor, "purple");
  roundtrip_host->moveBefore(roundtrip_item, nullptr, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(roundtrip_item->GetComputedStyle());
  EXPECT_NE(roundtrip_item->GetComputedStyle(), fallback_style);
}

TEST_F(NodeTest, moveBefore_DetachInDisplayLockedSubtree) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      .locked { content-visibility: hidden; }
    </style>
    <div id="source">
      <div id="moved_direct"><span id="moved_child">text</span></div>
      <div id="moved_nested"></div>
      <div id="moved_dirty_slot"></div>
    </div>
    <div id="source_same_frame">
      <div id="moved_same_frame"></div>
      <span>sibling</span>
    </div>
    <div id="locked" class="locked">
      <div id="nested"></div>
    </div>
    <div id="lock_later"></div>
    <div id="slot_host">
      <template shadowrootmode="open">
        <slot name="unlocked_slot"></slot>
        <div id="slot_lock" style="content-visibility: hidden">
          <slot name="locked_slot"></slot>
        </div>
      </template>
      <div id="wrapper" slot="unlocked_slot"></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* locked = GetDocument().getElementById(AtomicString("locked"));
  Element* nested = GetDocument().getElementById(AtomicString("nested"));
  Element* lock_later =
      GetDocument().getElementById(AtomicString("lock_later"));
  Element* slot_host = GetDocument().getElementById(AtomicString("slot_host"));
  Element* slot_lock =
      slot_host->GetShadowRoot()->getElementById(AtomicString("slot_lock"));
  Element* wrapper = GetDocument().getElementById(AtomicString("wrapper"));
  Element* moved_direct =
      GetDocument().getElementById(AtomicString("moved_direct"));
  Element* moved_child =
      GetDocument().getElementById(AtomicString("moved_child"));
  Element* moved_nested =
      GetDocument().getElementById(AtomicString("moved_nested"));
  Element* moved_dirty_slot =
      GetDocument().getElementById(AtomicString("moved_dirty_slot"));
  Element* moved_same_frame =
      GetDocument().getElementById(AtomicString("moved_same_frame"));

  // Style #nested while unlocked, then re-lock #locked so #nested has a
  // non-null ComputedStyle inside a locked subtree.
  locked->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(nested->GetComputedStyle());
  locked->setAttribute(html_names::kClassAttr, AtomicString("locked"));
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(locked->GetComputedStyle());
  ASSERT_TRUE(nested->GetComputedStyle());

  ASSERT_TRUE(moved_direct->GetLayoutObject());
  ASSERT_TRUE(moved_child->GetLayoutObject());
  ASSERT_TRUE(moved_nested->GetLayoutObject());
  ASSERT_TRUE(moved_dirty_slot->GetLayoutObject());
  ASSERT_TRUE(moved_same_frame->GetLayoutObject());

  // Moving into a display-locked element or its styled descendant schedules a
  // layout tree update and detaches the layout tree once slot assignments are
  // clean, marking style dirty for when the lock is later unlocked.
  locked->moveBefore(moved_direct, nullptr, ASSERT_NO_EXCEPTION);
  nested->moveBefore(moved_nested, nullptr, ASSERT_NO_EXCEPTION);

  // Moving into a wrapper whose slot assignment to a display-locked slot is
  // still dirty when moveBefore() runs.
  wrapper->setAttribute(html_names::kSlotAttr, AtomicString("locked_slot"));
  wrapper->moveBefore(moved_dirty_slot, nullptr, ASSERT_NO_EXCEPTION);

  // Moving into an unlocked container that becomes display-locked in the same
  // lifecycle update detaches during style recalc skip.
  lock_later->moveBefore(moved_same_frame, nullptr, ASSERT_NO_EXCEPTION);
  lock_later->setAttribute(html_names::kClassAttr, AtomicString("locked"));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(moved_direct->GetLayoutObject());
  EXPECT_FALSE(moved_child->GetLayoutObject());
  EXPECT_FALSE(moved_direct->GetComputedStyle());
  EXPECT_TRUE(moved_direct->NeedsStyleRecalc());
  EXPECT_FALSE(moved_nested->GetLayoutObject());
  EXPECT_FALSE(moved_nested->GetComputedStyle());
  EXPECT_TRUE(moved_nested->NeedsStyleRecalc());
  EXPECT_FALSE(wrapper->GetLayoutObject());
  EXPECT_FALSE(moved_dirty_slot->GetLayoutObject());
  EXPECT_FALSE(moved_same_frame->GetLayoutObject());
  EXPECT_TRUE(moved_same_frame->NeedsStyleRecalc());

  // Unlocking all containers restyles and reattaches all moved elements under
  // their new parents.
  locked->removeAttribute(html_names::kClassAttr);
  lock_later->removeAttribute(html_names::kClassAttr);
  slot_lock->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(moved_direct->GetLayoutObject());
  EXPECT_EQ(moved_direct->GetLayoutObject()->Parent(),
            locked->GetLayoutObject());
  EXPECT_TRUE(moved_child->GetLayoutObject());
  ASSERT_TRUE(moved_nested->GetLayoutObject());
  EXPECT_EQ(moved_nested->GetLayoutObject()->Parent(),
            nested->GetLayoutObject());
  ASSERT_TRUE(wrapper->GetLayoutObject());
  ASSERT_TRUE(moved_dirty_slot->GetLayoutObject());
  EXPECT_EQ(moved_dirty_slot->GetLayoutObject()->Parent(),
            wrapper->GetLayoutObject());
  ASSERT_TRUE(moved_same_frame->GetLayoutObject());
  EXPECT_EQ(moved_same_frame->GetLayoutObject()->Parent(),
            lock_later->GetLayoutObject());
}

TEST_F(NodeTest, moveBefore_DirtyStyleRecalcRootMovedViaDirtySlotIntoLock) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id="host">
      <template shadowrootmode="open">
        <slot id="s1" name="s1"></slot>
        <div id="lock" style="content-visibility: hidden">
          <slot id="s2" name="s2"></slot>
        </div>
      </template>
      <div id="wrapper" slot="s1"></div>
    </div>
    <div id="source">
      <div id="target"></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* host = GetDocument().getElementById(AtomicString("host"));
  Element* lock = host->GetShadowRoot()->getElementById(AtomicString("lock"));
  Element* wrapper = GetDocument().getElementById(AtomicString("wrapper"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target->GetLayoutObject());

  // Reassign #wrapper to the locked slot #s2 (dirtying slot assignment), then
  // move #target into #wrapper while #wrapper's old AssignedSlotWithoutRecalc()
  // still points to #s1.
  wrapper->setAttribute(html_names::kSlotAttr, AtomicString("s2"));
  wrapper->moveBefore(target, nullptr, ASSERT_NO_EXCEPTION);

  // Updating lifecycle phases must detach #wrapper and #target and clean up
  // StyleRecalcRoot and #s1's ChildNeedsStyleRecalc() without tripping DCHECKs.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(wrapper->GetLayoutObject());
  EXPECT_FALSE(target->GetLayoutObject());

  lock->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(wrapper->GetLayoutObject());
  EXPECT_TRUE(target->GetLayoutObject());
}

TEST_F(NodeTest, SingleStyleRecalcRootDetachedWhenMovedIntoDisplayLock) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id="host">
      <template shadowrootmode="open">
        <slot id="s1" name="s1"></slot>
        <div id="lock">
          <slot id="s2" name="s2"></slot>
        </div>
      </template>
      <div id="x" slot="s1"></div>
    </div>
    <div id="direct_lock" style="content-visibility: hidden"></div>
    <div id="y"></div>
    <div id="z"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* host = GetDocument().getElementById(AtomicString("host"));
  Element* lock = host->GetShadowRoot()->getElementById(AtomicString("lock"));
  Element* x = GetDocument().getElementById(AtomicString("x"));
  Element* y = GetDocument().getElementById(AtomicString("y"));
  Element* z = GetDocument().getElementById(AtomicString("z"));
  Element* direct_lock =
      GetDocument().getElementById(AtomicString("direct_lock"));

  // Lock #lock after #s2 already has a ComputedStyle, so moving nodes into #s2
  // will not skip marking style dirty during slot assignment recalc.
  lock->setAttribute(html_names::kStyleAttr,
                     AtomicString("content-visibility: hidden"));
  UpdateAllLifecyclePhasesForTest();

  // Case 1: Make #x the single StyleRecalcRoot, then move it into the locked
  // slot #s2 so DetachDisplayLockedFlatTreeMovedNodesIfNeeded detaches the
  // single StyleRecalcRoot.
  x->SetInlineStyleProperty(CSSPropertyID::kColor, "red");
  EXPECT_EQ(GetStyleRecalcRoot(), x);
  x->setAttribute(html_names::kSlotAttr, AtomicString("s2"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(x->GetLayoutObject());

  // Case 2: Make #y the single StyleRecalcRoot, move it into #direct_lock via
  // moveBefore(), and update style/layout tree across the document.
  y->SetInlineStyleProperty(CSSPropertyID::kColor, "red");
  EXPECT_EQ(GetStyleRecalcRoot(), y);
  direct_lock->moveBefore(y, nullptr, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(y->GetLayoutObject());

  // Case 3: Make #z the single StyleRecalcRoot, move it into #direct_lock via
  // moveBefore(), and force style update for #z (e.g. getComputedStyle(z)).
  z->SetInlineStyleProperty(CSSPropertyID::kColor, "red");
  EXPECT_EQ(GetStyleRecalcRoot(), z);
  direct_lock->moveBefore(z, nullptr, ASSERT_NO_EXCEPTION);
  GetDocument().UpdateStyleAndLayoutTreeForElement(z,
                                                   DocumentUpdateReason::kTest);
  EXPECT_TRUE(z->GetComputedStyle());
}

#if DCHECK_IS_ON()
TEST_F(NodeTest, ToStringDisallowedAssignmentRecalc) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id=host>
      <template shadowrootmode=open>
        <div contenteditable>
          <slot name=slot></slot>
        </div>
      </template>
      <div id=element>element</div>
    </div>
  )HTML");

  Element* element = GetElementById("element");
  element->setAttribute(html_names::kSlotAttr, AtomicString("slot"));

  SlotAssignmentRecalcForbiddenScope forbidden_scope(GetDocument());
  element->ToString();
}
#endif

// Note: This test does not include view transition pseudos due to missing setup
// in NodeTest. For view transition pseudo traversal, see
// ViewTransitionTest.IncludingPseudoTraversal in view_transition_test.cc.
TEST_F(NodeTest, PseudoAwareSiblingTraversalAllPseudos) {
  // Overall tree order: marker, before, child, after, interest_button
  // But we can only observe ::marker on a list item, not a button, so
  // we do this in two phases:
  // host 1: before, child, after, interest_button
  // host 2: marker, before, child, after

  SetBodyContent(R"HTML(
    <style>
      .host::before { content: 'before'; }
      .host::after { content: 'after'; }
      .host::interest-button { content: 'interest'; }
      .host::marker { content: 'marker'; }
    </style>
    <button id=host1 class=host interestfor=target><span id="child">child</span></button>
    <div popover id="target">Target</div>
    <li id=host2 class=host><span id="child2">child2</span></li>
  )HTML");

  Element* host1 = GetElementById("host1");
  ASSERT_TRUE(host1);
  Element* host2 = GetElementById("host2");
  ASSERT_TRUE(host2);
  UpdateAllLifecyclePhasesForTest();

  PseudoElement* before = host1->GetPseudoElement(kPseudoIdBefore);
  Element* child = host1->QuerySelector(AtomicString("#child"));
  PseudoElement* after = host1->GetPseudoElement(kPseudoIdAfter);
  PseudoElement* interest_button =
      host1->GetPseudoElement(kPseudoIdInterestButton);

  ASSERT_TRUE(before);
  ASSERT_TRUE(child);
  ASSERT_TRUE(after);
  ASSERT_TRUE(interest_button);

  // Phase 1: host 1 (before, child, after, interest_button)
  EXPECT_EQ(host1->PseudoAwareFirstChild(), before);
  EXPECT_EQ(host1->PseudoAwareLastChild(), interest_button);

  EXPECT_EQ(before->PseudoAwareNextSibling(), child);
  EXPECT_EQ(child->PseudoAwareNextSibling(), after);
  EXPECT_EQ(after->PseudoAwareNextSibling(), interest_button);
  EXPECT_FALSE(interest_button->PseudoAwareNextSibling());

  EXPECT_FALSE(before->PseudoAwarePreviousSibling());
  EXPECT_EQ(child->PseudoAwarePreviousSibling(), before);
  EXPECT_EQ(after->PseudoAwarePreviousSibling(), child);
  EXPECT_EQ(interest_button->PseudoAwarePreviousSibling(), after);

  // Phase 2: host 2 (marker, before, child, after)
  PseudoElement* marker = host2->GetPseudoElement(kPseudoIdMarker);
  PseudoElement* before2 = host2->GetPseudoElement(kPseudoIdBefore);
  Element* child2 = host2->QuerySelector(AtomicString("#child2"));
  PseudoElement* after2 = host2->GetPseudoElement(kPseudoIdAfter);

  ASSERT_TRUE(marker);
  ASSERT_TRUE(before2);
  ASSERT_TRUE(child2);
  ASSERT_TRUE(after2);

  EXPECT_EQ(host2->PseudoAwareFirstChild(), marker);
  EXPECT_EQ(host2->PseudoAwareLastChild(), after2);

  EXPECT_EQ(marker->PseudoAwareNextSibling(), before2);
  EXPECT_EQ(before2->PseudoAwareNextSibling(), child2);
  EXPECT_EQ(child2->PseudoAwareNextSibling(), after2);
  EXPECT_FALSE(after2->PseudoAwareNextSibling());

  EXPECT_FALSE(marker->PseudoAwarePreviousSibling());
  EXPECT_EQ(before2->PseudoAwarePreviousSibling(), marker);
  EXPECT_EQ(child2->PseudoAwarePreviousSibling(), before2);
  EXPECT_EQ(after2->PseudoAwarePreviousSibling(), child2);
}

}  // namespace blink
