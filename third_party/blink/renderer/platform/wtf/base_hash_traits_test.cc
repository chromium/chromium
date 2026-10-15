// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/base_hash_traits.h"

#include "base/i18n/language_tag.h"
#include "base/token.h"
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

TEST(BaseHashTraitsTest, UnguessableTokenHashTraitsValues) {
  // Empty and Deleted values should not crash when hashed.
  EXPECT_EQ(HashTraits<base::UnguessableToken>::GetHash(
                HashTraits<base::UnguessableToken>::EmptyValue()),
            0u);
  EXPECT_NE(HashTraits<base::UnguessableToken>::GetHash(
                HashTraits<base::UnguessableToken>::DeletedValue()),
            0u);

  EXPECT_FALSE(HashTraits<base::UnguessableToken>::DeletedValue().is_empty());
}

TEST(BaseHashTraitsTest, TokenHashMap) {
  HashMap<base::Token, String> map;
  base::Token token1 = base::Token::CreateRandom();
  base::Token token2 = base::Token::CreateRandom();

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

TEST(BaseHashTraitsTest, TokenHashSet) {
  HashSet<base::Token> set;
  base::Token token1 = base::Token::CreateRandom();
  base::Token token2 = base::Token::CreateRandom();

  set.insert(token1);
  set.insert(token2);
  EXPECT_EQ(set.size(), 2u);

  EXPECT_TRUE(set.Contains(token1));
  EXPECT_TRUE(set.Contains(token2));

  set.erase(token1);
  EXPECT_FALSE(set.Contains(token1));
  EXPECT_TRUE(set.Contains(token2));
}

TEST(BaseHashTraitsTest, TokenHashTraitsValues) {
  // Empty and Deleted values should not crash when hashed.
  EXPECT_EQ(
      HashTraits<base::Token>::GetHash(HashTraits<base::Token>::EmptyValue()),
      0u);
  EXPECT_NE(
      HashTraits<base::Token>::GetHash(HashTraits<base::Token>::DeletedValue()),
      0u);

  EXPECT_FALSE(HashTraits<base::Token>::DeletedValue().is_zero());
}

TEST(BaseHashTraitsTest, LanguageTagHashMap) {
  HashMap<base::i18n::LanguageTag, String> map;
  const base::i18n::LanguageTag tag1 = base::i18n::GetKnownLanguageTag("en-US");
  const base::i18n::LanguageTag tag2 = base::i18n::GetKnownLanguageTag("pt-BR");

  EXPECT_TRUE(map.empty());
  map.insert(tag1, "tag1");
  map.insert(tag2, "tag2");
  EXPECT_EQ(map.size(), 2u);

  EXPECT_TRUE(map.Contains(tag1));
  EXPECT_TRUE(map.Contains(tag2));
  EXPECT_EQ(map.at(tag1), "tag1");
  EXPECT_EQ(map.at(tag2), "tag2");

  // Tags that differ only by subtag are distinct keys.
  EXPECT_FALSE(map.Contains(base::i18n::GetKnownLanguageTag("en")));

  EXPECT_EQ(map.Take(tag1), "tag1");
  EXPECT_EQ(map.size(), 1u);
  EXPECT_FALSE(map.Contains(tag1));
  EXPECT_TRUE(map.Contains(tag2));
}

TEST(BaseHashTraitsTest, LanguageTagHashSet) {
  HashSet<base::i18n::LanguageTag> set;
  const base::i18n::LanguageTag tag1 = base::i18n::GetKnownLanguageTag("en");
  const base::i18n::LanguageTag tag2 = base::i18n::GetKnownLanguageTag("de-DE");

  set.insert(tag1);
  set.insert(tag2);
  EXPECT_EQ(set.size(), 2u);

  EXPECT_TRUE(set.Contains(tag1));
  EXPECT_TRUE(set.Contains(tag2));

  set.erase(tag1);
  EXPECT_FALSE(set.Contains(tag1));
  EXPECT_TRUE(set.Contains(tag2));
}

TEST(BaseHashTraitsTest, LanguageTagHashTraitsValues) {
  using Traits = HashTraits<base::i18n::LanguageTag>;

  // The empty and deleted sentinels must be distinct from each other and from
  // any real language tag.
  EXPECT_NE(Traits::EmptyValue(), Traits::DeletedValue());
  EXPECT_NE(Traits::EmptyValue(), base::i18n::GetKnownLanguageTag("en"));
  EXPECT_NE(Traits::DeletedValue(), base::i18n::GetKnownLanguageTag("en"));

  // Hashing the sentinels should not crash.
  EXPECT_NE(Traits::GetHash(Traits::EmptyValue()),
            Traits::GetHash(Traits::DeletedValue()));
}

}  // namespace

}  // namespace blink
