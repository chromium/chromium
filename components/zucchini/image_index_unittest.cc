// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/image_index.h"

#include <stddef.h>

#include <numeric>
#include <vector>

#include "base/test/gtest_util.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/test_disassembler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

class ImageIndexTest : public testing::Test {
 protected:
  ImageIndexTest()
      : buffer_(20),
        image_index_(ConstBufferView(buffer_.data(), buffer_.size())) {
    std::iota(buffer_.begin(), buffer_.end(), 0);
  }

  void InitializeWithDefaultTestData() {
    TestDisassembler disasm({2, TypeTag(0), PoolTag(0)},
                            {{1, 0}, {8, 1}, {10, 2}},
                            {4, TypeTag(1), PoolTag(0)}, {{3, 3}},
                            {3, TypeTag(2), PoolTag(1)}, {{12, 4}, {17, 5}});
    EXPECT_TRUE(image_index_.Initialize(&disasm));
  }

  std::vector<uint8_t> buffer_;
  ImageIndex image_index_;
};

TEST_F(ImageIndexTest, TypeAndPool) {
  TestDisassembler disasm({2, TypeTag(0), PoolTag(0)}, {},
                          {4, TypeTag(1), PoolTag(0)}, {},
                          {3, TypeTag(2), PoolTag(1)}, {});
  EXPECT_TRUE(image_index_.Initialize(&disasm));

  EXPECT_EQ(3U, image_index_.TypeCount());
  EXPECT_EQ(2U, image_index_.PoolCount());

  EXPECT_EQ(TypeTag(0), image_index_.refs(TypeTag(0)).type_tag());
  EXPECT_EQ(TypeTag(1), image_index_.refs(TypeTag(1)).type_tag());
  EXPECT_EQ(TypeTag(2), image_index_.refs(TypeTag(2)).type_tag());

  EXPECT_EQ(PoolTag(0), image_index_.refs(TypeTag(0)).pool_tag());
  EXPECT_EQ(PoolTag(0), image_index_.refs(TypeTag(1)).pool_tag());
  EXPECT_EQ(PoolTag(1), image_index_.refs(TypeTag(2)).pool_tag());
}

TEST_F(ImageIndexTest, InvalidInitialize1) {
  // Overlap within the same group.
  TestDisassembler disasm({2, TypeTag(0), PoolTag(0)}, {{1, 0}, {2, 0}},
                          {4, TypeTag(1), PoolTag(0)}, {},
                          {3, TypeTag(2), PoolTag(1)}, {});
  EXPECT_FALSE(image_index_.Initialize(&disasm));
}

TEST_F(ImageIndexTest, InvalidInitialize2) {
  // Overlap across different readers.
  TestDisassembler disasm({2, TypeTag(0), PoolTag(0)},
                          {{1, 0}, {8, 1}, {10, 2}},
                          {4, TypeTag(1), PoolTag(0)}, {{3, 3}},
                          {3, TypeTag(2), PoolTag(1)}, {{11, 0}});
  EXPECT_FALSE(image_index_.Initialize(&disasm));
}

TEST_F(ImageIndexTest, LookupType) {
  InitializeWithDefaultTestData();

  std::vector<int> expected = {
      -1,            // raw
      0,  0,         // ref 0
      1,  1,  1, 1,  // ref 1
      -1,            // raw
      0,  0,         // ref 0
      0,  0,         // ref 0
      2,  2,  2,     // ref 2
      -1, -1,        // raw
      2,  2,  2,     // ref 2
  };

  for (offset_t i = 0; i < image_index_.size(); ++i)
    EXPECT_EQ(TypeTag(expected[i]), image_index_.LookupType(i));
}

TEST_F(ImageIndexTest, IsToken) {
  InitializeWithDefaultTestData();

  std::vector<bool> expected = {
      true,                       // raw
      true, false,                // ref 0
      true, false, false, false,  // ref 1
      true,                       // raw
      true, false,                // ref 0
      true, false,                // ref 0
      true, false, false,         // ref 2
      true, true,                 // raw
      true, false, false,         // ref 2
  };

  for (offset_t i = 0; i < image_index_.size(); ++i)
    EXPECT_EQ(expected[i], image_index_.IsToken(i));
}

TEST_F(ImageIndexTest, IsReference) {
  InitializeWithDefaultTestData();

  std::vector<bool> expected = {
      false,                     // raw
      true,  true,               // ref 0
      true,  true,  true, true,  // ref 1
      false,                     // raw
      true,  true,               // ref 0
      true,  true,               // ref 0
      true,  true,  true,        // ref 2
      false, false,              // raw
      true,  true,  true,        // ref 2
  };

  for (offset_t i = 0; i < image_index_.size(); ++i)
    EXPECT_EQ(expected[i], image_index_.IsReference(i));
}

}  // namespace zucchini
