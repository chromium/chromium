// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/two_keys_adapter_map.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TwoKeysAdapterMapTest : public ::testing::Test {
 public:
  struct MoveOnlyValue {
    explicit MoveOnlyValue(std::string str) : str(std::move(str)) {}
    MoveOnlyValue(MoveOnlyValue&& other) : str(std::move(other.str)) {}
    MoveOnlyValue& operator=(MoveOnlyValue&& other) {
      str = std::move(other.str);
      return *this;
    }

    std::string str;

    DISALLOW_COPY_AND_ASSIGN(MoveOnlyValue);
  };

  TwoKeysAdapterMap<std::string, std::string, MoveOnlyValue> map_;
};

TEST_F(TwoKeysAdapterMapTest, ShouldInitiallyBeEmpty) {
  EXPECT_EQ(0u, map_.PrimarySize());
  EXPECT_EQ(0u, map_.SecondarySize());
  EXPECT_FALSE(map_.FindByPrimary("invalid"));
  EXPECT_FALSE(map_.FindBySecondary("invalid"));
}

TEST_F(TwoKeysAdapterMapTest, InsertPrimaryShouldAllowLookup) {
  map_.Insert("aPrimary", MoveOnlyValue("aValue"));
  EXPECT_EQ(1u, map_.PrimarySize());
  EXPECT_EQ(0u, map_.SecondarySize());
  EXPECT_TRUE(map_.FindByPrimary("aPrimary"));
  EXPECT_EQ("aValue", map_.FindByPrimary("aPrimary")->str);
}

TEST_F(TwoKeysAdapterMapTest, SetSecondaryKeyShouldAllowLookup) {
  map_.Insert("aPrimary", MoveOnlyValue("aValue"));
  map_.SetSecondaryKey("aPrimary", "aSecondary");
  EXPECT_EQ(1u, map_.SecondarySize());
  EXPECT_TRUE(map_.FindBySecondary("aSecondary"));
  EXPECT_EQ("aValue", map_.FindBySecondary("aSecondary")->str);
}

TEST_F(TwoKeysAdapterMapTest, EraseByPrimaryShouldRemoveElement) {
  map_.Insert("aPrimary", MoveOnlyValue("aValue"));
  map_.SetSecondaryKey("aPrimary", "aSecondary");
  EXPECT_TRUE(map_.EraseByPrimary("aPrimary"));
  EXPECT_EQ(0u, map_.PrimarySize());
  EXPECT_EQ(0u, map_.SecondarySize());
  EXPECT_FALSE(map_.FindByPrimary("aPrimary"));
  EXPECT_FALSE(map_.FindBySecondary("aSecondary"));
}

TEST_F(TwoKeysAdapterMapTest, EraseBySecondaryShouldRemoveElement) {
  map_.Insert("aPrimary", MoveOnlyValue("aValue"));
  map_.SetSecondaryKey("aPrimary", "aSecondary");
  EXPECT_TRUE(map_.EraseBySecondary("aSecondary"));
  EXPECT_EQ(0u, map_.PrimarySize());
  EXPECT_EQ(0u, map_.SecondarySize());
  EXPECT_FALSE(map_.FindByPrimary("aPrimary"));
  EXPECT_FALSE(map_.FindBySecondary("aSecondary"));
}

TEST_F(TwoKeysAdapterMapTest, EraseInvalidElementShouldReturnFalse) {
  EXPECT_FALSE(map_.EraseByPrimary("invalid"));
  EXPECT_FALSE(map_.EraseBySecondary("invalid"));
}

}  // namespace blink
