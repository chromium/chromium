// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/live_node_list_registry.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/name_node_list.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {
namespace {

class LiveNodeListRegistryTest : public PageTestBase {
 public:
  void SetUp() override { PageTestBase::SetUp(gfx::Size()); }

 protected:
  const LiveNodeListBase* CreateNodeList() {
    return MakeGarbageCollected<NameNodeList>(GetDocument(), kNameNodeListType,
                                              g_empty_atom);
  }
};

TEST_F(LiveNodeListRegistryTest, InitialState) {
  LiveNodeListRegistry registry;
  EXPECT_TRUE(registry.IsEmpty());
  EXPECT_FALSE(registry.ContainsInvalidationType(kInvalidateOnNameAttrChange));
}

// The invalidation types which match should be updated as elements are added.
TEST_F(LiveNodeListRegistryTest, Add) {
  LiveNodeListRegistry registry;
  const auto* a = CreateNodeList();
  const auto* b = CreateNodeList();

  // Addition of a single node list with a single invalidation type.
  registry.Add(a, kInvalidateOnNameAttrChange);
  EXPECT_FALSE(registry.IsEmpty());
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnNameAttrChange));
  EXPECT_FALSE(registry.ContainsInvalidationType(kInvalidateOnClassAttrChange));
  EXPECT_FALSE(
      registry.ContainsInvalidationType(kInvalidateOnIdNameAttrChange));

  // Addition of another node list with another invalidation type.
  registry.Add(b, kInvalidateOnClassAttrChange);
  EXPECT_FALSE(registry.IsEmpty());
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnNameAttrChange));
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnClassAttrChange));
  EXPECT_FALSE(
      registry.ContainsInvalidationType(kInvalidateOnIdNameAttrChange));

  // It is okay for the same node list to be added with different invalidation
  // types.
  registry.Add(a, kInvalidateOnIdNameAttrChange);
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnNameAttrChange));
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnClassAttrChange));
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnIdNameAttrChange));
}

// The set of types which match should be updated as elements are removed.
TEST_F(LiveNodeListRegistryTest, ExplicitRemove) {
  LiveNodeListRegistry registry;
  const auto* a = CreateNodeList();
  const auto* b = CreateNodeList();

  registry.Add(a, kInvalidateOnNameAttrChange);
  registry.Add(b, kInvalidateOnClassAttrChange);
  registry.Add(a, kInvalidateOnIdNameAttrChange);
  EXPECT_FALSE(registry.IsEmpty());
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnNameAttrChange));
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnClassAttrChange));
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnIdNameAttrChange));

  registry.Remove(a, kInvalidateOnNameAttrChange);
  EXPECT_FALSE(registry.IsEmpty());
  EXPECT_FALSE(registry.ContainsInvalidationType(kInvalidateOnNameAttrChange));
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnClassAttrChange));
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnIdNameAttrChange));

  registry.Remove(a, kInvalidateOnIdNameAttrChange);
  EXPECT_FALSE(registry.IsEmpty());
  EXPECT_FALSE(registry.ContainsInvalidationType(kInvalidateOnNameAttrChange));
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnClassAttrChange));
  EXPECT_FALSE(
      registry.ContainsInvalidationType(kInvalidateOnIdNameAttrChange));

  registry.Remove(b, kInvalidateOnClassAttrChange);
  EXPECT_TRUE(registry.IsEmpty());
  EXPECT_FALSE(registry.ContainsInvalidationType(kInvalidateOnNameAttrChange));
  EXPECT_FALSE(registry.ContainsInvalidationType(kInvalidateOnClassAttrChange));
  EXPECT_FALSE(
      registry.ContainsInvalidationType(kInvalidateOnIdNameAttrChange));
}

// This is a hack for test purposes. The test below forces a GC to happen and
// claims that there are no GC pointers on the stack. For this to be valid, the
// tracker itself must live on the heap, not on the stack.
struct LiveNodeListRegistryWrapper final
    : public GarbageCollected<LiveNodeListRegistryWrapper> {
  LiveNodeListRegistry registry;
  void Trace(Visitor* visitor) const { visitor->Trace(registry); }
};

// The set of types which match should be updated as elements are removed due to
// the garbage collected. Similar to the previous case, except all references to
// |a| are removed together by the GC.
TEST_F(LiveNodeListRegistryTest, ImplicitRemove) {
  auto wrapper =
      WrapPersistent(MakeGarbageCollected<LiveNodeListRegistryWrapper>());
  auto& registry = wrapper->registry;
  auto a = WrapPersistent(CreateNodeList());
  auto b = WrapPersistent(CreateNodeList());

  registry.Add(a, kInvalidateOnNameAttrChange);
  registry.Add(b, kInvalidateOnClassAttrChange);
  registry.Add(a, kInvalidateOnIdNameAttrChange);
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(registry.IsEmpty());
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnNameAttrChange));
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnClassAttrChange));
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnIdNameAttrChange));

  a.Clear();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(registry.IsEmpty());
  EXPECT_FALSE(registry.ContainsInvalidationType(kInvalidateOnNameAttrChange));
  EXPECT_TRUE(registry.ContainsInvalidationType(kInvalidateOnClassAttrChange));
  EXPECT_FALSE(
      registry.ContainsInvalidationType(kInvalidateOnIdNameAttrChange));

  b.Clear();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(registry.IsEmpty());
  EXPECT_FALSE(registry.ContainsInvalidationType(kInvalidateOnNameAttrChange));
  EXPECT_FALSE(registry.ContainsInvalidationType(kInvalidateOnClassAttrChange));
  EXPECT_FALSE(
      registry.ContainsInvalidationType(kInvalidateOnIdNameAttrChange));
}

}  // namespace
}  // namespace blink
