// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ----------------------------------------------------------------------
//
// Unittest for the Interval class.
//
// Author: Will Neveitt (wneveitt@google.com)
// ----------------------------------------------------------------------

#include "net/base/interval.h"

#include "base/logging.h"
#include "net/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace net {
namespace test {
namespace {

class IntervalTest : public ::testing::Test {
 protected:
  // Test intersection between the two intervals i1 and i2.  Tries
  // i1.IntersectWith(i2) and vice versa. The intersection should change i1 iff
  // changes_i1 is true, and the same for changes_i2.  The resulting
  // intersection should be result.
  void TestIntersect(const Interval<int64_t>& i1,
                     const Interval<int64_t>& i2,
                     bool changes_i1,
                     bool changes_i2,
                     const Interval<int64_t>& result) {
    Interval<int64_t> i;
    i.CopyFrom(i1);
    EXPECT_TRUE(i.IntersectWith(i2) == changes_i1 && i.Equals(result));
    i.CopyFrom(i2);
    EXPECT_TRUE(i.IntersectWith(i1) == changes_i2 && i.Equals(result));
  }
};

TEST_F(IntervalTest, ConstructorsCopyAndClear) {
  Interval<int32_t> empty;
  EXPECT_TRUE(empty.Empty());

  Interval<int32_t> d2(0, 100);
  EXPECT_EQ(0, d2.min());
  EXPECT_EQ(100, d2.max());
  EXPECT_EQ(Interval<int32_t>(0, 100), d2);
  EXPECT_NE(Interval<int32_t>(0, 99), d2);

  empty.CopyFrom(d2);
  EXPECT_EQ(0, d2.min());
  EXPECT_EQ(100, d2.max());
  EXPECT_TRUE(empty.Equals(d2));
  EXPECT_EQ(empty, d2);
  EXPECT_TRUE(d2.Equals(empty));
  EXPECT_EQ(d2, empty);

  Interval<int32_t> max_less_than_min(40, 20);
  EXPECT_TRUE(max_less_than_min.Empty());
  EXPECT_EQ(40, max_less_than_min.min());
  EXPECT_EQ(20, max_less_than_min.max());

  Interval<int> d3(10, 20);
  d3.Clear();
  EXPECT_TRUE(d3.Empty());
}

TEST_F(IntervalTest, GettersSetters) {
  Interval<int32_t> d1(100, 200);

  // SetMin:
  d1.SetMin(30);
  EXPECT_EQ(30, d1.min());
  EXPECT_EQ(200, d1.max());

  // SetMax:
  d1.SetMax(220);
  EXPECT_EQ(30, d1.min());
  EXPECT_EQ(220, d1.max());

  // Set:
  d1.Clear();
  d1.Set(30, 220);
  EXPECT_EQ(30, d1.min());
  EXPECT_EQ(220, d1.max());

  // SpanningUnion:
  Interval<int32_t> d2;
  EXPECT_TRUE(!d1.SpanningUnion(d2));
  EXPECT_EQ(30, d1.min());
  EXPECT_EQ(220, d1.max());

  EXPECT_TRUE(d2.SpanningUnion(d1));
  EXPECT_EQ(30, d2.min());
  EXPECT_EQ(220, d2.max());

  d2.SetMin(40);
  d2.SetMax(100);
  EXPECT_TRUE(!d1.SpanningUnion(d2));
  EXPECT_EQ(30, d1.min());
  EXPECT_EQ(220, d1.max());

  d2.SetMin(20);
  d2.SetMax(100);
  EXPECT_TRUE(d1.SpanningUnion(d2));
  EXPECT_EQ(20, d1.min());
  EXPECT_EQ(220, d1.max());

  d2.SetMin(50);
  d2.SetMax(300);
  EXPECT_TRUE(d1.SpanningUnion(d2));
  EXPECT_EQ(20, d1.min());
  EXPECT_EQ(300, d1.max());

  d2.SetMin(0);
  d2.SetMax(500);
  EXPECT_TRUE(d1.SpanningUnion(d2));
  EXPECT_EQ(0, d1.min());
  EXPECT_EQ(500, d1.max());

  d2.SetMin(100);
  d2.SetMax(0);
  EXPECT_TRUE(!d1.SpanningUnion(d2));
  EXPECT_EQ(0, d1.min());
  EXPECT_EQ(500, d1.max());
  EXPECT_TRUE(d2.SpanningUnion(d1));
  EXPECT_EQ(0, d2.min());
  EXPECT_EQ(500, d2.max());
}

TEST_F(IntervalTest, CoveringOps) {
  const Interval<int64_t> empty;
  const Interval<int64_t> d(100, 200);
  const Interval<int64_t> d1(0, 50);
  const Interval<int64_t> d2(50, 110);
  const Interval<int64_t> d3(110, 180);
  const Interval<int64_t> d4(180, 220);
  const Interval<int64_t> d5(220, 300);
  const Interval<int64_t> d6(100, 150);
  const Interval<int64_t> d7(150, 200);
  const Interval<int64_t> d8(0, 300);

  // Intersection:
  EXPECT_TRUE(d.Intersects(d));
  EXPECT_TRUE(!empty.Intersects(d) && !d.Intersects(empty));
  EXPECT_TRUE(!d.Intersects(d1) && !d1.Intersects(d));
  EXPECT_TRUE(d.Intersects(d2) && d2.Intersects(d));
  EXPECT_TRUE(d.Intersects(d3) && d3.Intersects(d));
  EXPECT_TRUE(d.Intersects(d4) && d4.Intersects(d));
  EXPECT_TRUE(!d.Intersects(d5) && !d5.Intersects(d));
  EXPECT_TRUE(d.Intersects(d6) && d6.Intersects(d));
  EXPECT_TRUE(d.Intersects(d7) && d7.Intersects(d));
  EXPECT_TRUE(d.Intersects(d8) && d8.Intersects(d));

  Interval<int64_t> i;
  EXPECT_TRUE(d.Intersects(d, &i) && d.Equals(i));
  EXPECT_TRUE(!empty.Intersects(d, nullptr) && !d.Intersects(empty, nullptr));
  EXPECT_TRUE(!d.Intersects(d1, nullptr) && !d1.Intersects(d, nullptr));
  EXPECT_TRUE(d.Intersects(d2, &i) && i.Equals(Interval<int64_t>(100, 110)));
  EXPECT_TRUE(d2.Intersects(d, &i) && i.Equals(Interval<int64_t>(100, 110)));
  EXPECT_TRUE(d.Intersects(d3, &i) && i.Equals(d3));
  EXPECT_TRUE(d3.Intersects(d, &i) && i.Equals(d3));
  EXPECT_TRUE(d.Intersects(d4, &i) && i.Equals(Interval<int64_t>(180, 200)));
  EXPECT_TRUE(d4.Intersects(d, &i) && i.Equals(Interval<int64_t>(180, 200)));
  EXPECT_TRUE(!d.Intersects(d5, nullptr) && !d5.Intersects(d, nullptr));
  EXPECT_TRUE(d.Intersects(d6, &i) && i.Equals(d6));
  EXPECT_TRUE(d6.Intersects(d, &i) && i.Equals(d6));
  EXPECT_TRUE(d.Intersects(d7, &i) && i.Equals(d7));
  EXPECT_TRUE(d7.Intersects(d, &i) && i.Equals(d7));
  EXPECT_TRUE(d.Intersects(d8, &i) && i.Equals(d));
  EXPECT_TRUE(d8.Intersects(d, &i) && i.Equals(d));

  // Test IntersectsWith().
  // Arguments are TestIntersect(i1, i2, changes_i1, changes_i2, result).
  TestIntersect(empty, d, false, true, empty);
  TestIntersect(d, d1, true, true, empty);
  TestIntersect(d1, d2, true, true, empty);
  TestIntersect(d, d2, true, true, Interval<int64_t>(100, 110));
  TestIntersect(d8, d, true, false, d);
  TestIntersect(d8, d1, true, false, d1);
  TestIntersect(d8, d5, true, false, d5);

  // Contains:
  EXPECT_TRUE(!empty.Contains(d) && !d.Contains(empty));
  EXPECT_TRUE(d.Contains(d));
  EXPECT_TRUE(!d.Contains(d1) && !d1.Contains(d));
  EXPECT_TRUE(!d.Contains(d2) && !d2.Contains(d));
  EXPECT_TRUE(d.Contains(d3) && !d3.Contains(d));
  EXPECT_TRUE(!d.Contains(d4) && !d4.Contains(d));
  EXPECT_TRUE(!d.Contains(d5) && !d5.Contains(d));
  EXPECT_TRUE(d.Contains(d6) && !d6.Contains(d));
  EXPECT_TRUE(d.Contains(d7) && !d7.Contains(d));
  EXPECT_TRUE(!d.Contains(d8) && d8.Contains(d));

  EXPECT_TRUE(d.Contains(100));
  EXPECT_TRUE(!d.Contains(200));
  EXPECT_TRUE(d.Contains(150));
  EXPECT_TRUE(!d.Contains(99));
  EXPECT_TRUE(!d.Contains(201));

  // Difference:
  Interval<int64_t> lo;
  Interval<int64_t> hi;

  EXPECT_TRUE(d.Difference(d2, &lo, &hi));
  EXPECT_TRUE(lo.Empty());
  EXPECT_EQ(110u, hi.min());
  EXPECT_EQ(200u, hi.max());

  EXPECT_TRUE(d.Difference(d3, &lo, &hi));
  EXPECT_EQ(100u, lo.min());
  EXPECT_EQ(110u, lo.max());
  EXPECT_EQ(180u, hi.min());
  EXPECT_EQ(200u, hi.max());

  EXPECT_TRUE(d.Difference(d4, &lo, &hi));
  EXPECT_EQ(100u, lo.min());
  EXPECT_EQ(180u, lo.max());
  EXPECT_TRUE(hi.Empty());

  EXPECT_FALSE(d.Difference(d5, &lo, &hi));
  EXPECT_EQ(100u, lo.min());
  EXPECT_EQ(200u, lo.max());
  EXPECT_TRUE(hi.Empty());

  EXPECT_TRUE(d.Difference(d6, &lo, &hi));
  EXPECT_TRUE(lo.Empty());
  EXPECT_EQ(150u, hi.min());
  EXPECT_EQ(200u, hi.max());

  EXPECT_TRUE(d.Difference(d7, &lo, &hi));
  EXPECT_EQ(100u, lo.min());
  EXPECT_EQ(150u, lo.max());
  EXPECT_TRUE(hi.Empty());

  EXPECT_TRUE(d.Difference(d8, &lo, &hi));
  EXPECT_TRUE(lo.Empty());
  EXPECT_TRUE(hi.Empty());
}

TEST_F(IntervalTest, Length) {
  const Interval<int> empty1;
  const Interval<int> empty2(1, 1);
  const Interval<int> empty3(1, 0);
  const Interval<base::TimeDelta> empty4(
      base::TimeDelta() + base::TimeDelta::FromSeconds(1), base::TimeDelta());
  const Interval<int> d1(1, 2);
  const Interval<int> d2(0, 50);
  const Interval<base::TimeDelta> d3(
      base::TimeDelta(), base::TimeDelta() + base::TimeDelta::FromSeconds(1));
  const Interval<base::TimeDelta> d4(
      base::TimeDelta() + base::TimeDelta::FromHours(1),
      base::TimeDelta() + base::TimeDelta::FromMinutes(90));

  EXPECT_EQ(0, empty1.Length());
  EXPECT_EQ(0, empty2.Length());
  EXPECT_EQ(0, empty3.Length());
  EXPECT_EQ(base::TimeDelta(), empty4.Length());
  EXPECT_EQ(1, d1.Length());
  EXPECT_EQ(50, d2.Length());
  EXPECT_EQ(base::TimeDelta::FromSeconds(1), d3.Length());
  EXPECT_EQ(base::TimeDelta::FromMinutes(30), d4.Length());
}

TEST_F(IntervalTest, IntervalOfTypeWithNoOperatorMinus) {
  // Interval<T> should work even if T does not support operator-().  We just
  // can't call Interval<T>::Length() for such types.
  const Interval<string> d1("a", "b");
  const Interval<std::pair<int, int>> d2({1, 2}, {4, 3});
  EXPECT_EQ("a", d1.min());
  EXPECT_EQ("b", d1.max());
  EXPECT_EQ(std::make_pair(1, 2), d2.min());
  EXPECT_EQ(std::make_pair(4, 3), d2.max());
}

}  // unnamed namespace
}  // namespace test
}  // namespace net
