// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "media/video/half_float_maker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class HalfFloatMakerTest : public testing::Test {};

// Convert an IEEE 754 half-float to a double value
// that we can do math on.
double FromHalfFloat(uint16_t half_float) {
  if (!half_float)
    return 0.0;
  int sign = (half_float & 0x8000) ? -1 : 1;
  int exponent = (half_float >> 10) & 0x1F;
  int fraction = half_float & 0x3FF;
  if (exponent == 0) {
    return pow(2.0, -24.0) * fraction;
  } else if (exponent == 0x1F) {
    return sign * 1000000000000.0;
  } else {
    return pow(2.0, exponent - 25) * (0x400 + fraction);
  }
}

TEST_F(HalfFloatMakerTest, MakeHalfFloatTest) {
  unsigned short integers[1 << 16];
  unsigned short half_floats[1 << 16];
  for (int bits = 9; bits <= 16; bits++) {
    std::unique_ptr<media::HalfFloatMaker> half_float_maker;
    half_float_maker = media::HalfFloatMaker::NewHalfFloatMaker(bits);
    int num_values = 1 << bits;
    for (int i = 0; i < num_values; i++)
      integers[i] = i;

    half_float_maker->MakeHalfFloats(integers, num_values, half_floats);
    // Multiplier to converting integers to 0.0..1.0 range.
    double multiplier = 1.0 / (num_values - 1);

    for (int i = 0; i < num_values; i++) {
      // This value is in range 0..1
      float value = integers[i] * multiplier;
      // Reverse the effect of offset and multiplier to get the expected
      // output value from the half-float converter.
      float expected_value =
          value / half_float_maker->Multiplier() + half_float_maker->Offset();
      EXPECT_EQ(integers[i], i);

      // We expect the result to be within +/- one least-significant bit.
      // Within the range we care about, half-floats values and
      // their representation both sort in the same order, so we
      // can just add one to get the next bigger half-float.
      float expected_precision =
          FromHalfFloat(half_floats[i] + 1) - FromHalfFloat(half_floats[i]);
      EXPECT_NEAR(FromHalfFloat(half_floats[i]), expected_value,
                  expected_precision)
          << "i = " << i << " bits = " << bits;
    }
  }
}

}  // namespace media
