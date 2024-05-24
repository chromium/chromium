// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/interval_map.h"

#include <stdint.h>

#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "media/base/test_random.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// Our tests only modifiy the interval map entries in [0..kTestSize).
// We need this to be big enough to hit tricky corner cases, but small
// enough that we get lots of entry duplication to clean up.
// Also, SimpleIntervalMap uses a vector of size kTestSize to emulate
// a intervalmap, so making this too big will the test down a lot.
const int kTestSize = 16;

class SimpleIntervalMap {
 public:
  SimpleIntervalMap() : data_(kTestSize) {}

  void IncrementInterval(int32_t from, int32_t to, int32_t how_much) {
    for (int32_t i = from; i < to; i++) {
      data_[i] += how_much;
    }
  }

  void SetInterval(int32_t from, int32_t to, int32_t how_much) {
    for (int32_t i = from; i < to; i++) {
      data_[i] = how_much;
    }
  }

  int32_t operator[](int32_t index) const { return data_[index]; }

 private:
  std::vector<int32_t> data_;
};

class IntervalMapTest : public testing::Test {
 public:
  IntervalMapTest() : rnd_(42) {}
  void IncrementInterval(int32_t from, int32_t to, int32_t how_much) {
    truth_.IncrementInterval(from, to, how_much);
    testee_.IncrementInterval(from, to, how_much);
    std::string message =
        base::StringPrintf("After [%d - %d) += %d", from, to, how_much);
    Compare(message);
  }

  void SetInterval(int32_t from, int32_t to, int32_t how_much) {
    truth_.SetInterval(from, to, how_much);
    testee_.SetInterval(from, to, how_much);
    std::string message =
        base::StringPrintf("After [%d - %d) += %d", from, to, how_much);
    Compare(message);
  }

  // Will exercise operator[] and IntervalMap::const_iterator.
  void Compare(const std::string& message) {
    bool had_fail = HasFailure();
    for (int i = 0; i < kTestSize; i++) {
      EXPECT_EQ(truth_[i], testee_[i]) << " i = " << i << " " << message;
    }
    EXPECT_EQ(testee_[-1], 0) << message;
    EXPECT_EQ(testee_[kTestSize], 0) << message;
    int32_t prev_ = 0;
    int32_t end_of_last_interval = 0;
    for (auto r : testee_) {
      EXPECT_LT(r.first.begin, r.first.end);
      if (r.first.begin == std::numeric_limits<int32_t>::min()) {
        EXPECT_EQ(0, r.second);
      } else {
        EXPECT_EQ(end_of_last_interval, r.first.begin);
        EXPECT_GE(r.first.begin, 0) << message;
        EXPECT_LE(r.first.begin, kTestSize) << message;
        EXPECT_NE(r.second, prev_) << message;
      }
      end_of_last_interval = r.first.end;
      prev_ = r.second;
    }
    EXPECT_EQ(prev_, 0) << message;

    if (HasFailure() && !had_fail) {
      for (int i = 0; i < kTestSize; i++) {
        LOG(ERROR) << i << ": Truth =" << truth_[i]
                   << " Testee = " << testee_[i];
      }
      for (auto r : testee_) {
        LOG(ERROR) << "Interval:  " << r.first.begin << " - " << r.first.end
                   << " = " << r.second;
      }
    }
  }

  void Clear() {
    for (int j = 0; j < kTestSize; j++) {
      IncrementInterval(j, j + 1, -truth_[j]);
    }
  }

 protected:
  media::TestRandom rnd_;
  SimpleIntervalMap truth_;
  IntervalMap<int32_t, int32_t> testee_;
};

TEST_F(IntervalMapTest, SimpleTest) {
  IncrementInterval(3, 7, 4);
  EXPECT_EQ(0, testee_[0]);
  EXPECT_EQ(0, testee_[2]);
  EXPECT_EQ(4, testee_[3]);
  EXPECT_EQ(4, testee_[5]);
  EXPECT_EQ(4, testee_[6]);
  EXPECT_EQ(0, testee_[7]);
  IncrementInterval(3, 7, -4);
  EXPECT_TRUE(testee_.empty());
}

TEST_F(IntervalMapTest, SimpleIncrementTest) {
  IncrementInterval(3, 7, 1);
  IncrementInterval(6, 10, 2);
  EXPECT_EQ(0, testee_[2]);
  EXPECT_EQ(1, testee_[3]);
  EXPECT_EQ(1, testee_[5]);
  EXPECT_EQ(3, testee_[6]);
  EXPECT_EQ(2, testee_[7]);
  EXPECT_EQ(2, testee_[9]);
  EXPECT_EQ(0, testee_[10]);
  SetInterval(3, 12, 0);
  EXPECT_TRUE(testee_.empty());
}

TEST_F(IntervalMapTest, IncrementJoinIntervalsTest) {
  IncrementInterval(3, 5, 1);
  IncrementInterval(7, 8, 1);
  IncrementInterval(9, 11, 1);
  IncrementInterval(5, 7, 1);
  IncrementInterval(8, 9, 1);
  auto i = testee_.find(5);
  EXPECT_EQ(3, i.interval_begin());
  EXPECT_EQ(11, i.interval_end());
  EXPECT_EQ(1, i.value());
}

TEST_F(IntervalMapTest, SetJoinIntervalsTest) {
  SetInterval(3, 5, 1);
  SetInterval(7, 8, 1);
  SetInterval(9, 11, 1);
  SetInterval(5, 9, 1);  // overwrites one interval
  auto i = testee_.find(5);
  EXPECT_EQ(3, i.interval_begin());
  EXPECT_EQ(11, i.interval_end());
  EXPECT_EQ(1, i.value());
}

TEST_F(IntervalMapTest, FindTest) {
  IncrementInterval(5, 6, 1);
  IncrementInterval(1, 10, 2);
  int32_t min_value = std::numeric_limits<int32_t>::min();
  int32_t max_value = std::numeric_limits<int32_t>::max();
  auto i = testee_.find(0);
  EXPECT_EQ(min_value, i.interval_begin());
  EXPECT_EQ(1, i.interval_end());
  EXPECT_EQ(0, i.value());
  i = testee_.find(4);
  EXPECT_EQ(1, i.interval_begin());
  EXPECT_EQ(5, i.interval_end());
  EXPECT_EQ(2, i.value());
  i = testee_.find(5);
  EXPECT_EQ(5, i.interval_begin());
  EXPECT_EQ(6, i.interval_end());
  EXPECT_EQ(3, i.value());
  i = testee_.find(6);
  EXPECT_EQ(6, i.interval_begin());
  EXPECT_EQ(10, i.interval_end());
  EXPECT_EQ(2, i.value());
  i = testee_.find(9);
  EXPECT_EQ(6, i.interval_begin());
  EXPECT_EQ(10, i.interval_end());
  EXPECT_EQ(2, i.value());
  i = testee_.find(10);
  EXPECT_EQ(10, i.interval_begin());
  EXPECT_EQ(max_value, i.interval_end());
  EXPECT_EQ(0, i.value());
}

TEST_F(IntervalMapTest, MinMaxInt) {
  int32_t min_value = std::numeric_limits<int32_t>::min();
  int32_t max_value = std::numeric_limits<int32_t>::max();

  // Change a single value at minint
  testee_.IncrementInterval(min_value, min_value + 1, 7);
  EXPECT_EQ(7, testee_[min_value]);
  EXPECT_EQ(0, testee_[min_value + 1]);
  auto i = testee_.find(0);
  EXPECT_EQ(min_value + 1, i.interval_begin());
  EXPECT_EQ(max_value, i.interval_end());
  EXPECT_EQ(0, i.value());
  --i;
  EXPECT_TRUE(i == testee_.find(min_value));
  EXPECT_EQ(min_value, i.interval_begin());
  EXPECT_EQ(min_value + 1, i.interval_end());
  EXPECT_EQ(7, i.value());
  testee_.clear();

  // Change a single value at maxint
  // Note that we don't actually have a way to represent a range
  // that includes maxint as the end of the interval is non-inclusive.
  testee_.IncrementInterval(max_value - 1, max_value, 7);
  EXPECT_EQ(7, testee_[max_value - 1]);
  EXPECT_EQ(0, testee_[max_value - 2]);
  i = testee_.find(0);
  EXPECT_EQ(min_value, i.interval_begin());
  EXPECT_EQ(max_value - 1, i.interval_end());
  EXPECT_EQ(0, i.value());
  ++i;
  EXPECT_TRUE(i == testee_.find(max_value - 1));
  EXPECT_EQ(max_value - 1, i.interval_begin());
  EXPECT_EQ(max_value, i.interval_end());
  EXPECT_EQ(7, i.value());

  testee_.clear();

  // Change entire range (almost)
  testee_.IncrementInterval(min_value, max_value, 17);
  EXPECT_EQ(17, testee_[min_value]);
  EXPECT_EQ(17, testee_[0]);
  EXPECT_EQ(17, testee_[max_value - 1]);
  i = testee_.find(0);
  EXPECT_EQ(min_value, i.interval_begin());
  EXPECT_EQ(max_value, i.interval_end());
  EXPECT_EQ(17, i.value());
  EXPECT_TRUE(i == testee_.find(max_value - 1));
  EXPECT_TRUE(i == testee_.find(min_value));
}

TEST_F(IntervalMapTest, RandomIncrementTest) {
  for (int j = 0; j < 200; j++) {
    Clear();
    for (int i = 0; i < 200; i++) {
      int32_t begin = rnd_.Rand() % (kTestSize - 1);
      int32_t end = begin + 1 + rnd_.Rand() % (kTestSize - begin - 1);
      IncrementInterval(begin, end, (rnd_.Rand() & 32) ? 1 : -1);
      if (HasFailure()) {
        return;
      }
    }
  }
}

TEST_F(IntervalMapTest, RandomSetTest) {
  for (int j = 0; j < 200; j++) {
    Clear();
    for (int i = 0; i < 200; i++) {
      int32_t begin = rnd_.Rand() % (kTestSize - 1);
      int32_t end = begin + 1 + rnd_.Rand() % (kTestSize - begin - 1);
      SetInterval(begin, end, rnd_.Rand() & 3);
      if (HasFailure()) {
        return;
      }
    }
  }
}

}  // namespace blink
