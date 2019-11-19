// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Copy elements in HTMLCollection into a vector because HTMLCollection is a
// live list and would be invalid if a mutation occurs.
HeapVector<Member<Element>> CollectFromHTMLCollection(
    HTMLCollection& collection) {
  HeapVector<Member<Element>> elements;
  for (Element* element : collection)
    elements.push_back(element);
  return elements;
}

template <class T>
HeapVector<Member<Node>> CollectFromIterable(T iterable) {
  HeapVector<Member<Node>> nodes;
  for (auto& node : iterable)
    nodes.push_back(&node);
  return nodes;
}

// Process Mini-DSL for Declarative Shadow DOM.

// In Mini-DSL, <shadowroot> elements would behaves as if it were "Declarative
// Shadow DOM". This is usuful for constructing a composed tree in testing.
// Declarative Shadow DOM is under the dicussion. See
// https://github.com/whatwg/dom/issues/510. Once Declarative Shadow DOM is
// standardized and implemented, this DSL is no longer required.
void ConvertDeclarativeShadowDOMToShadowRoot(Element& element) {
  HTMLCollection* shadow_root_collection =
      element.getElementsByTagName("shadowroot");
  if (!shadow_root_collection)
    return;
  for (Element* shadow_root :
       CollectFromHTMLCollection(*shadow_root_collection)) {
    Element* parent = shadow_root->parentElement();
    DCHECK(parent);
    parent->removeChild(shadow_root);
    ShadowRoot& attached_shadow_root =
        parent->AttachShadowRootInternal(ShadowRootType::kOpen);
    for (Node* child :
         CollectFromIterable(NodeTraversal::ChildrenOf(*shadow_root)))
      attached_shadow_root.AppendChild(child);
  }
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

  Persistent<Document> document_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void SlotAssignmentTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  document_ = &dummy_page_holder_->GetDocument();
  DCHECK(document_);
}

void SlotAssignmentTest::SetBody(const char* html) {
  Element* body = GetDocument().body();
  body->SetInnerHTMLFromString(String::FromUTF8(html));
  ConvertDeclarativeShadowDOMToShadowRoot(*body);
  RemoveWhiteSpaceOnlyTextNode(*body);
}

TEST_F(SlotAssignmentTest, DeclarativeShadowDOM) {
  SetBody(R"HTML(
    <div id=host>
      <shadowroot></shadowroot>
    </div>
  )HTML");

  Element* host = GetDocument().QuerySelector("#host");
  ASSERT_NE(nullptr, host);
  ASSERT_NE(nullptr, host->OpenShadowRoot());
}

TEST_F(SlotAssignmentTest, NestedDeclarativeShadowDOM) {
  SetBody(R"HTML(
    <div id=host1>
      <shadowroot>
        <div id=host2>
          <shadowroot></shadowroot>
        </div>
      </shadowroot>
    </div>
  )HTML");

  Element* host1 = GetDocument().QuerySelector("#host1");
  ASSERT_NE(nullptr, host1);
  ShadowRoot* shadow_root1 = host1->OpenShadowRoot();
  ASSERT_NE(nullptr, shadow_root1);

  Element* host2 = shadow_root1->QuerySelector("#host2");
  ASSERT_NE(nullptr, host2);
  ShadowRoot* shadow_root2 = host2->OpenShadowRoot();
  ASSERT_NE(nullptr, shadow_root2);
}

TEST_F(SlotAssignmentTest, AssignedNodesAreSet) {
  SetBody(R"HTML(
    <div id=host>
      <shadowroot>
        <slot></slot>
      </shadowroot>
      <div id='host-child'></div>
    </div>
  )HTML");

  Element* host = GetDocument().QuerySelector("#host");
  Element* host_child = GetDocument().QuerySelector("#host-child");
  ShadowRoot* shadow_root = host->OpenShadowRoot();
  auto* slot = DynamicTo<HTMLSlotElement>(shadow_root->QuerySelector("slot"));
  ASSERT_NE(nullptr, slot);

  EXPECT_EQ(slot, host_child->AssignedSlot());
  HeapVector<Member<Node>> expected_nodes;
  expected_nodes.push_back(host_child);
  EXPECT_EQ(expected_nodes, slot->AssignedNodes());
}

}  // namespace blink
