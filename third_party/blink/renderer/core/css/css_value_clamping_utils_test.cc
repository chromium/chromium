#include "third_party/blink/renderer/core/css/css_value_clamping_utils.h"

#include <limits>
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSValueClampingTest, IsLengthNotClampedZeroValue) {
  EXPECT_EQ(CSSValueClampingUtils::ClampLength(0.0), 0.0);
}

TEST(CSSValueClampingTest, IsLengthNotClampedPositiveFiniteValue) {
  EXPECT_EQ(CSSValueClampingUtils::ClampLength(10.0), 10.0);
}

TEST(CSSValueClampingTest, IsLengthNotClampediNegativeFiniteValue) {
  EXPECT_EQ(CSSValueClampingUtils::ClampLength(-10.0), -10.0);
}

TEST(CSSValueClampingTest, IsLengthClampedPositiveInfinity) {
  EXPECT_EQ(CSSValueClampingUtils::ClampLength(
                std::numeric_limits<double>::infinity()),
            std::numeric_limits<double>::max());
}

TEST(CSSValueClampingTest, IsLengthClampedNaN) {
  EXPECT_EQ(CSSValueClampingUtils::ClampLength(
                std::numeric_limits<double>::quiet_NaN()),
            0.0);
}

TEST(CSSValueClampingTest, IsLengthClampedNegativeInfinity) {
  EXPECT_EQ(CSSValueClampingUtils::ClampLength(
                -std::numeric_limits<double>::infinity()),
            std::numeric_limits<double>::lowest());
}

}  // namespace blink
