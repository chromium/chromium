// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

using DOMNodeIdsTest = EditingTestBase;

TEST_F(DOMNodeIdsTest, NonNull) {
  SetBodyContent("<div id='a'></div><div id='b'></div>");
  Node* a = GetDocument().getElementById("a");
  Node* b = GetDocument().getElementById("b");

  DOMNodeId id_a = DOMNodeIds::IdForNode(a);
  EXPECT_NE(kInvalidDOMNodeId, id_a);
  EXPECT_EQ(id_a, DOMNodeIds::IdForNode(a));
  EXPECT_EQ(a, DOMNodeIds::NodeForId(id_a));

  DOMNodeId id_b = DOMNodeIds::IdForNode(b);
  EXPECT_NE(kInvalidDOMNodeId, id_b);
  EXPECT_NE(id_a, id_b);
  EXPECT_EQ(id_b, DOMNodeIds::IdForNode(b));
  EXPECT_EQ(b, DOMNodeIds::NodeForId(id_b));

  EXPECT_EQ(id_a, DOMNodeIds::IdForNode(a));
  EXPECT_EQ(a, DOMNodeIds::NodeForId(id_a));
}

TEST_F(DOMNodeIdsTest, DeletedNode) {
  SetBodyContent("<div id='a'></div>");
  Node* a = GetDocument().getElementById("a");
  DOMNodeId id_a = DOMNodeIds::IdForNode(a);

  a->remove();
  ThreadState::Current()->CollectAllGarbageForTesting(
      BlinkGC::kNoHeapPointersOnStack);
  EXPECT_EQ(nullptr, DOMNodeIds::NodeForId(id_a));
}

TEST_F(DOMNodeIdsTest, UnusedID) {
  SetBodyContent("<div id='a'></div>");
  Node* a = GetDocument().getElementById("a");
  DOMNodeId id_a = DOMNodeIds::IdForNode(a);
  EXPECT_EQ(nullptr, DOMNodeIds::NodeForId(id_a + 1));
}

TEST_F(DOMNodeIdsTest, Null) {
  EXPECT_EQ(kInvalidDOMNodeId, DOMNodeIds::IdForNode(nullptr));
  EXPECT_EQ(nullptr, DOMNodeIds::NodeForId(kInvalidDOMNodeId));
}

}  // namespace blink
