// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

using DOMNodeIdsTest = EditingTestBase;

TEST_F(DOMNodeIdsTest, NonNull) {
  SetBodyContent("<div id='a'></div><div id='b'></div>");
  Node* a = GetDocument().getElementById(AtomicString("a"));
  Node* b = GetDocument().getElementById(AtomicString("b"));

  DOMNodeId id_a = a->GetDomNodeId();
  EXPECT_NE(kInvalidDOMNodeId, id_a);
  EXPECT_EQ(id_a, a->GetDomNodeId());
  EXPECT_EQ(a, DOMNodeIds::NodeForId(id_a));

  DOMNodeId id_b = b->GetDomNodeId();
  EXPECT_NE(kInvalidDOMNodeId, id_b);
  EXPECT_NE(id_a, id_b);
  EXPECT_EQ(id_b, b->GetDomNodeId());
  EXPECT_EQ(b, DOMNodeIds::NodeForId(id_b));

  EXPECT_EQ(id_a, a->GetDomNodeId());
  EXPECT_EQ(a, DOMNodeIds::NodeForId(id_a));
}

TEST_F(DOMNodeIdsTest, DeletedNode) {
  SetBodyContent("<div id='a'></div>");
  Node* a = GetDocument().getElementById(AtomicString("a"));
  DOMNodeId id_a = a->GetDomNodeId();

  a->remove();
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_EQ(nullptr, DOMNodeIds::NodeForId(id_a));
}

TEST_F(DOMNodeIdsTest, UnusedID) {
  SetBodyContent("<div id='a'></div>");
  Node* a = GetDocument().getElementById(AtomicString("a"));
  DOMNodeId id_a = a->GetDomNodeId();
  EXPECT_EQ(nullptr, DOMNodeIds::NodeForId(id_a + 1));
}

TEST_F(DOMNodeIdsTest, Null) {
  EXPECT_EQ(kInvalidDOMNodeId, DOMNodeIds::IdForNode(nullptr));
  EXPECT_EQ(nullptr, DOMNodeIds::NodeForId(kInvalidDOMNodeId));
}

TEST_F(DOMNodeIdsTest, ExistingIdForNode) {
  SetBodyContent("<div id='a'></div>");
  Node* a = GetDocument().getElementById(AtomicString("a"));

  // Node a does not yet have an ID.
  EXPECT_EQ(kInvalidDOMNodeId, DOMNodeIds::ExistingIdForNode(a));

  // IdForNode() forces node a to have an ID.
  DOMNodeId id_a = a->GetDomNodeId();
  EXPECT_NE(kInvalidDOMNodeId, id_a);

  // Both ExistingIdForNode() and IdForNode() still return the same ID.
  EXPECT_EQ(id_a, DOMNodeIds::ExistingIdForNode(a));
  EXPECT_EQ(id_a, a->GetDomNodeId());
}

}  // namespace blink
