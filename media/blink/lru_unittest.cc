// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <list>

#include "base/logging.h"
#include "media/base/test_random.h"
#include "media/blink/lru.h"
#include "testing/gtest/include/gtest/gtest.h"

// Range of integer used in tests below.
// We keep the integers small to get lots of re-use of integers.
const int kTestIntRange = 16;

namespace media {

class LRUTest;

class SimpleLRU {
 public:
  void Insert(int x) {
    DCHECK(!Contains(x));
    data_.push_back(x);
  }

  void Remove(int x) {
    for (auto i = data_.begin(); i != data_.end(); ++i) {
      if (*i == x) {
        data_.erase(i);
        DCHECK(!Contains(x));
        return;
      }
    }
    LOG(FATAL) << "Remove non-existing element " << x;
  }

  void Use(int x) {
    if (Contains(x))
      Remove(x);
    Insert(x);
  }

  bool Empty() const { return data_.empty(); }

  int Pop() {
    DCHECK(!Empty());
    int ret = data_.front();
    data_.pop_front();
    return ret;
  }

  int Peek() {
    DCHECK(!Empty());
    return data_.front();
  }

  bool Contains(int x) const {
    for (auto i = data_.begin(); i != data_.end(); ++i) {
      if (*i == x) {
        return true;
      }
    }
    return false;
  }

  size_t Size() const { return data_.size(); }

 private:
  friend class LRUTest;
  std::list<int> data_;
};

class LRUTest : public testing::Test {
 public:
  LRUTest() : rnd_(42) {}

  void Insert(int x) {
    truth_.Insert(x);
    testee_.Insert(x);
    Compare();
  }

  void Remove(int x) {
    truth_.Remove(x);
    testee_.Remove(x);
    Compare();
  }

  void Use(int x) {
    truth_.Use(x);
    testee_.Use(x);
    Compare();
  }

  int Pop() {
    int truth_value = truth_.Pop();
    int testee_value = testee_.Pop();
    EXPECT_EQ(truth_value, testee_value);
    Compare();
    return truth_value;
  }

  void Compare() {
    EXPECT_EQ(truth_.Size(), testee_.Size());
    auto testee_iterator = testee_.lru_.rbegin();
    for (const auto truth : truth_.data_) {
      EXPECT_TRUE(testee_iterator != testee_.lru_.rend());
      EXPECT_EQ(truth, *testee_iterator);
      ++testee_iterator;
    }
    EXPECT_TRUE(testee_iterator == testee_.lru_.rend());
  }

  bool Empty() const {
    EXPECT_EQ(truth_.Empty(), testee_.Empty());
    return truth_.Empty();
  }

  bool Contains(int i) const {
    EXPECT_EQ(truth_.Contains(i), testee_.Contains(i));
    return testee_.Contains(i);
  }

  void Clear() {
    while (!Empty())
      Pop();
  }

  int Peek() {
    EXPECT_EQ(truth_.Peek(), testee_.Peek());
    return testee_.Peek();
  }

 protected:
  media::TestRandom rnd_;
  SimpleLRU truth_;
  media::LRU<int> testee_;
};

TEST_F(LRUTest, SimpleTest) {
  Insert(1);  // 1
  Insert(2);  // 1 2
  Insert(3);  // 1 2 3
  EXPECT_EQ(1, Peek());
  EXPECT_EQ(1, Pop());  // 2 3
  EXPECT_EQ(2, Peek());
  Use(2);  // 3 2
  EXPECT_EQ(3, Peek());
  EXPECT_EQ(3, Pop());  // 2
  EXPECT_EQ(2, Pop());
  EXPECT_TRUE(Empty());
}

TEST_F(LRUTest, UseTest) {
  EXPECT_TRUE(Empty());
  // Using a value that's not on the LRU adds it.
  Use(3);  // 3
  EXPECT_EQ(3, Peek());
  Use(5);  // 3 5
  EXPECT_EQ(3, Peek());
  EXPECT_TRUE(Contains(5));
  Use(7);  // 3 5 7
  EXPECT_EQ(3, Peek());
  EXPECT_TRUE(Contains(7));
  // Using a value that's alraedy on the LRU moves it to the top.
  Use(3);  // 5 7 3
  EXPECT_EQ(5, Peek());
  EXPECT_TRUE(Contains(5));
  EXPECT_EQ(5, Pop());  // 7 3
  EXPECT_FALSE(Contains(5));
  EXPECT_EQ(7, Peek());
  EXPECT_TRUE(Contains(7));
  EXPECT_TRUE(Contains(3));
  Use(9);  // 7 3 9
  EXPECT_EQ(7, Peek());
  // Using the same value again has no effect.
  Use(9);  // 7 3 9
  EXPECT_EQ(7, Peek());
  Use(3);  // 7 9 3
  EXPECT_EQ(7, Pop());
  EXPECT_EQ(9, Pop());
  EXPECT_EQ(3, Pop());
  EXPECT_TRUE(Empty());
}

TEST_F(LRUTest, RemoveTest) {
  Insert(5);  // 5
  Insert(4);  // 5 4
  Insert(3);  // 5 4 3
  Insert(2);  // 5 4 3 2
  Insert(1);  // 5 4 3 2 1
  EXPECT_EQ(5, Peek());
  Remove(5);  // 4 3 2 1
  EXPECT_EQ(4, Peek());
  Remove(1);  // 4 3 2
  EXPECT_EQ(4, Peek());
  Remove(3);  // 4 2
  EXPECT_EQ(4, Pop());
  EXPECT_EQ(2, Pop());
  EXPECT_TRUE(Empty());
}

TEST_F(LRUTest, RandomTest) {
  for (int j = 0; j < 100; j++) {
    Clear();
    for (int i = 0; i < 1000; i++) {
      int value = rnd_.Rand() % kTestIntRange;
      switch (rnd_.Rand() % 3) {
        case 0:
          if (!Empty())
            Pop();
          break;

        case 1:
          Use(value);
          break;

        case 2:
          if (Contains(value)) {
            Remove(value);
          } else {
            Insert(value);
          }
          break;
      }
      if (HasFailure()) {
        return;
      }
    }
  }
}

}  // namespace media
