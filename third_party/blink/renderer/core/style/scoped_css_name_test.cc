// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/scoped_css_name.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

// Tests that hash tables of Member<ScopedCSSName> hash the names
// themselves, not the wrapper pointers.

TEST(ScopedCSSNameTest, HashInsertDuplicate) {
  ScopedCSSName* foo =
      MakeGarbageCollected<ScopedCSSName>(AtomicString("foo"), nullptr);

  HeapHashSet<Member<ScopedCSSName>> hash_set;
  EXPECT_TRUE(hash_set.insert(foo).is_new_entry);
  EXPECT_FALSE(hash_set.insert(foo).is_new_entry);
  EXPECT_NE(hash_set.find(foo), hash_set.end());
  EXPECT_EQ(*hash_set.find(foo), foo);
}

TEST(ScopedCSSNameTest, HashDifferentNames) {
  ScopedCSSName* foo =
      MakeGarbageCollected<ScopedCSSName>(AtomicString("foo"), nullptr);
  ScopedCSSName* bar =
      MakeGarbageCollected<ScopedCSSName>(AtomicString("bar"), nullptr);
  EXPECT_NE(foo->GetHash(), bar->GetHash());

  HeapHashSet<Member<ScopedCSSName>> hash_set;
  EXPECT_TRUE(hash_set.insert(foo).is_new_entry);
  EXPECT_TRUE(hash_set.insert(bar).is_new_entry);
}

TEST(ScopedCSSNameTest, HashEqualNames) {
  ScopedCSSName* foo1 =
      MakeGarbageCollected<ScopedCSSName>(AtomicString("foo"), nullptr);
  ScopedCSSName* foo2 =
      MakeGarbageCollected<ScopedCSSName>(AtomicString("foo"), nullptr);
  ScopedCSSName* foo3 =
      MakeGarbageCollected<ScopedCSSName>(AtomicString("foo"), nullptr);
  EXPECT_EQ(foo1->GetHash(), foo2->GetHash());
  EXPECT_EQ(foo2->GetHash(), foo3->GetHash());

  HeapHashSet<Member<ScopedCSSName>> hash_set;
  EXPECT_TRUE(hash_set.insert(foo1).is_new_entry);
  EXPECT_FALSE(hash_set.insert(foo2).is_new_entry);
  EXPECT_FALSE(hash_set.insert(foo3).is_new_entry);
}

TEST(ScopedCSSNameTest, LookupEmpty) {
  ScopedCSSName* foo =
      MakeGarbageCollected<ScopedCSSName>(AtomicString("foo"), nullptr);

  HeapHashSet<Member<const ScopedCSSName>> hash_set;
  EXPECT_EQ(hash_set.end(), hash_set.find(foo));
}

TEST(ScopedCSSNameTest, LookupDeleted) {
  ScopedCSSName* foo =
      MakeGarbageCollected<ScopedCSSName>(AtomicString("foo"), nullptr);

  HeapHashSet<Member<const ScopedCSSName>> hash_set;
  EXPECT_TRUE(hash_set.insert(foo).is_new_entry);
  EXPECT_EQ(1u, hash_set.size());
  hash_set.erase(foo);
  EXPECT_EQ(0u, hash_set.size());
  // Don't crash:
  EXPECT_EQ(hash_set.end(), hash_set.find(foo));
}

}  // namespace blink
