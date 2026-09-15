// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dom_node_id_traits.h"

#include <stdint.h>

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {
namespace {

constexpr DOMNodeIdType kMaxId(std::numeric_limits<int32_t>::max());

TEST(DOMNodeIdTraitsTest, HashMap) {
  HashMap<DOMNodeIdType, int> map;
  const DOMNodeIdType id(1);

  EXPECT_TRUE(map.insert(id, 10).is_new_entry);
  EXPECT_TRUE(map.insert(kMaxId, 20).is_new_entry);
  EXPECT_FALSE(map.insert(id, 30).is_new_entry);
  EXPECT_EQ(map.size(), 2u);
  EXPECT_EQ(map.at(id), 10);
  EXPECT_EQ(map.at(kMaxId), 20);

  map.erase(id);
  EXPECT_FALSE(map.Contains(id));
  EXPECT_EQ(map.at(kMaxId), 20);

  EXPECT_TRUE(map.insert(id, 30).is_new_entry);
  EXPECT_EQ(map.at(id), 30);
  EXPECT_EQ(map.size(), 2u);
}

TEST(DOMNodeIdTraitsTest, HashSet) {
  HashSet<DOMNodeIdType> set;
  const DOMNodeIdType id(1);

  EXPECT_TRUE(set.insert(id).is_new_entry);
  EXPECT_TRUE(set.insert(kMaxId).is_new_entry);
  EXPECT_FALSE(set.insert(id).is_new_entry);
  EXPECT_EQ(set.size(), 2u);
  EXPECT_TRUE(set.Contains(id));
  EXPECT_TRUE(set.Contains(kMaxId));

  set.erase(id);
  EXPECT_FALSE(set.Contains(id));
  EXPECT_TRUE(set.Contains(kMaxId));

  EXPECT_TRUE(set.insert(id).is_new_entry);
  EXPECT_TRUE(set.Contains(id));
  EXPECT_EQ(set.size(), 2u);
}

TEST(DOMNodeIdTraitsTest, IntegerKeyPolicies) {
  using Traits = HashTraits<DOMNodeIdType>;
  using IntegerTraits = HashTraits<int32_t>;

  static_assert(Traits::kEmptyValueIsZero == IntegerTraits::kEmptyValueIsZero);
  static_assert(Traits::kSupportsCompaction ==
                IntegerTraits::kSupportsCompaction);
  static_assert(Traits::NeedsToForbidGCOnMove<>::value ==
                IntegerTraits::NeedsToForbidGCOnMove<>::value);
  static_assert(Traits::kCanTraceConcurrently ==
                IntegerTraits::kCanTraceConcurrently);
  static_assert(sizeof(DOMNodeIdType) == sizeof(int32_t));

  EXPECT_EQ(Traits::EmptyValue().value(), IntegerTraits::EmptyValue());
  EXPECT_EQ(Traits::DeletedValue().value(), IntegerTraits::DeletedValue());
  EXPECT_NE(Traits::EmptyValue(), Traits::DeletedValue());
  EXPECT_TRUE(IsHashTraitsEmptyValue<Traits>(DOMNodeIdType()));
  EXPECT_TRUE(IsHashTraitsDeletedValue<Traits>(DOMNodeIdType(-1)));
  EXPECT_FALSE(IsHashTraitsEmptyOrDeletedValue<Traits>(DOMNodeIdType(1)));
  EXPECT_FALSE(IsHashTraitsEmptyOrDeletedValue<Traits>(kMaxId));
  EXPECT_EQ(Traits::GetHash(kMaxId), IntegerTraits::GetHash(kMaxId.value()));
}

}  // namespace
}  // namespace blink
