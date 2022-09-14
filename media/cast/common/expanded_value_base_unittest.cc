// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/common/expanded_value_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {

// A basic subclass of ExpandedValueBase to use for testing.
class TestValue : public ExpandedValueBase<int64_t, TestValue> {
 public:
  explicit TestValue(int64_t value) : ExpandedValueBase(value) {}
};

}  // namespace

// Tests the various scenarios of truncating and then re-expanding values, and
// confirming the correct behavior.  Note that, while the code below just tests
// truncation/expansion to/from 8 bits, the 16- and 32-bit cases are implicitly
// confirmed because the Expand() method uses the compiler to derive all of its
// constants (based on the type of its argument).
TEST(ExpandedValueBaseTest, TruncationAndExpansion) {
  // Test that expansion works when the reference is always equal to the value
  // that was truncated.
  for (int64_t i = -512; i <= 512; ++i) {
    const TestValue original_value(i);
    const uint8_t truncated = original_value.lower_8_bits();
    const TestValue reference(i);
    ASSERT_EQ(original_value, reference.Expand(truncated)) << "i=" << i;
  }

  // Test that expansion works when the reference is always one less than the
  // value that was truncated.
  for (int64_t i = -512; i <= 512; ++i) {
    const TestValue original_value(i);
    const uint8_t truncated = original_value.lower_8_bits();
    const TestValue reference(i - 1);
    ASSERT_EQ(original_value, reference.Expand(truncated)) << "i=" << i;
  }

  // Test that expansion works when the reference is always one greater than the
  // value that was truncated.
  for (int64_t i = -512; i <= 512; ++i) {
    const TestValue original_value(i);
    const uint8_t truncated = original_value.lower_8_bits();
    const TestValue reference(i + 1);
    ASSERT_EQ(original_value, reference.Expand(truncated)) << "i=" << i;
  }

  // Test cases where the difference between the original value and the fixed
  // reference is within the range [-128,+127].  The truncated value should
  // always be re-expanded to the original value.
  for (int64_t bias = -5; bias <= 5; ++bias) {
    for (int64_t i = -128; i <= 127; ++i) {
      const TestValue original_value(bias + i);
      const uint8_t truncated = original_value.lower_8_bits();
      const TestValue reference(bias);
      ASSERT_EQ(original_value, reference.Expand(truncated)) << "bias=" << bias
                                                             << ", i=" << i;
    }
  }

  // Test cases where the difference between the original value and the fixed
  // reference is within the range [+128,+255].  When the truncated value is
  // re-expanded, it should be 256 less than the original value.
  for (int64_t bias = -5; bias <= 5; ++bias) {
    for (int64_t i = 128; i <= 255; ++i) {
      // Example: Let |original_value| be 192.  Then, the truncated 8-bit value
      // will be 0xc0.  When a |reference| of zero is asked to expand 0xc0 back
      // to the original value, it should produce -64 since -64 is closer to
      // |reference| than 192.
      const TestValue original_value(bias + i);
      const uint8_t truncated = original_value.lower_8_bits();
      const TestValue reexpanded_value(bias + i - 256);
      ASSERT_EQ(reexpanded_value.lower_8_bits(), truncated);
      const TestValue reference(bias);
      ASSERT_EQ(reexpanded_value, reference.Expand(truncated))
          << "bias=" << bias << ", i=" << i;
    }
  }

  // Test cases where the difference between the original value and the fixed
  // reference is within the range [-256,-129].  When the truncated value is
  // re-expanded, it should be 256 more than the original value.
  for (int64_t bias = -5; bias <= 5; ++bias) {
    for (int64_t i = -256; i <= -129; ++i) {
      // Example: Let |original_value| be -192.  Then, the truncated 8-bit value
      // will be 0x40.  When a |reference| of zero is asked to expand 0x40 back
      // to the original value, it should produce 64 since 64 is closer to the
      // |reference| than -192.
      const TestValue original_value(bias + i);
      const uint8_t truncated = original_value.lower_8_bits();
      const TestValue reexpanded_value(bias + i + 256);
      ASSERT_EQ(reexpanded_value.lower_8_bits(), truncated);
      const TestValue reference(bias);
      ASSERT_EQ(reexpanded_value, reference.Expand(truncated))
          << "bias=" << bias << ", i=" << i;
    }
  }
}

}  // namespace cast
}  // namespace media
