// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/patch_utils.h"

#include <stdint.h>

#include <iterator>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

constexpr int RADIUS = 4;

template <class T, class EncodeFunc, class DecodeFunc>
void TestEncodeDecodeImpl(const std::vector<T>& centers,
                          EncodeFunc encode_func,
                          DecodeFunc decode_func) {
  std::vector<uint8_t> buffer;

  std::vector<T> values;
  for (T center : centers) {
    // Test the neighborhood values |centers|.
    T start = center - RADIUS;
    for (int inc = 0; inc <= (RADIUS * 2); ++inc) {
      T value = start + inc;
      encode_func(value, std::back_inserter(buffer));
      values.push_back(value);
    }
  }

  auto it = buffer.begin();
  for (T expected : values) {
    T value = T(-1);  // Fill with 1-bits to ensure proper overwrite.
    auto res = decode_func(it, buffer.end(), &value);
    EXPECT_NE(0, res);
    EXPECT_EQ(expected, value);
    it += res;
  }
  EXPECT_EQ(it, buffer.end());

  T value = T(-1);
  auto res = decode_func(it, buffer.end(), &value);
  EXPECT_EQ(0, res);
  EXPECT_EQ(T(-1), value);
}

template <class T>
void TestEncodeDecodeVarUInt(const std::vector<T>& centers) {
  TestEncodeDecodeImpl<T>(
      centers, [](T value, auto dst) { return EncodeVarUInt<T>(value, dst); },
      [](auto first, auto last, T* value) {
        return DecodeVarUInt<T>(first, last, value);
      });
}

template <class T>
void TestEncodeDecodeVarInt(const std::vector<T>& centers) {
  TestEncodeDecodeImpl<T>(
      centers, [](T value, auto dst) { return EncodeVarInt<T>(value, dst); },
      [](auto first, auto last, T* value) {
        return DecodeVarInt<T>(first, last, value);
      });
}

template <class T>
void PushPowersOf2AndNegations(int lo_bit,
                               int hi_bit,
                               T subtract_from,
                               std::vector<T>* out) {
  for (int bit = lo_bit; bit <= hi_bit; ++bit) {
    T v = static_cast<T>(1) << bit;
    DCHECK_GT(v, static_cast<T>(RADIUS));
    out->push_back(v);
    out->push_back(subtract_from - v);
  }
}

// "Center" values of EncodeDecode*() tests are chosen to avoid underflow /
// overflow when shifted by [-RADIUS, RADIUS].
TEST(PatchUtilsTest, EncodeDecodeVarUInt32) {
  std::vector<uint32_t> centers = {RADIUS, UINT32_MAX - RADIUS};
  PushPowersOf2AndNegations<uint32_t>(3, 30, UINT32_MAX, &centers);
  centers.push_back(UINT32_MAX >> 1);
  TestEncodeDecodeVarUInt<uint32_t>(centers);
}

TEST(PatchUtilsTest, EncodeDecodeVarInt32) {
  std::vector<int32_t> centers = {RADIUS, -1 - RADIUS};
  PushPowersOf2AndNegations<int32_t>(3, 30, 0, &centers);
  centers.push_back(INT32_MIN + RADIUS);
  centers.push_back(INT32_MAX - RADIUS);
  TestEncodeDecodeVarInt<int32_t>(centers);
}

TEST(PatchUtilsTest, EncodeDecodeVarUInt64) {
  std::vector<uint64_t> centers = {RADIUS, UINT64_MAX - RADIUS};
  PushPowersOf2AndNegations<uint64_t>(3, 62, UINT64_MAX, &centers);
  centers.push_back(UINT64_MAX >> 1);
  TestEncodeDecodeVarUInt<uint64_t>(centers);
}

TEST(PatchUtilsTest, EncodeDecodeVarInt64) {
  std::vector<int64_t> centers = {RADIUS, -1LL - RADIUS};
  PushPowersOf2AndNegations<int64_t>(3, 62, 0LL, &centers);
  centers.push_back(INT64_MIN + RADIUS);
  centers.push_back(INT64_MAX - RADIUS);
  TestEncodeDecodeVarInt<int64_t>(centers);
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
