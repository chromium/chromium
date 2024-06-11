// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
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
  body->setHTMLUnsafe(String::FromUTF8(html));
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

}  // namespace blink
