// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_scope.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

// This test verifies that hash tables of Member<AnchorScopedName>
// hash by value instead of address.
//
// See AnchorScopedNameHashTraits.

namespace {

AnchorScopedName* MakeName(const char* str,
                           const TreeScope* tree_scope,
                           const Element* element) {
  auto* name =
      MakeGarbageCollected<ScopedCSSName>(AtomicString(str), tree_scope);
  return MakeGarbageCollected<AnchorScopedName>(*name, element);
}

}  // namespace

TEST(AnchorScopedNameTest, HashInsertDuplicate) {
  AnchorScopedName* foo =
      MakeName("foo", /*tree_scope=*/nullptr, /*element=*/nullptr);

  HeapHashSet<Member<AnchorScopedName>> hash_set;
  EXPECT_TRUE(hash_set.insert(foo).is_new_entry);
  EXPECT_FALSE(hash_set.insert(foo).is_new_entry);
  EXPECT_NE(hash_set.find(foo), hash_set.end());
  EXPECT_EQ(*hash_set.find(foo), foo);
}

TEST(AnchorScopedNameTest, HashDifferentNames) {
  AnchorScopedName* foo =
      MakeName("foo", /*tree_scope=*/nullptr, /*element=*/nullptr);
  AnchorScopedName* bar =
      MakeName("bar", /*tree_scope=*/nullptr, /*element=*/nullptr);
  EXPECT_NE(foo->GetHash(), bar->GetHash());

  HeapHashSet<Member<AnchorScopedName>> hash_set;
  EXPECT_TRUE(hash_set.insert(foo).is_new_entry);
  EXPECT_TRUE(hash_set.insert(bar).is_new_entry);
}

TEST(AnchorScopedNameTest, HashEqualNames) {
  AnchorScopedName* foo1 =
      MakeName("foo", /*tree_scope=*/nullptr, /*element=*/nullptr);
  AnchorScopedName* foo2 =
      MakeName("foo", /*tree_scope=*/nullptr, /*element=*/nullptr);
  AnchorScopedName* foo3 =
      MakeName("foo", /*tree_scope=*/nullptr, /*element=*/nullptr);
  EXPECT_EQ(foo1->GetHash(), foo2->GetHash());
  EXPECT_EQ(foo2->GetHash(), foo3->GetHash());

  HeapHashSet<Member<AnchorScopedName>> hash_set;
  EXPECT_TRUE(hash_set.insert(foo1).is_new_entry);
  EXPECT_FALSE(hash_set.insert(foo2).is_new_entry);
  EXPECT_FALSE(hash_set.insert(foo3).is_new_entry);
}

TEST(AnchorScopedNameTest, LookupEmpty) {
  AnchorScopedName* foo =
      MakeName("foo", /*tree_scope=*/nullptr, /*element=*/nullptr);

  HeapHashSet<Member<const AnchorScopedName>> hash_set;
  EXPECT_EQ(hash_set.end(), hash_set.find(foo));
}

TEST(AnchorScopedNameTest, LookupDeleted) {
  AnchorScopedName* foo =
      MakeName("foo", /*tree_scope=*/nullptr, /*element=*/nullptr);

  HeapHashSet<Member<const AnchorScopedName>> hash_set;
  EXPECT_TRUE(hash_set.insert(foo).is_new_entry);
  EXPECT_EQ(1u, hash_set.size());
  hash_set.erase(foo);
  EXPECT_EQ(0u, hash_set.size());
  // Don't crash:
  EXPECT_EQ(hash_set.end(), hash_set.find(foo));
}

TEST(AnchorScopedNameTest, ElementEquality) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* div1 = MakeGarbageCollected<HTMLDivElement>(*document);
  auto* div2 = MakeGarbageCollected<HTMLDivElement>(*document);

  AnchorScopedName* foo1 =
      MakeName("foo", /*tree_scope=*/nullptr, /*element=*/div1);
  AnchorScopedName* foo2 =
      MakeName("foo", /*tree_scope=*/nullptr, /*element=*/div1);
  AnchorScopedName* foo3 =
      MakeName("foo", /*tree_scope=*/nullptr, /*element=*/div2);

  EXPECT_EQ(foo1->GetHash(), foo2->GetHash());
  EXPECT_NE(foo2->GetHash(), foo3->GetHash());

  EXPECT_EQ(*foo1, *foo2);
  EXPECT_NE(*foo2, *foo3);
}

TEST(AnchorScopedNameTest, TreeScopeEquality) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document1 =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* document2 =
      Document::CreateForTest(execution_context.GetExecutionContext());

  AnchorScopedName* foo1 =
      MakeName("foo", /*tree_scope=*/document1, /*element=*/nullptr);
  AnchorScopedName* foo2 =
      MakeName("foo", /*tree_scope=*/document1, /*element=*/nullptr);
  AnchorScopedName* foo3 =
      MakeName("foo", /*tree_scope=*/document2, /*element=*/nullptr);

  EXPECT_EQ(foo1->GetHash(), foo2->GetHash());
  EXPECT_NE(foo2->GetHash(), foo3->GetHash());

  EXPECT_EQ(*foo1, *foo2);
  EXPECT_NE(*foo2, *foo3);
}

}  // namespace blink
