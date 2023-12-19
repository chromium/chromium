// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/patch_utils.h"

#include <stdint.h>

#include <iterator>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

template <class T>
void TestEncodeDecodeVarUInt(const std::vector<T>& data) {
  std::vector<uint8_t> buffer;

  std::vector<T> values;
  for (T basis : data) {
    // For variety, test the neighborhood values for each case in |data|. Some
    // test cases may result in overflow when computing |value|, but we don't
    // care about that.
    for (int delta = -4; delta <= 4; ++delta) {
      T value = delta + basis;
      EncodeVarUInt<T>(value, std::back_inserter(buffer));
      values.push_back(value);

      value = delta - basis;
      EncodeVarUInt<T>(value, std::back_inserter(buffer));
      values.push_back(value);
    }
  }

  auto it = buffer.begin();
  for (T expected : values) {
    T value = T(-1);
    auto res = DecodeVarUInt(it, buffer.end(), &value);
    EXPECT_NE(0, res);
    EXPECT_EQ(expected, value);
    it += res;
  }
  EXPECT_EQ(it, buffer.end());

  T value = T(-1);
  auto res = DecodeVarUInt(it, buffer.end(), &value);
  EXPECT_EQ(0, res);
  EXPECT_EQ(T(-1), value);
}

template <class T>
void TestEncodeDecodeVarInt(const std::vector<T>& data) {
  std::vector<uint8_t> buffer;

  std::vector<T> values;
  for (T basis : data) {
    // For variety, test the neighborhood values for each case in |data|. Some
    // test cases may result in overflow when computing |value|, but we don't
    // care about that.
    for (int delta = -4; delta <= 4; ++delta) {
      T value = delta + basis;
      EncodeVarInt(value, std::back_inserter(buffer));
      values.push_back(value);

      value = delta - basis;
      EncodeVarInt(value, std::back_inserter(buffer));
      values.push_back(value);
    }
  }

  auto it = buffer.begin();
  for (T expected : values) {
    T value = T(-1);
    auto res = DecodeVarInt(it, buffer.end(), &value);
    EXPECT_NE(0, res);
    EXPECT_EQ(expected, value);
    it += res;
  }
  EXPECT_EQ(it, buffer.end());

  T value = T(-1);
  auto res = DecodeVarInt(it, buffer.end(), &value);
  EXPECT_EQ(0, res);
  EXPECT_EQ(T(-1), value);
}

TEST(PatchUtilsTest, EncodeDecodeVarUInt32) {
  TestEncodeDecodeVarUInt<uint32_t>({0, 64, 128, 8192, 16384, 1 << 20, 1 << 21,
                                     1 << 22, 1 << 27, 1 << 28, 0x7FFFFFFFU,
                                     UINT32_MAX - 4});
}

TEST(PatchUtilsTest, EncodeDecodeVarInt32) {
  TestEncodeDecodeVarInt<int32_t>({0, 64, 128, 8192, 16384, 1 << 20, 1 << 21,
                                   1 << 22, 1 << 27, 1 << 28, -1, INT32_MIN + 5,
                                   INT32_MAX - 4});
}

TEST(PatchUtilsTest, EncodeDecodeVarUInt64) {
  TestEncodeDecodeVarUInt<uint64_t>({0, 64, 128, 8192, 16384, 1 << 20, 1 << 21,
                                     1 << 22, 1ULL << 55, 1ULL << 56,
                                     0x7FFFFFFFFFFFFFFFULL, (UINT64_MAX - 4)});
}

TEST(PatchUtilsTest, EncodeDecodeVarInt64) {
  TestEncodeDecodeVarInt<int64_t>({0, 64, 128, 8192, 16384, 1 << 20, 1 << 21,
                                   1 << 22, 1LL << 55, 1LL << 56, -1,
                                   (INT64_MIN + 5), (INT64_MAX - 4)});
}

TEST(PatchUtilsTest, DecodeVarUInt32Malformed) {
  constexpr uint32_t kUninit = static_cast<uint32_t>(-1LL);

  // Output variable to ensure that on failure, the output variable is not
  // written to.
  uint32_t value = uint32_t(-1);

  auto TestDecodeVarInt = [&value](const std::vector<uint8_t>& buffer) {
    value = kUninit;
    return DecodeVarUInt(buffer.begin(), buffer.end(), &value);
  };

  // Exhausted.
  EXPECT_EQ(0, TestDecodeVarInt(std::vector<uint8_t>{}));
  EXPECT_EQ(kUninit, value);
  EXPECT_EQ(0, TestDecodeVarInt(std::vector<uint8_t>(4, 128)));
  EXPECT_EQ(kUninit, value);

  // Overflow.
  EXPECT_EQ(0, TestDecodeVarInt(std::vector<uint8_t>(6, 128)));
  EXPECT_EQ(kUninit, value);
  EXPECT_EQ(0, TestDecodeVarInt({128, 128, 128, 128, 128, 42}));
  EXPECT_EQ(kUninit, value);

  // Following are pathological cases that are not handled for simplicity,
  // hence decoding is expected to be successful.
  EXPECT_NE(0, TestDecodeVarInt({128, 128, 128, 128, 16}));
  EXPECT_EQ(uint32_t(0), value);
  EXPECT_NE(0, TestDecodeVarInt({128, 128, 128, 128, 32}));
  EXPECT_EQ(uint32_t(0), value);
  EXPECT_NE(0, TestDecodeVarInt({128, 128, 128, 128, 64}));
  EXPECT_EQ(uint32_t(0), value);
}

TEST(PatchUtilsTest, DecodeVarUInt64Malformed) {
  constexpr uint64_t kUninit = static_cast<uint64_t>(-1);

  uint64_t value = kUninit;
  auto TestDecodeVarInt = [&value](const std::vector<uint8_t>& buffer) {
    value = kUninit;
    return DecodeVarUInt(buffer.begin(), buffer.end(), &value);
  };

  // Exhausted.
  EXPECT_EQ(0, TestDecodeVarInt(std::vector<uint8_t>{}));
  EXPECT_EQ(kUninit, value);
  EXPECT_EQ(0, TestDecodeVarInt(std::vector<uint8_t>(9, 128)));
  EXPECT_EQ(kUninit, value);

  // Overflow.
  EXPECT_EQ(0, TestDecodeVarInt(std::vector<uint8_t>(10, 128)));
  EXPECT_EQ(kUninit, value);
  EXPECT_EQ(0, TestDecodeVarInt(
                   {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 42}));
  EXPECT_EQ(kUninit, value);
}

}  // namespace zucchini
