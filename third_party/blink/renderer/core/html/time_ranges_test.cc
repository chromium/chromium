/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/time_ranges.h"

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

static std::string ToString(const TimeRanges& ranges) {
  std::stringstream ss;
  ss << "{";
  for (unsigned i = 0; i < ranges.length(); ++i) {
    ss << " [" << ranges.start(i, IGNORE_EXCEPTION_FOR_TESTING) << ","
       << ranges.end(i, IGNORE_EXCEPTION_FOR_TESTING) << ")";
  }
  ss << " }";

  return ss.str();
}

#define ASSERT_RANGE(expected, range) ASSERT_EQ(expected, ToString(*range))

TEST(TimeRangesTest, Empty) {
  test::TaskEnvironment task_environment;
  ASSERT_RANGE("{ }", MakeGarbageCollected<TimeRanges>());
}

TEST(TimeRangesTest, SingleRange) {
  test::TaskEnvironment task_environment;
  ASSERT_RANGE("{ [1,2) }", MakeGarbageCollected<TimeRanges>(1, 2));
}

TEST(TimeRangesTest, CreateFromWebTimeRanges) {
  test::TaskEnvironment task_environment;
  blink::WebTimeRanges web_ranges(static_cast<size_t>(2));
  web_ranges[0].start = 0;
  web_ranges[0].end = 1;
  web_ranges[1].start = 2;
  web_ranges[1].end = 3;
  ASSERT_RANGE("{ [0,1) [2,3) }", MakeGarbageCollected<TimeRanges>(web_ranges));
}

TEST(TimeRangesTest, AddOrder) {
  test::TaskEnvironment task_environment;
  auto* range_a = MakeGarbageCollected<TimeRanges>();
  auto* range_b = MakeGarbageCollected<TimeRanges>();

  range_a->Add(0, 2);
  range_a->Add(3, 4);
  range_a->Add(5, 100);

  std::string expected = "{ [0,2) [3,4) [5,100) }";
  ASSERT_RANGE(expected, range_a);

  // Add the values in rangeA to rangeB in reverse order.
  for (int i = range_a->length() - 1; i >= 0; --i) {
    range_b->Add(range_a->start(i, IGNORE_EXCEPTION_FOR_TESTING),
                 range_a->end(i, IGNORE_EXCEPTION_FOR_TESTING));
  }

  ASSERT_RANGE(expected, range_b);
}

TEST(TimeRangesTest, OverlappingAdds) {
  test::TaskEnvironment task_environment;
  auto* ranges = MakeGarbageCollected<TimeRanges>();

  ranges->Add(0, 2);
  ranges->Add(10, 11);
  ASSERT_RANGE("{ [0,2) [10,11) }", ranges);

  ranges->Add(0, 2);
  ASSERT_RANGE("{ [0,2) [10,11) }", ranges);

  ranges->Add(2, 3);
  ASSERT_RANGE("{ [0,3) [10,11) }", ranges);

  ranges->Add(2, 6);
  ASSERT_RANGE("{ [0,6) [10,11) }", ranges);

  ranges->Add(9, 10);
  ASSERT_RANGE("{ [0,6) [9,11) }", ranges);

  ranges->Add(8, 10);
  ASSERT_RANGE("{ [0,6) [8,11) }", ranges);

  ranges->Add(-1, 7);
  ASSERT_RANGE("{ [-1,7) [8,11) }", ranges);

  ranges->Add(6, 9);
  ASSERT_RANGE("{ [-1,11) }", ranges);
}

TEST(TimeRangesTest, IntersectWith_Self) {
  test::TaskEnvironment task_environment;
  auto* ranges = MakeGarbageCollected<TimeRanges>(0, 2);

  ASSERT_RANGE("{ [0,2) }", ranges);

  ranges->IntersectWith(ranges);

  ASSERT_RANGE("{ [0,2) }", ranges);
}

TEST(TimeRangesTest, IntersectWith_IdenticalRange) {
  test::TaskEnvironment task_environment;
  auto* ranges_a = MakeGarbageCollected<TimeRanges>(0, 2);
  auto* ranges_b = ranges_a->Copy();

  ASSERT_RANGE("{ [0,2) }", ranges_a);
  ASSERT_RANGE("{ [0,2) }", ranges_b);

  ranges_a->IntersectWith(ranges_b);

  ASSERT_RANGE("{ [0,2) }", ranges_a);
  ASSERT_RANGE("{ [0,2) }", ranges_b);
}

TEST(TimeRangesTest, IntersectWith_Empty) {
  test::TaskEnvironment task_environment;
  auto* ranges_a = MakeGarbageCollected<TimeRanges>(0, 2);
  auto* ranges_b = MakeGarbageCollected<TimeRanges>();

  ASSERT_RANGE("{ [0,2) }", ranges_a);
  ASSERT_RANGE("{ }", ranges_b);

  ranges_a->IntersectWith(ranges_b);

  ASSERT_RANGE("{ }", ranges_a);
  ASSERT_RANGE("{ }", ranges_b);
}

TEST(TimeRangesTest, IntersectWith_DisjointRanges1) {
  test::TaskEnvironment task_environment;
  auto* ranges_a = MakeGarbageCollected<TimeRanges>();
  auto* ranges_b = MakeGarbageCollected<TimeRanges>();

  ranges_a->Add(0, 1);
  ranges_a->Add(4, 5);

  ranges_b->Add(2, 3);
  ranges_b->Add(6, 7);

  ASSERT_RANGE("{ [0,1) [4,5) }", ranges_a);
  ASSERT_RANGE("{ [2,3) [6,7) }", ranges_b);

  ranges_a->IntersectWith(ranges_b);

  ASSERT_RANGE("{ }", ranges_a);
  ASSERT_RANGE("{ [2,3) [6,7) }", ranges_b);
}

TEST(TimeRangesTest, IntersectWith_DisjointRanges2) {
  test::TaskEnvironment task_environment;
  auto* ranges_a = MakeGarbageCollected<TimeRanges>();
  auto* ranges_b = MakeGarbageCollected<TimeRanges>();

  ranges_a->Add(0, 1);
  ranges_a->Add(4, 5);

  ranges_b->Add(1, 4);
  ranges_b->Add(5, 7);

  ASSERT_RANGE("{ [0,1) [4,5) }", ranges_a);
  ASSERT_RANGE("{ [1,4) [5,7) }", ranges_b);

  ranges_a->IntersectWith(ranges_b);

  ASSERT_RANGE("{ }", ranges_a);
  ASSERT_RANGE("{ [1,4) [5,7) }", ranges_b);
}

TEST(TimeRangesTest, IntersectWith_CompleteOverlap1) {
  test::TaskEnvironment task_environment;
  auto* ranges_a = MakeGarbageCollected<TimeRanges>();
  auto* ranges_b = MakeGarbageCollected<TimeRanges>();

  ranges_a->Add(1, 3);
  ranges_a->Add(4, 5);
  ranges_a->Add(6, 9);

  ranges_b->Add(0, 10);

  ASSERT_RANGE("{ [1,3) [4,5) [6,9) }", ranges_a);
  ASSERT_RANGE("{ [0,10) }", ranges_b);

  ranges_a->IntersectWith(ranges_b);

  ASSERT_RANGE("{ [1,3) [4,5) [6,9) }", ranges_a);
  ASSERT_RANGE("{ [0,10) }", ranges_b);
}

TEST(TimeRangesTest, IntersectWith_CompleteOverlap2) {
  test::TaskEnvironment task_environment;
  auto* ranges_a = MakeGarbageCollected<TimeRanges>();
  auto* ranges_b = MakeGarbageCollected<TimeRanges>();

  ranges_a->Add(1, 3);
  ranges_a->Add(4, 5);
  ranges_a->Add(6, 9);

  ranges_b->Add(1, 9);

  ASSERT_RANGE("{ [1,3) [4,5) [6,9) }", ranges_a);
  ASSERT_RANGE("{ [1,9) }", ranges_b);

  ranges_a->IntersectWith(ranges_b);

  ASSERT_RANGE("{ [1,3) [4,5) [6,9) }", ranges_a);
  ASSERT_RANGE("{ [1,9) }", ranges_b);
}

TEST(TimeRangesTest, IntersectWith_Gaps1) {
  test::TaskEnvironment task_environment;
  auto* ranges_a = MakeGarbageCollected<TimeRanges>();
  auto* ranges_b = MakeGarbageCollected<TimeRanges>();

  ranges_a->Add(0, 2);
  ranges_a->Add(4, 6);

  ranges_b->Add(1, 5);

  ASSERT_RANGE("{ [0,2) [4,6) }", ranges_a);
  ASSERT_RANGE("{ [1,5) }", ranges_b);

  ranges_a->IntersectWith(ranges_b);

  ASSERT_RANGE("{ [1,2) [4,5) }", ranges_a);
  ASSERT_RANGE("{ [1,5) }", ranges_b);
}

TEST(TimeRangesTest, IntersectWith_Gaps2) {
  test::TaskEnvironment task_environment;
  auto* ranges_a = MakeGarbageCollected<TimeRanges>();
  auto* ranges_b = MakeGarbageCollected<TimeRanges>();

  ranges_a->Add(0, 2);
  ranges_a->Add(4, 6);
  ranges_a->Add(8, 10);

  ranges_b->Add(1, 9);

  ASSERT_RANGE("{ [0,2) [4,6) [8,10) }", ranges_a);
  ASSERT_RANGE("{ [1,9) }", ranges_b);

  ranges_a->IntersectWith(ranges_b);

  ASSERT_RANGE("{ [1,2) [4,6) [8,9) }", ranges_a);
  ASSERT_RANGE("{ [1,9) }", ranges_b);
}

TEST(TimeRangesTest, IntersectWith_Gaps3) {
  test::TaskEnvironment task_environment;
  auto* ranges_a = MakeGarbageCollected<TimeRanges>();
  auto* ranges_b = MakeGarbageCollected<TimeRanges>();

  ranges_a->Add(0, 2);
  ranges_a->Add(4, 7);
  ranges_a->Add(8, 10);

  ranges_b->Add(1, 5);
  ranges_b->Add(6, 9);

  ASSERT_RANGE("{ [0,2) [4,7) [8,10) }", ranges_a);
  ASSERT_RANGE("{ [1,5) [6,9) }", ranges_b);

  ranges_a->IntersectWith(ranges_b);

  ASSERT_RANGE("{ [1,2) [4,5) [6,7) [8,9) }", ranges_a);
  ASSERT_RANGE("{ [1,5) [6,9) }", ranges_b);
}

TEST(TimeRangesTest, Nearest) {
  test::TaskEnvironment task_environment;
  auto* ranges = MakeGarbageCollected<TimeRanges>();
  ranges->Add(0, 2);
  ranges->Add(5, 7);

  ASSERT_EQ(0, ranges->Nearest(0, 0));
  ASSERT_EQ(1, ranges->Nearest(1, 0));
  ASSERT_EQ(2, ranges->Nearest(2, 0));
  ASSERT_EQ(2, ranges->Nearest(3, 0));
  ASSERT_EQ(5, ranges->Nearest(4, 0));
  ASSERT_EQ(5, ranges->Nearest(5, 0));
  ASSERT_EQ(7, ranges->Nearest(8, 0));

  ranges->Add(9, 11);
  ASSERT_EQ(7, ranges->Nearest(8, 6));
  ASSERT_EQ(7, ranges->Nearest(8, 8));
  ASSERT_EQ(9, ranges->Nearest(8, 10));
}

}  // namespace blink
