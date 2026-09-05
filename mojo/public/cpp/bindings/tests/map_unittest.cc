// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/tests/rect_chromium.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/interfaces/bindings/tests/rect.test-mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_structs.test-mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace mojo {
namespace test {
namespace {

using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(MapTest, StructKey) {
  base::flat_map<RectPtr, int32_t> map;
  map.insert({Rect::New(1, 2, 3, 4), 123});

  RectPtr key = Rect::New(1, 2, 3, 4);
  ASSERT_NE(map.end(), map.find(key));
  ASSERT_EQ(123, map.find(key)->second);

  map.erase(key);
  ASSERT_EQ(0u, map.size());
}

TEST(MapTest, TypemappedStructKey) {
  base::flat_map<ContainsHashablePtr, int32_t> map;
  map.insert({ContainsHashable::New(RectChromium(1, 2, 3, 4)), 123});

  ContainsHashablePtr key = ContainsHashable::New(RectChromium(1, 2, 3, 4));
  ASSERT_NE(map.end(), map.find(key));
  ASSERT_EQ(123, map.find(key)->second);

  map.erase(key);
  ASSERT_EQ(0u, map.size());
}

TEST(MapTest, HashMapTypemappedKey) {
  absl::flat_hash_map<RectChromium, int32_t> map;
  map.insert({RectChromium(1, 2, 3, 4), 123});

  RectChromium key(1, 2, 3, 4);
  EXPECT_THAT(map, Contains(Pair(key, 123)));

  map.erase(key);
  EXPECT_THAT(map, IsEmpty());
}

TEST(MapTest, HashMapSerialization) {
  auto input = ContainsHashMap::New();
  input->map_str_int["hello"] = 1;
  input->map_str_int["world"] = 2;
  absl::flat_hash_map<int32_t, std::string> nullable_map;
  nullable_map[42] = "answer";
  input->nullable_map = std::move(nullable_map);

  auto cloned = input.Clone();
  EXPECT_EQ(input, cloned);

  ContainsHashMapPtr output;
  ASSERT_TRUE(SerializeAndDeserialize<ContainsHashMap>(input, output));
  EXPECT_EQ(cloned, output);
  EXPECT_THAT(output->map_str_int,
              UnorderedElementsAre(Pair("hello", 1), Pair("world", 2)));
  EXPECT_THAT(output->nullable_map,
              Optional(UnorderedElementsAre(Pair(42, "answer"))));
}

TEST(MapTest, MapAndHashMapWireCompatibility) {
  auto contains_map = ContainsMap::New();
  contains_map->map_str_int["hello"] = 1;
  contains_map->map_str_int["world"] = 2;
  base::flat_map<int32_t, std::string> nullable_map;
  nullable_map[42] = "answer";
  contains_map->nullable_map = std::move(nullable_map);

  // Test that we can serialize ContainsMap and Deserialize as ContainsHashMap.
  auto serialized_map = ContainsMap::Serialize(&contains_map);

  ContainsHashMapPtr hash_map;
  ASSERT_TRUE(ContainsHashMap::Deserialize(serialized_map, &hash_map));
  EXPECT_THAT(hash_map->map_str_int,
              UnorderedElementsAre(Pair("hello", 1), Pair("world", 2)));
  EXPECT_THAT(hash_map->nullable_map,
              Optional(UnorderedElementsAre(Pair(42, "answer"))));

  // Now test the opposite direction
  auto serialized_hash_map = ContainsHashMap::Serialize(&hash_map);

  ContainsMapPtr contain_map_roundtrip;
  ASSERT_TRUE(
      ContainsMap::Deserialize(serialized_hash_map, &contain_map_roundtrip));
  EXPECT_THAT(contain_map_roundtrip->map_str_int,
              UnorderedElementsAre(Pair("hello", 1), Pair("world", 2)));
  EXPECT_THAT(contain_map_roundtrip->nullable_map,
              Optional(UnorderedElementsAre(Pair(42, "answer"))));
  EXPECT_EQ(contains_map, contain_map_roundtrip);
}

}  // namespace
}  // namespace test
}  // namespace mojo
