// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/ignoring_ascii_case_hash.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(IgnoringAsciiCaseHashTest, GetHashIgnoringCase) {
  using iac = IgnoringAsciiCaseHash;
  EXPECT_EQ(HashTraits<String>::GetHash(String("a")), iac::GetHash("a"));
  EXPECT_EQ(HashTraits<String>::GetHash(String("a")),
            iac::GetHash(AtomicString("a")));

  unsigned hash = iac::GetHash(String("a"));
  EXPECT_EQ(hash, iac::GetHash("A"));
  EXPECT_EQ(hash, iac::GetHash(u"A"));
  EXPECT_EQ(hash, iac::GetHash(AtomicString("A")));

  hash = iac::GetHash(String("bc"));
  EXPECT_EQ(hash, iac::GetHash("Bc"));
  EXPECT_EQ(hash, iac::GetHash(u"bC"));

  hash = iac::GetHash(String("def"));
  EXPECT_EQ(hash, iac::GetHash("Def"));
  EXPECT_EQ(hash, iac::GetHash(u"dEF"));

  // 4 characters.
  hash = iac::GetHash(String("ghij"));
  EXPECT_EQ(hash, iac::GetHash("GhiJ"));
  EXPECT_EQ(hash, iac::GetHash(u"ghIJ"));

  // 8 characters.
  hash = iac::GetHash(String("klmnopqr"));
  EXPECT_EQ(hash, iac::GetHash("klMnOpqr"));
  EXPECT_EQ(hash, iac::GetHash(u"klmnoPQr"));

  // 17 characters for Read64.
  hash = iac::GetHash(String("stuvwxyz@abcdefgh"));
  EXPECT_EQ(hash, iac::GetHash("stUvwxyz@ABCDEFGH"));
  EXPECT_EQ(hash, iac::GetHash(u"StuvWXyZ@abcdefgH"));
}

TEST(IgnoringAsciiCaseHashTest, NonAscii) {
  using iac = IgnoringAsciiCaseHash;
  // U+00DF is the lowercase of U+1E9E.
  const UChar kSharpS[] = {0x00df, 0};
  const UChar kSharpSInUpperCase[] = {0x1e9e, 0};
  String sharp_s(kSharpS);
  String sharp_s_in_upper_case(kSharpSInUpperCase);
  // Unlike CaseFoldingHash, IgnoringAsciiCaseHash should not fold non-ASCII
  // characters.
  EXPECT_FALSE(iac::Equal(sharp_s, sharp_s_in_upper_case));
  EXPECT_NE(iac::GetHash(sharp_s), iac::GetHash(sharp_s_in_upper_case));

  // Turkish I.
  const UChar kI[] = {'I', 0};
  const UChar kDotlessI[] = {0x0131, 0};
  String i(kI);
  String dotless_i(kDotlessI);
  EXPECT_FALSE(iac::Equal(i, dotless_i));
  EXPECT_NE(iac::GetHash(i), iac::GetHash(dotless_i));
}

TEST(IgnoringAsciiCaseHashTest, StringKeyHash) {
  HashMap<String, int, IgnoringAsciiCaseHashTraits<String>> map;
  map.insert("k", 1);
  map.insert("K", 2);
  EXPECT_EQ(1u, map.size());

  const auto not_found = map.end();
  EXPECT_NE(not_found, map.find("k"));
  EXPECT_NE(not_found, map.find("K"));
  EXPECT_NE(not_found, map.find(u"K"));
  EXPECT_NE(not_found, map.find(AtomicString("K")));
  // U+212A Kelvin sign should not match to ASCII "k" because of no FoldCase().
  EXPECT_EQ(not_found, map.find(u"\u212A"));
}

TEST(IgnoringAsciiCaseHashTest, AtomicStringKeyHash) {
  HashMap<AtomicString, int, IgnoringAsciiCaseHashTraits<AtomicString>> map;
  map.insert(AtomicString("k"), 1);
  map.insert(AtomicString("K"), 2);
  EXPECT_EQ(1u, map.size());

  const auto not_found = map.end();
  EXPECT_NE(not_found, map.find(AtomicString("k")));
  EXPECT_NE(not_found, map.find(AtomicString("K")));
  // U+212A Kelvin sign should not match to ASCII "k" because of no FoldCase().
  EXPECT_EQ(not_found, map.find(AtomicString(u"\u212A")));
}

TEST(IgnoringAsciiCaseHashTest, Translator) {
  using Translator = IgnoringAsciiCaseHashTranslator;
  HashMap<String, int, IgnoringAsciiCaseHashTraits<String>> map;
  map.insert("k", 42);
  EXPECT_EQ(1u, map.size());
  auto it = map.Find<Translator, StringView>("k");
  EXPECT_NE(map.end(), it);
  it = map.Find<Translator, StringView>(u"k");
  EXPECT_NE(map.end(), it);
  it = map.Find<Translator, StringView>(u"\u212A");
  EXPECT_EQ(map.end(), it);

  HashSet<AtomicString, IgnoringAsciiCaseHashTraits<AtomicString>> set;
  set.insert(AtomicString("k"));
  EXPECT_EQ(1u, set.size());
  auto set_it = set.Find<Translator, StringView>("k");
  EXPECT_NE(set.end(), set_it);
  set_it = set.Find<Translator, StringView>(u"k");
  EXPECT_NE(set.end(), set_it);
  set_it = set.Find<Translator, StringView>(u"\u212A");
  EXPECT_EQ(set.end(), set_it);
}

}  // namespace blink
