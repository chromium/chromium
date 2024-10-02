// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/ranges.h"

#include <stddef.h>

#include <sstream>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Human-readable output operator, for debugging/testability.
template<class T>
std::ostream& operator<<(std::ostream& os, const Ranges<T>& r) {
  os << "{ ";
  for(size_t i = 0; i < r.size(); ++i)
    os << "[" << r.start(i) << "," << r.end(i) << ") ";
  os << "}";
  return os;
}

// Helper method for asserting stringified form of |r| matches expectation.
template <class T>
static void ExpectRanges(const Ranges<T>& r, std::string_view expected_string) {
  std::stringstream ss;
  ss << r;
  ASSERT_EQ(ss.str(), expected_string);
}

#define ASSERT_RANGES(ranges, expectation) \
  ASSERT_NO_FATAL_FAILURE(ExpectRanges(ranges, expectation));

TEST(RangesTest, SimpleTests) {
  Ranges<int> r;
  ASSERT_EQ(r.size(), 0u) << r;
  ASSERT_EQ(r.Add(0, 1), 1u) << r;
  ASSERT_EQ(r.size(), 1u) << r;
  ASSERT_RANGES(r, "{ [0,1) }");
  ASSERT_EQ(r.Add(2, 3), 2u) << r;
  ASSERT_RANGES(r, "{ [0,1) [2,3) }");
  ASSERT_EQ(r.Add(1, 2), 1u) << r;
  ASSERT_RANGES(r, "{ [0,3) }");
  ASSERT_EQ(r.Add(1, 4), 1u) << r;
  ASSERT_RANGES(r, "{ [0,4) }");
  ASSERT_EQ(r.Add(7, 9), 2u) << r;
  ASSERT_EQ(r.Add(5, 6), 3u) << r;
  ASSERT_RANGES(r, "{ [0,4) [5,6) [7,9) }");
  ASSERT_EQ(r.Add(6, 7), 2u) << r;
  ASSERT_RANGES(r, "{ [0,4) [5,9) }");
}

TEST(RangesTest, ExtendRange) {
  Ranges<double> r;
  ASSERT_EQ(r.Add(0, 1), 1u) << r;
  ASSERT_EQ(r.Add(0.5, 1.5), 1u) << r;
  ASSERT_RANGES(r, "{ [0,1.5) }");

  r.clear();
  ASSERT_EQ(r.Add(0, 1), 1u) << r;
  ASSERT_EQ(r.Add(-0.5, 0.5), 1u) << r;
  ASSERT_RANGES(r, "{ [-0.5,1) }");

  r.clear();
  ASSERT_EQ(r.Add(0, 1), 1u) << r;
  ASSERT_EQ(r.Add(2, 3), 2u) << r;
  ASSERT_EQ(r.Add(4, 5), 3u) << r;
  ASSERT_EQ(r.Add(0.5, 1.5), 3u) << r;
  ASSERT_RANGES(r, "{ [0,1.5) [2,3) [4,5) }");

  r.clear();
  ASSERT_EQ(r.Add(0, 1), 1u) << r;
  ASSERT_EQ(r.Add(2, 3), 2u) << r;
  ASSERT_EQ(r.Add(4, 5), 3u) << r;
  ASSERT_EQ(r.Add(1.5, 2.5), 3u) << r;
  ASSERT_RANGES(r, "{ [0,1) [1.5,3) [4,5) }");
}

TEST(RangesTest, CoalesceRanges) {
  Ranges<double> r;
  ASSERT_EQ(r.Add(0, 1), 1u) << r;
  ASSERT_EQ(r.Add(2, 3), 2u) << r;
  ASSERT_EQ(r.Add(4, 5), 3u) << r;
  ASSERT_EQ(r.Add(0.5, 2.5), 2u) << r;
  ASSERT_RANGES(r, "{ [0,3) [4,5) }");

  r.clear();
  ASSERT_EQ(r.Add(0, 1), 1u) << r;
  ASSERT_EQ(r.Add(2, 3), 2u) << r;
  ASSERT_EQ(r.Add(4, 5), 3u) << r;
  ASSERT_EQ(r.Add(0.5, 4.5), 1u) << r;
  ASSERT_RANGES(r, "{ [0,5) }");

  r.clear();
  ASSERT_EQ(r.Add(0, 1), 1u) << r;
  ASSERT_EQ(r.Add(1, 2), 1u) << r;
  ASSERT_RANGES(r, "{ [0,2) }");
}

TEST(RangesTest, IntersectionWith) {
  Ranges<int> a;
  Ranges<int> b;

  ASSERT_EQ(a.Add(0, 1), 1u) << a;
  ASSERT_EQ(a.Add(4, 7), 2u) << a;
  ASSERT_EQ(a.Add(10, 12), 3u) << a;

  // Test intersections with an empty range.
  ASSERT_RANGES(a, "{ [0,1) [4,7) [10,12) }");
  ASSERT_RANGES(b, "{ }");
  ASSERT_RANGES(a.IntersectionWith(b), "{ }");
  ASSERT_RANGES(b.IntersectionWith(a), "{ }");

  // Test intersections with a completely overlapping range.
  ASSERT_EQ(b.Add(-1, 13), 1u) << b;
  ASSERT_RANGES(a, "{ [0,1) [4,7) [10,12) }");
  ASSERT_RANGES(b, "{ [-1,13) }");
  ASSERT_RANGES(a.IntersectionWith(b), "{ [0,1) [4,7) [10,12) }");
  ASSERT_RANGES(b.IntersectionWith(a), "{ [0,1) [4,7) [10,12) }");

  // Test intersections with a disjoint ranges.
  b.clear();
  ASSERT_EQ(b.Add(1, 4), 1u) << b;
  ASSERT_EQ(b.Add(8, 9), 2u) << b;
  ASSERT_RANGES(a, "{ [0,1) [4,7) [10,12) }");
  ASSERT_RANGES(b, "{ [1,4) [8,9) }");
  ASSERT_RANGES(a.IntersectionWith(b), "{ }");
  ASSERT_RANGES(b.IntersectionWith(a), "{ }");

  // Test intersections with partially overlapping ranges.
  b.clear();
  ASSERT_EQ(b.Add(0, 3), 1u) << b;
  ASSERT_EQ(b.Add(5, 11), 2u) << b;
  ASSERT_RANGES(a, "{ [0,1) [4,7) [10,12) }");
  ASSERT_RANGES(b, "{ [0,3) [5,11) }");
  ASSERT_RANGES(a.IntersectionWith(b), "{ [0,1) [5,7) [10,11) }");
  ASSERT_RANGES(b.IntersectionWith(a), "{ [0,1) [5,7) [10,11) }");

  // Test intersection with a range that starts at the beginning of the
  // first range and ends at the end of the last range.
  b.clear();
  ASSERT_EQ(b.Add(0, 12), 1u) << b;
  ASSERT_RANGES(a, "{ [0,1) [4,7) [10,12) }");
  ASSERT_RANGES(b, "{ [0,12) }");
  ASSERT_RANGES(a.IntersectionWith(b), "{ [0,1) [4,7) [10,12) }");
  ASSERT_RANGES(b.IntersectionWith(a), "{ [0,1) [4,7) [10,12) }");
}

}  // namespace media
