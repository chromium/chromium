// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry_assignment.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

template <class T>
HeapVector<Member<Node>> CollectFromIterable(T iterable) {
  HeapVector<Member<Node>> nodes;
  for (auto& node : iterable)
    nodes.push_back(&node);
  return nodes;
}

void RemoveWhiteSpaceOnlyTextNode(ContainerNode& container) {
  for (Node* descendant :
       CollectFromIterable(NodeTraversal::InclusiveDescendantsOf(container))) {
    if (auto* text = DynamicTo<Text>(descendant)) {
      if (text->ContainsOnlyWhitespaceOrEmpty())
        text->remove();
    } else if (auto* element = DynamicTo<Element>(descendant)) {
      if (ShadowRoot* shadow_root = element->OpenShadowRoot())
        RemoveWhiteSpaceOnlyTextNode(*shadow_root);
    }
  }
}

}  // namespace

class SlotAssignmentTest : public testing::Test {
 public:
  SlotAssignmentTest() {}

 protected:
  Document& GetDocument() const { return *document_; }
  void SetBody(const char* html);

 private:
  void SetUp() override;

  test::TaskEnvironment task_environment_;
  Persistent<Document> document_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void SlotAssignmentTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  document_ = &dummy_page_holder_->GetDocument();
  DCHECK(document_);
}

void SlotAssignmentTest::SetBody(const char* html) {
  Element* body = GetDocument().body();
  body->SetHTMLUnsafeWithoutTrustedTypes(String::FromUtf8(html));
  RemoveWhiteSpaceOnlyTextNode(*body);
}

TEST_F(SlotAssignmentTest, DeclarativeShadowDOM) {
  SetBody(R"HTML(
    <div id=host>
      <template shadowrootmode=open></template>
    </div>
  )HTML");

  Element* host = GetDocument().QuerySelector(AtomicString("#host"));
  ASSERT_NE(nullptr, host);
  ASSERT_NE(nullptr, host->OpenShadowRoot());
}

TEST_F(SlotAssignmentTest, NestedDeclarativeShadowDOM) {
  SetBody(R"HTML(
    <div id=host1>
      <template shadowrootmode=open>
        <div id=host2>
          <template shadowrootmode=open></template>
        </div>
      </template>
    </div>
  )HTML");

  Element* host1 = GetDocument().QuerySelector(AtomicString("#host1"));
  ASSERT_NE(nullptr, host1);
  ShadowRoot* shadow_root1 = host1->OpenShadowRoot();
  ASSERT_NE(nullptr, shadow_root1);

  Element* host2 = shadow_root1->QuerySelector(AtomicString("#host2"));
  ASSERT_NE(nullptr, host2);
  ShadowRoot* shadow_root2 = host2->OpenShadowRoot();
  ASSERT_NE(nullptr, shadow_root2);
}

TEST_F(SlotAssignmentTest, AssignedNodesAreSet) {
  SetBody(R"HTML(
    <div id=host>
      <template shadowrootmode=open>
        <slot></slot>
      </template>
      <div id='host-child'></div>
    </div>
  )HTML");

  Element* host = GetDocument().QuerySelector(AtomicString("#host"));
  Element* host_child =
      GetDocument().QuerySelector(AtomicString("#host-child"));
  ShadowRoot* shadow_root = host->OpenShadowRoot();
  auto* slot = DynamicTo<HTMLSlotElement>(
      shadow_root->QuerySelector(AtomicString("slot")));
  ASSERT_NE(nullptr, slot);

  EXPECT_EQ(slot, host_child->AssignedSlot());
  HeapVector<Member<Node>> expected_nodes;
  expected_nodes.push_back(host_child);
  EXPECT_EQ(expected_nodes, slot->AssignedNodes());
}

TEST_F(SlotAssignmentTest, ScheduleVisualUpdate) {
  SetBody(R"HTML(
    <div id="host">
      <template shadowrootmode=open>
        <slot></slot>
      </template>
      <div></div>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  auto* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().getElementById(AtomicString("host"))->appendChild(div);
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            GetDocument().Lifecycle().GetState());
}

// Regression test for crbug.com/520167277.
// Verifies that slot assignment recalculation works safely when triggered
// during layout tree rebuild of slot children.
TEST_F(SlotAssignmentTest, RebuildDistributedChildrenDuringRecalc) {
  SetBody(R"HTML(
    <div id="host">
      <template shadowrootmode="open">
        <slot></slot>
      </template>
      <div id="child1"></div>
      <div id="child2"></div>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  Element* host = GetDocument().QuerySelector(AtomicString("#host"));
  ASSERT_NE(nullptr, host);
  ShadowRoot* shadow_root = host->GetShadowRoot();
  ASSERT_NE(nullptr, shadow_root);
  Element* child1 = GetDocument().QuerySelector(AtomicString("#child1"));
  Element* child2 = GetDocument().QuerySelector(AtomicString("#child2"));
  ASSERT_NE(nullptr, child1);
  ASSERT_NE(nullptr, child2);

  // Force layout tree rebuild for children and mark slot assignment dirty.
  child1->SetForceReattachLayoutTree();
  child2->SetForceReattachLayoutTree();
  shadow_root->SetNeedsAssignmentRecalc();

  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(shadow_root->NeedsSlotAssignmentRecalc());
}

// Regression test for crbug.com/576736463 and crbug.com/575533210.
// Verifies that moving a node with an author manual slot assignment into a UA
// manual-slotting host (<details>) does not overwrite the node's author manual
// slot assignment or dirty the author shadow root during
// SlotAssignmentEngine::RecalcSlotAssignments().
TEST_F(SlotAssignmentTest,
       UserAgentManualSlottingPreservesAuthorManualAssignment) {
  SetBody(R"HTML(
    <div id="host">
      <div id="c1"></div>
      <div id="n"></div>
    </div>
    <div id="parking" style="display:none"></div>
    <details id="details" open>
      <summary>Summary</summary>
    </details>
  )HTML");

  Element* host = GetDocument().getElementById(AtomicString("host"));
  Element* c1 = GetDocument().getElementById(AtomicString("c1"));
  Element* n = GetDocument().getElementById(AtomicString("n"));
  Element* parking = GetDocument().getElementById(AtomicString("parking"));
  Element* details = GetDocument().getElementById(AtomicString("details"));
  ASSERT_NE(nullptr, host);
  ASSERT_NE(nullptr, c1);
  ASSERT_NE(nullptr, n);
  ASSERT_NE(nullptr, parking);
  ASSERT_NE(nullptr, details);

  ShadowRoot& host_shadow_root = host->AttachShadowRootInternal(
      ShadowRootMode::kOpen, FocusDelegation::kNone,
      SlotAssignmentMode::kManual, CustomElementRegistryAssignment::Inherit(),
      /*serializable=*/false, /*clonable=*/false,
      /*reference_target=*/g_null_atom);
  host_shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<div style='content-visibility: hidden'><slot id='s1'></slot></div>"
      "<slot id='s2'></slot>");
  auto* s1 =
      To<HTMLSlotElement>(host_shadow_root.getElementById(AtomicString("s1")));
  auto* s2 =
      To<HTMLSlotElement>(host_shadow_root.getElementById(AtomicString("s2")));
  ASSERT_NE(nullptr, s1);
  ASSERT_NE(nullptr, s2);

  s1->Assign(c1);
  s2->Assign(n);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(s2, n->AssignedSlot());

  // Move `n` out of `host` into `#parking` and update lifecycle so
  // `host_shadow_root` is clean while `n` retains its manual assignment to
  // `s2`.
  parking->appendChild(n);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(host_shadow_root.NeedsSlotAssignmentRecalc());
  EXPECT_TRUE(s2->ManuallyAssignedNodes().Contains(n));

  // Move `n` from `#parking` into `<details>`. Only `<details>`'s UA shadow
  // root needs slot assignment recalc.
  ShadowRoot* details_shadow_root = details->UserAgentShadowRoot();
  ASSERT_NE(nullptr, details_shadow_root);
  details->appendChild(n);
  EXPECT_FALSE(host_shadow_root.NeedsSlotAssignmentRecalc());
  EXPECT_TRUE(details_shadow_root->NeedsSlotAssignmentRecalc());

  // Recalculating `<details>`'s UA slot assignment must not steal `n`'s author
  // manual slot assignment from `s2` or dirty `host_shadow_root`.
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
  EXPECT_FALSE(host_shadow_root.NeedsSlotAssignmentRecalc());
  EXPECT_FALSE(details_shadow_root->NeedsSlotAssignmentRecalc());
  EXPECT_FALSE(GetDocument().IsSlotAssignmentDirty());
  EXPECT_TRUE(s2->ManuallyAssignedNodes().Contains(n));
  EXPECT_EQ(s2, n->ManuallyAssignedSlot());
  ASSERT_NE(nullptr, n->AssignedSlot());
  EXPECT_EQ(details_shadow_root, n->AssignedSlot()->ContainingShadowRoot());

  // Full lifecycle update (including PerformLayout and
  // PropagateFrameRectsRecursively) must complete with clean slot assignments.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Moving `n` back to `host` restores its flat-tree assignment to `s2`.
  host->appendChild(n);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(s2, n->AssignedSlot());
}

TEST_F(SlotAssignmentTest, AuthorManualAssignOfUserAgentSlottedNode) {
  SetBody(R"HTML(
    <div id="host">
      <div id="c1"></div>
    </div>
    <details id="details" open>
      <div id="details_child"></div>
    </details>
  )HTML");

  Element* host = GetDocument().getElementById(AtomicString("host"));
  Element* c1 = GetDocument().getElementById(AtomicString("c1"));
  Element* details = GetDocument().getElementById(AtomicString("details"));
  Element* details_child =
      GetDocument().getElementById(AtomicString("details_child"));
  ASSERT_NE(nullptr, host);
  ASSERT_NE(nullptr, c1);
  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details_child);

  ShadowRoot& host_shadow_root = host->AttachShadowRootInternal(
      ShadowRootMode::kOpen, FocusDelegation::kNone,
      SlotAssignmentMode::kManual, CustomElementRegistryAssignment::Inherit(),
      /*serializable=*/false, /*clonable=*/false,
      /*reference_target=*/g_null_atom);
  host_shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<div style='content-visibility: hidden'><slot id='s1'></slot></div>");
  auto* s1 =
      To<HTMLSlotElement>(host_shadow_root.getElementById(AtomicString("s1")));
  ASSERT_NE(nullptr, s1);

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  ShadowRoot* details_shadow_root = details->UserAgentShadowRoot();
  ASSERT_NE(nullptr, details_shadow_root);
  ASSERT_NE(nullptr, details_child->AssignedSlot());
  EXPECT_EQ(details_shadow_root,
            details_child->AssignedSlot()->ContainingShadowRoot());

  HeapVector<Member<Node>> assigned;
  assigned.push_back(c1);
  assigned.push_back(details_child);
  s1->Assign(assigned);

  // Assigning `details_child` to an author slot while it is a child of
  // `<details>` must not dirty `<details>`'s UA shadow root or remove
  // `details_child` from `<details>`'s UA slot.
  EXPECT_FALSE(details_shadow_root->NeedsSlotAssignmentRecalc());
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
  EXPECT_FALSE(host_shadow_root.NeedsSlotAssignmentRecalc());
  EXPECT_FALSE(details_shadow_root->NeedsSlotAssignmentRecalc());
  EXPECT_FALSE(GetDocument().IsSlotAssignmentDirty());
  ASSERT_NE(nullptr, details_child->AssignedSlot());
  EXPECT_EQ(details_shadow_root,
            details_child->AssignedSlot()->ContainingShadowRoot());
}

TEST_F(SlotAssignmentTest, RemoveManualSlotWithAssignedNodes) {
  SetBody(R"HTML(
    <div id="host">
      <div id="c1">Hello</div>
    </div>
  )HTML");

  Element* host = GetDocument().getElementById(AtomicString("host"));
  Element* c1 = GetDocument().getElementById(AtomicString("c1"));
  ASSERT_NE(nullptr, host);
  ASSERT_NE(nullptr, c1);

  ShadowRoot& shadow_root = host->AttachShadowRootInternal(
      ShadowRootMode::kOpen, FocusDelegation::kNone,
      SlotAssignmentMode::kManual, CustomElementRegistryAssignment::Inherit(),
      /*serializable=*/false, /*clonable=*/false,
      /*reference_target=*/g_null_atom);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<slot id='s1'></slot><slot id='s2'></slot>");
  auto* s1 =
      To<HTMLSlotElement>(shadow_root.getElementById(AtomicString("s1")));
  ASSERT_NE(nullptr, s1);

  s1->Assign(c1);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(s1, c1->AssignedSlot());
  EXPECT_NE(nullptr, c1->GetLayoutObject());

  s1->remove();
  EXPECT_TRUE(shadow_root.NeedsSlotAssignmentRecalc());

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(shadow_root.NeedsSlotAssignmentRecalc());
  EXPECT_EQ(nullptr, c1->AssignedSlot());
  EXPECT_EQ(nullptr, c1->GetLayoutObject());
}

}  // namespace blink
