// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/names_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

typedef HashMap<String, String> ExpectedMap;

void ExpectEqMap(const ExpectedMap& exp, NamesMap& map) {
  EXPECT_EQ(exp.size(), map.size());

  for (auto kv : exp) {
    SpaceSplitString* value = map.Get(AtomicString(kv.key));
    if (!value) {
      ADD_FAILURE() << "key: " << kv.key << " was nullptr";
      return;
    }
    EXPECT_EQ(kv.value, value->SerializeToString()) << "for key: " << kv.key;
  }
}

TEST(NamesMapTest, Set) {
  test::TaskEnvironment task_environment;
  // This is vector of pairs where first is an expected output and second is a
  // vector of inputs, all of which should produce that output.
  Vector<std::pair<ExpectedMap, Vector<String>>> test_cases({
      // First a set of tests where we have an expected value and several valid
      // strings that encode that value, followed by strings encode the same
      // value but include invalid input.
      {{},
       {
           // Valid
           "",
           " ",
           "  ",
           ",",
           ",,",
           " ,",
           ", ",
           " , , ",
           // Invalid
           ":",
           "foo:",
           "foo: bar buz",
           ":bar",
           ": bar buz",
       }},
      {{{"foo", "foo"}},
       {
           // Valid
           "foo",
           " foo",
           ", foo",
           ", foo",
           "foo",
           "foo ",
           "foo,",
           "foo ,"
           // Plus invalid
           ":,foo",
           ":bar,foo",
           "bar:,foo",
           "bar: bar buz,foo",
           "foo,:",
           "foo, :bar",
           "foo, bar:",
           "foo, bar: bar buz",
       }},
      {{{"foo", "bar"}},
       {
           // Valid
           "foo:bar",
           " foo:bar",
           "foo :bar",
           "foo: bar",
           "foo:bar ",
           "foo:bar",
           ",foo:bar",
           ", foo:bar",
           " ,foo:bar",
           "foo:bar,",
           "foo:bar, ",
           "foo:bar ,",
           // Plus invalid
           ":,foo:bar",
           ":bar,foo:bar",
           "bar:,foo:bar",
           "bar: bar buz,foo:bar",
           "foo:bar,:",
           "foo:bar, :bar",
           "foo:bar, bar:",
           "foo:bar, bar: bar buz",
       }},
      {{{"foo", "bar buz"}},
       {
           // Valid
           "foo:bar,foo:buz",
           "foo:bar, foo:buz",
           "foo:bar ,foo:buz",
           // Plus invalid. In this case invalid occurs between the valid items.
           "foo:bar,bar:,foo:buz",
           "foo:bar,bar: ,foo:buz",
           "foo:bar,:bar,foo:buz",
           "foo:bar, :bar,foo:buz",
           "foo:bar,bar: bill bob,foo:buz",
       }},
      // Miscellaneous tests.
      // Same value for 2 keys.
      {{{"foo", "bar"}, {"buz", "bar"}}, {"foo:bar,buz:bar"}},
      // Mix key-only with key-value.
      {{{"foo", "foo"}, {"buz", "bar"}}, {"foo,buz:bar", "buz:bar,foo"}},
  });

  NamesMap* map = MakeGarbageCollected<NamesMap>();
  for (auto test_case : test_cases) {
    for (String input : test_case.second) {
      SCOPED_TRACE(input);
      map->Set(AtomicString(input));
      ExpectEqMap(test_case.first, *map);
    }
  }
}

TEST(NamesMapTest, SetNull) {
  test::TaskEnvironment task_environment;
  NamesMap* map = MakeGarbageCollected<NamesMap>();
  map->Set(AtomicString("foo bar"));
  map->Set(g_null_atom);
  ExpectEqMap({}, *map);
}
}  // namespace blink
