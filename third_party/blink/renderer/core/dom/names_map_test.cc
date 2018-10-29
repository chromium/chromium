// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/names_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

typedef HashMap<String, String> ExpectedMap;

void ExpectEqMap(const ExpectedMap& exp, NamesMap& map) {
  EXPECT_EQ(exp.size(), map.size());

  for (auto kv : exp) {
    base::Optional<SpaceSplitString> value = map.Get(AtomicString(kv.key));
    if (!value) {
      ADD_FAILURE() << "key: " << kv.key << " was nullptr";
      return;
    }
    EXPECT_EQ(kv.value, value.value().SerializeToString())
        << "for key: " << kv.key;
  }
}

TEST(NamesMapTest, Set) {
  Vector<std::pair<String, ExpectedMap>> test_cases({
      // Various valid values.
      {"foo", {{"foo", "foo"}}},
      {"foo: bar", {{"foo", "bar"}}},
      {"foo : bar", {{"foo", "bar"}}},
      {"foo: bar, foo: buz", {{"foo", "bar buz"}}},
      {"foo: bar, buz", {{"foo", "bar"}, {"buz", "buz"}}},
      {"foo: bar, buz: bar", {{"foo", "bar"}, {"buz", "bar"}}},
      {"foo, buz: bar", {{"foo", "foo"}, {"buz", "bar"}}},

      // This is an error but qux should be ignored.
      {"foo: bar qux, buz: bar", {{"foo", "bar"}, {"buz", "bar"}}},
      {"foo: bar, buz: bar qux", {{"foo", "bar"}, {"buz", "bar"}}},

      // This is an error but the extra commas and colons should be ignored.
      {"foo:", {{"foo", "foo"}}},
      {"foo:,", {{"foo", "foo"}}},
      {"foo :", {{"foo", "foo"}}},
      {"foo :,", {{"foo", "foo"}}},
      {"foo: bar, buz:", {{"foo", "bar"}, {"buz", "buz"}}},
      {"foo: bar, buz :", {{"foo", "bar"}, {"buz", "buz"}}},
      {",foo: bar, buz: bar", {{"foo", "bar"}, {"buz", "bar"}}},
      {"foo: bar,, buz: bar", {{"foo", "bar"}, {"buz", "bar"}}},
      {"foo: bar, buz: bar,", {{"foo", "bar"}, {"buz", "bar"}}},
      {"foo: bar, buz: bar,,", {{"foo", "bar"}, {"buz", "bar"}}},
      {":foo: bar, buz: bar", {{"foo", "bar"}, {"buz", "bar"}}},
      {"foo: bar:, buz: bar", {{"foo", "bar"}, {"buz", "bar"}}},
      {"foo: :bar, buz: bar", {{"foo", "bar"}, {"buz", "bar"}}},
      {"foo: bar, buz: bar:", {{"foo", "bar"}, {"buz", "bar"}}},
      {"foo: bar, buz: bar::", {{"foo", "bar"}, {"buz", "bar"}}},

      // Spaces in odd places.
      {" foo: bar, buz: bar", {{"foo", "bar"}, {"buz", "bar"}}},
      {"foo: bar,  buz: bar", {{"foo", "bar"}, {"buz", "bar"}}},
      {"foo: bar, buz: bar ", {{"foo", "bar"}, {"buz", "bar"}}},
  });

  NamesMap map;
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.first);
    map.Set(AtomicString(test_case.first));
    ExpectEqMap(test_case.second, map);
  }
}

TEST(NamesMapTest, SetNull) {
  NamesMap map;
  map.Set(AtomicString("foo bar"));
  map.Set(g_null_atom);
  ExpectEqMap({}, map);
}
}  // namespace blink
