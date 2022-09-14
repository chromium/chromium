// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/vp8_bool_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

const static size_t NUM_BITS_TO_TEST = 100;

namespace {

// 100 zeros with probability of 0x80.
const uint8_t kDataZerosAndEvenProbabilities[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00};

// 100 ones with probability of 0x80.
const uint8_t kDataOnesAndEvenProbabilities[] = {
    0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xf0, 0x20};

// [0, 1, 0, 1, ..., 1] with probability [0, 1, 2, 3, ..., 99].
const uint8_t kDataParitiesAndIncreasingProbabilities[] = {
    0x00, 0x02, 0x08, 0x31, 0x8e, 0xca, 0xab, 0xe2, 0xc8, 0x31, 0x12,
    0xb3, 0x2c, 0x19, 0x90, 0xc6, 0x6a, 0xeb, 0x17, 0x52, 0x30};

}  // namespace

class Vp8BoolDecoderTest : public ::testing::Test {
 public:
  Vp8BoolDecoderTest() = default;

  Vp8BoolDecoderTest(const Vp8BoolDecoderTest&) = delete;
  Vp8BoolDecoderTest& operator=(const Vp8BoolDecoderTest&) = delete;

 protected:
  // Fixture member, the bool decoder to be tested.
  Vp8BoolDecoder bd_;
};

#define INITIALIZE(data) ASSERT_TRUE(bd_.Initialize(data, sizeof(data)));

TEST_F(Vp8BoolDecoderTest, DecodeBoolsWithZerosAndEvenProbabilities) {
  INITIALIZE(kDataZerosAndEvenProbabilities);
  ASSERT_EQ(0u, bd_.BitOffset());
  for (size_t i = 0; i < NUM_BITS_TO_TEST; ++i) {
    bool out = true;
    ASSERT_TRUE(bd_.ReadBool(&out, 0x80));
    ASSERT_FALSE(out);
    ASSERT_EQ(i, bd_.BitOffset());
  }
}

TEST_F(Vp8BoolDecoderTest, DecodeLiteralsWithZerosAndEvenProbabilities) {
  INITIALIZE(kDataZerosAndEvenProbabilities);

  int value = 1;
  ASSERT_TRUE(bd_.ReadLiteral(1, &value));
  ASSERT_EQ(0, value);

  value = 1;
  ASSERT_TRUE(bd_.ReadLiteral(32, &value));
  ASSERT_EQ(0, value);

  value = 1;
  ASSERT_TRUE(bd_.ReadLiteralWithSign(1, &value));
  ASSERT_EQ(0, value);

  value = 1;
  ASSERT_TRUE(bd_.ReadLiteralWithSign(31, &value));
  ASSERT_EQ(0, value);
}

TEST_F(Vp8BoolDecoderTest, DecodeBoolsWithOnesAndEvenProbabilities) {
  INITIALIZE(kDataOnesAndEvenProbabilities);

  ASSERT_EQ(0u, bd_.BitOffset());
  for (size_t i = 0; i < NUM_BITS_TO_TEST; ++i) {
    bool out = false;
    ASSERT_TRUE(bd_.ReadBool(&out, 0x80));
    ASSERT_TRUE(out);
    ASSERT_EQ(i + 1, bd_.BitOffset());
  }
}

TEST_F(Vp8BoolDecoderTest, DecodeLiteralsWithOnesAndEvenProbabilities) {
  INITIALIZE(kDataOnesAndEvenProbabilities);

  int value = 0;
  ASSERT_TRUE(bd_.ReadLiteral(1, &value));
  EXPECT_EQ(1, value);

  value = 0;
  ASSERT_TRUE(bd_.ReadLiteral(31, &value));
  EXPECT_EQ(0x7FFFFFFF, value);

  value = 0;
  ASSERT_TRUE(bd_.ReadLiteralWithSign(1, &value));
  EXPECT_EQ(-1, value);

  value = 0;
  ASSERT_TRUE(bd_.ReadLiteralWithSign(31, &value));
  EXPECT_EQ(-0x7FFFFFFF, value);
}

TEST_F(Vp8BoolDecoderTest, DecodeBoolsWithParitiesAndIncreasingProbabilities) {
  INITIALIZE(kDataParitiesAndIncreasingProbabilities);

  for (size_t i = 0; i < NUM_BITS_TO_TEST; ++i) {
    bool out = !(i & 1);
    ASSERT_LE(i, std::numeric_limits<uint8_t>::max());
    ASSERT_TRUE(bd_.ReadBool(&out, static_cast<uint8_t>(i)));
    EXPECT_EQ(out, !!(i & 1));
  }
}

}  // namespace media
