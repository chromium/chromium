// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/node_rare_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/node-inl.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

class NodeRareDataTest : public PageTestBase {
 protected:
  using FieldId = NodeRareData::FieldId;
  static constexpr unsigned kMinimumVectorSize =
      NodeRareData::kMinimumVectorSize;

  wtf_size_t GetSize(const NodeRareData* data) const { return data->size(); }
  bool HasField(const NodeRareData* data, FieldId id) const {
    return data->HasField(id);
  }
  NodeRareDataField* GetField(const NodeRareData* data, FieldId id) const {
    return data->GetField(id);
  }
  void SetFieldToNullIfExists(NodeRareData* data, FieldId id) {
    data->SetFieldToNullIfExists(id);
  }

  class TestElement : public HTMLDivElement {
   public:
    explicit TestElement(Document& document) : HTMLDivElement(document) {}
    using Node::EnsureRareData;
    using Node::RareData;
  };
};

TEST_F(NodeRareDataTest, BasicOperations) {
  TestElement* node = MakeGarbageCollected<TestElement>(GetDocument());
  NodeRareData* rare_data = &node->EnsureRareData();
  ASSERT_NE(nullptr, rare_data);
  EXPECT_EQ(0u, GetSize(rare_data));

  auto get_node_id = [&rare_data]() {
    return static_cast<const NodeRareData*>(rare_data)->NodeId();
  };

  // Initially empty.
  EXPECT_EQ(0, get_node_id());
  EXPECT_TRUE(rare_data->GetNonce().IsNull());

  // Set NodeId (wrapped int32_t).
  NodeRareData* prev_ptr = rare_data;
  int32_t& node_id_ref = rare_data->NodeId().RefreshNodeAndUnwrap(*node);
  rare_data = node->RareData();
  // Initial capacity is kMinimumVectorSize (2), so setting first field should
  // not reallocate.
  EXPECT_EQ(prev_ptr, rare_data);
  node_id_ref = 42;
  EXPECT_EQ(42, get_node_id());
  EXPECT_EQ(1u, GetSize(rare_data));

  // Set Nonce (wrapped AtomicString).
  prev_ptr = rare_data;
  rare_data->SetNonce(AtomicString("test-nonce")).RefreshNode(*node);
  rare_data = node->RareData();
  // Size reaches kMinimumVectorSize, still within capacity.
  EXPECT_EQ(prev_ptr, rare_data);
  EXPECT_EQ(AtomicString("test-nonce"), rare_data->GetNonce());
  EXPECT_EQ(42, get_node_id());
  EXPECT_EQ(kMinimumVectorSize, GetSize(rare_data));

  // Set IsValue (wrapped AtomicString). This should trigger reallocation
  // as the new size (3) will be greater than the capacity (kMinimumVectorSize =
  // 2).
  prev_ptr = rare_data;
  rare_data->SetIsValue(AtomicString("test-is-value")).RefreshNode(*node);
  rare_data = node->RareData();
  EXPECT_NE(prev_ptr, rare_data);  // Reallocation expected!
  EXPECT_EQ(AtomicString("test-is-value"), rare_data->IsValue());
  EXPECT_EQ(AtomicString("test-nonce"), rare_data->GetNonce());
  EXPECT_EQ(42, get_node_id());
  EXPECT_EQ(3u, GetSize(rare_data));

  // Set SavedLayerScrollOffset (wrapped ScrollOffset/gfx::Vector2dF).
  prev_ptr = rare_data;
  rare_data->SetSavedLayerScrollOffset(gfx::Vector2dF(1.0f, 2.0f))
      .RefreshNode(*node);
  rare_data = node->RareData();
  // Size reaches kMinimumVectorSize * 2 (4), still within capacity.
  EXPECT_EQ(prev_ptr, rare_data);
  EXPECT_EQ(gfx::Vector2dF(1.0f, 2.0f), rare_data->SavedLayerScrollOffset());
  EXPECT_EQ(AtomicString("test-is-value"), rare_data->IsValue());
  EXPECT_EQ(AtomicString("test-nonce"), rare_data->GetNonce());
  EXPECT_EQ(42, get_node_id());
  EXPECT_EQ(kMinimumVectorSize * 2, GetSize(rare_data));

  // Set LastSentUnboundedBounds (wrapped gfx::Rect). This should trigger
  // reallocation as the new size (5) will be greater than the capacity
  // (kMinimumVectorSize * 2 = 4).
  prev_ptr = rare_data;
  rare_data->SetLastSentUnboundedBounds(gfx::Rect(10, 20, 30, 40))
      .RefreshNode(*node);
  rare_data = node->RareData();
  EXPECT_NE(prev_ptr, rare_data);  // Reallocation expected!
  EXPECT_EQ(gfx::Rect(10, 20, 30, 40), rare_data->LastSentUnboundedBounds());
  EXPECT_EQ(gfx::Vector2dF(1.0f, 2.0f), rare_data->SavedLayerScrollOffset());
  EXPECT_EQ(AtomicString("test-is-value"), rare_data->IsValue());
  EXPECT_EQ(AtomicString("test-nonce"), rare_data->GetNonce());
  EXPECT_EQ(42, get_node_id());
  EXPECT_EQ(5u, GetSize(rare_data));
}

TEST_F(NodeRareDataTest, OverwriteAndNullify) {
  TestElement* node = MakeGarbageCollected<TestElement>(GetDocument());
  NodeRareData* rare_data = &node->EnsureRareData();

  // Set and overwrite Nonce.
  rare_data->SetNonce(AtomicString("nonce1")).RefreshNode(*node);
  rare_data = node->RareData();
  EXPECT_EQ(AtomicString("nonce1"), rare_data->GetNonce());
  EXPECT_EQ(1u, GetSize(rare_data));

  NodeRareData* prev_ptr = rare_data;
  rare_data->SetNonce(AtomicString("nonce2")).RefreshNode(*node);
  rare_data = node->RareData();
  EXPECT_EQ(prev_ptr, rare_data);
  EXPECT_EQ(AtomicString("nonce2"), rare_data->GetNonce());
  EXPECT_EQ(1u, GetSize(rare_data));

  rare_data->SetNonce(g_null_atom).RefreshNode(*node);
  rare_data = node->RareData();
  EXPECT_TRUE(rare_data->GetNonce().IsNull());
  EXPECT_EQ(1u, GetSize(rare_data));
}

TEST_F(NodeRareDataTest, SetFieldToNullIfExists) {
  TestElement* node = MakeGarbageCollected<TestElement>(GetDocument());
  NodeRareData* rare_data = &node->EnsureRareData();

  // Initially HasField is false.
  EXPECT_FALSE(HasField(rare_data, FieldId::kNodeLists));
  EXPECT_EQ(nullptr, GetField(rare_data, FieldId::kNodeLists));

  // Ensure it (creates it).
  NodeListsNodeData& node_lists =
      rare_data->EnsureNodeLists().RefreshNodeAndUnwrap(*node);
  rare_data = node->RareData();
  EXPECT_EQ(&node_lists, rare_data->NodeLists());
  EXPECT_NE(nullptr, rare_data->NodeLists());
  EXPECT_TRUE(HasField(rare_data, FieldId::kNodeLists));
  EXPECT_NE(nullptr, GetField(rare_data, FieldId::kNodeLists));
  EXPECT_EQ(1u, GetSize(rare_data));

  SetFieldToNullIfExists(rare_data, FieldId::kNodeLists);

  EXPECT_EQ(nullptr, rare_data->NodeLists());

  // But HasField should still be true, and size should still be 1, because we
  // don't remove the slot from the vector. This avoids expensive shifting of
  // subsequent elements and allows fast reuse of the slot.
  EXPECT_TRUE(HasField(rare_data, FieldId::kNodeLists));
  EXPECT_EQ(nullptr, GetField(rare_data, FieldId::kNodeLists));
  EXPECT_EQ(1u, GetSize(rare_data));
}

TEST_F(NodeRareDataTest, PseudoElementReallocation) {
  TestElement* node = MakeGarbageCollected<TestElement>(GetDocument());
  NodeRareData* rare_data = &node->EnsureRareData();
  EXPECT_EQ(0u, GetSize(rare_data));

  // Create a parent element and a pseudo element.
  Element* parent = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  PseudoElement* pseudo = PseudoElement::Create(parent, kPseudoIdBefore);
  ASSERT_NE(nullptr, pseudo);

  // Set PseudoElement (size 1).
  rare_data->SetPseudoElement(kPseudoIdBefore, pseudo).RefreshNode(*node);
  rare_data = node->RareData();
  EXPECT_TRUE(rare_data->HasAnyPseudos());
  EXPECT_EQ(pseudo, rare_data->GetPseudoElement(kPseudoIdBefore));
  EXPECT_EQ(1u, GetSize(rare_data));

  // Trigger reallocation by adding two other fields.
  // Add 1st extra field: Nonce (size reaches kMinimumVectorSize, no realloc).
  NodeRareData* prev_ptr = rare_data;
  rare_data->SetNonce(AtomicString("nonce")).RefreshNode(*node);
  rare_data = node->RareData();
  EXPECT_EQ(prev_ptr, rare_data);
  EXPECT_EQ(kMinimumVectorSize, GetSize(rare_data));

  // Add 2nd extra field: IsValue (size 3 > kMinimumVectorSize, reallocation
  // expected).
  rare_data->SetIsValue(AtomicString("is-value")).RefreshNode(*node);
  rare_data = node->RareData();
  EXPECT_NE(prev_ptr, rare_data);
  EXPECT_EQ(3u, GetSize(rare_data));

  // Verify all fields are preserved.
  EXPECT_TRUE(rare_data->HasAnyPseudos());
  EXPECT_EQ(pseudo, rare_data->GetPseudoElement(kPseudoIdBefore));
  EXPECT_EQ(AtomicString("nonce"), rare_data->GetNonce());
  EXPECT_EQ(AtomicString("is-value"), rare_data->IsValue());

  // Clean up.
  rare_data->ClearPseudoElements();
  EXPECT_FALSE(rare_data->HasAnyPseudos());
  // ClearPseudoElements calls SetFieldToNullIfExists, so size is still 3
  // and kPseudoElementData is still in bitfield but points to nullptr.
  EXPECT_EQ(3u, GetSize(rare_data));
  EXPECT_TRUE(HasField(rare_data, FieldId::kPseudoElementData));
  EXPECT_EQ(nullptr, GetField(rare_data, FieldId::kPseudoElementData));

  // Force GC to verify that the old rare_data (which was reallocated) is
  // destroyed without triggering the
  // `DCHECK(!GetField(FieldId::kPseudoElementData))` in its destructor.
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
}

}  // namespace blink
