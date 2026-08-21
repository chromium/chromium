// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/base_hash_traits.h"

#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

TEST(BaseHashTraitsTest, UnguessableTokenHashMap) {
  HashMap<base::UnguessableToken, String> map;
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();

  EXPECT_TRUE(map.empty());
  map.insert(token1, "token1");
  map.insert(token2, "token2");
  EXPECT_EQ(map.size(), 2u);

  EXPECT_TRUE(map.Contains(token1));
  EXPECT_TRUE(map.Contains(token2));
  EXPECT_EQ(map.at(token1), "token1");
  EXPECT_EQ(map.at(token2), "token2");

  EXPECT_EQ(map.Take(token1), "token1");
  EXPECT_EQ(map.size(), 1u);
  EXPECT_FALSE(map.Contains(token1));
  EXPECT_TRUE(map.Contains(token2));
}

TEST(BaseHashTraitsTest, UnguessableTokenHashSet) {
  HashSet<base::UnguessableToken> set;
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();

  set.insert(token1);
  set.insert(token2);
  EXPECT_EQ(set.size(), 2u);

  EXPECT_TRUE(set.Contains(token1));
  EXPECT_TRUE(set.Contains(token2));

  set.erase(token1);
  EXPECT_FALSE(set.Contains(token1));
  EXPECT_TRUE(set.Contains(token2));
}

TEST(BaseHashTraitsTest, HashTraitsValues) {
  // Empty and Deleted values should not crash when hashed.
  EXPECT_EQ(HashTraits<base::UnguessableToken>::GetHash(
                HashTraits<base::UnguessableToken>::EmptyValue()),
            0u);
  EXPECT_NE(HashTraits<base::UnguessableToken>::GetHash(
                HashTraits<base::UnguessableToken>::DeletedValue()),
            0u);

  EXPECT_FALSE(HashTraits<base::UnguessableToken>::DeletedValue().is_empty());
}

}  // namespace

}  // namespace blink
