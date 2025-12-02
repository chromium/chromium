// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/sequence.h"

#include <list>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(SequenceTest, ConcatTwoVectors) {
  std::vector<int> v1 = {1, 2};
  std::vector<int> v2 = {3, 4};

  sequence::Sequence<int> auto v12 = sequence::Concat(v1, v2);

  std::vector<int> actual = {v12.begin(), v12.end()};
  std::vector<int> expected = {1, 2, 3, 4};

  EXPECT_EQ(actual, expected);
}

TEST(SequenceTest, ConcatVecAndList) {
  std::vector<int> v1 = {1, 2};
  std::list<int> v2 = {3, 4};

  sequence::Sequence<int> auto v12 = sequence::Concat(v1, v2);

  std::vector<int> actual = {v12.begin(), v12.end()};
  std::vector<int> expected = {1, 2, 3, 4};

  EXPECT_EQ(actual, expected);
}

TEST(SequenceTest, ConcatVecAndSequence) {
  std::vector<int> v1 = {1, 2};
  std::vector<int> v2 = {3, 4};
  std::vector<int> v3 = {5, 6};

  sequence::Sequence<int> auto v12 = sequence::Concat(v1, v2);
  sequence::Sequence<int> auto v123 = sequence::Concat(v12, v3);

  std::vector<int> actual = {v123.begin(), v123.end()};
  std::vector<int> expected = {1, 2, 3, 4, 5, 6};

  EXPECT_EQ(actual, expected);
}

TEST(SequenceTest, ConcatHandlesEmptySubSequences) {
  std::vector<int> empty1;
  std::vector<int> val = {1};
  std::vector<int> empty2;

  sequence::Sequence<int> auto seq = sequence::Concat(empty1, val, empty2);

  auto it = seq.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(it, seq.end());
}

TEST(SequenceTest, ConcatHandlesMoveOnlyTypes) {
  std::vector<std::unique_ptr<int>> v1;
  v1.push_back(std::make_unique<int>(1));
  v1.push_back(std::make_unique<int>(2));

  std::vector<std::unique_ptr<int>> v2;
  v2.push_back(std::make_unique<int>(3));
  v2.push_back(std::make_unique<int>(4));

  std::vector<int> actual;
  std::vector<int> expected = {1, 2, 3, 4};
  for (const std::unique_ptr<int>& ptr : sequence::Concat(v1, v2)) {
    actual.push_back(*ptr);
  }

  EXPECT_EQ(actual, expected);
}

TEST(SequenceTest, ConcatCanUseMovedTypes) {
  std::vector<std::unique_ptr<int>> v1;
  v1.push_back(std::make_unique<int>(1));
  v1.push_back(std::make_unique<int>(2));

  std::vector<std::unique_ptr<int>> v2;
  v2.push_back(std::make_unique<int>(3));
  v2.push_back(std::make_unique<int>(4));

  std::vector<int> actual;
  std::vector<int> expected = {1, 2, 3, 4};
  for (const std::unique_ptr<int>& ptr : sequence::Concat(std::move(v1), v2)) {
    actual.push_back(*ptr);
  }

  EXPECT_EQ(actual, expected);
}

TEST(SequenceTest, ConcatCanUseSinglets) {
  std::unique_ptr<int> x = std::make_unique<int>(1);
  std::unique_ptr<int> y = std::make_unique<int>(2);

  std::vector<int> actual;
  std::vector<int> expected = {1, 2};
  for (const std::unique_ptr<int>& ptr :
       sequence::Concat(sequence::Singlet(x), sequence::Singlet(y))) {
    actual.push_back(*ptr);
  }

  EXPECT_EQ(actual, expected);
}

TEST(SequenceTest, CheckIteratorComparisons) {
  std::vector<int> x = {1, 2, 3};
  std::vector<int> y = {4, 5, 6};

  auto seq = sequence::Concat(x, y);
  auto it1 = seq.begin();
  auto it2 = seq.begin();

  ASSERT_EQ(it1, it2);
  it1++;
  ASSERT_NE(it1, it2);
  it2++;
  ASSERT_EQ(it1, it2);
  it1++;
  it1++;
  ASSERT_NE(it1, it2);
  it2++;
  ASSERT_NE(it1, it2);
  it2++;
  ASSERT_EQ(it1, it2);
  ASSERT_NE(it1, seq.end());
  it1++;
  it1++;
  it1++;
  it1++;
  ASSERT_EQ(it1, seq.end());
}

namespace {

// Helper class must be defined outside the TEST function
struct CopyableNum {
  static int copy_count;
  static int move_count;
  static int make_count;
  int n_;

  explicit CopyableNum(int n) : n_(n) { make_count++; }

  CopyableNum(const CopyableNum& o) : n_(o.n_) { copy_count++; }

  CopyableNum& operator=(const CopyableNum& o) {
    n_ = o.n_;
    copy_count++;
    return *this;
  }

  CopyableNum(CopyableNum&& o) noexcept : n_(o.n_) {
    o.n_ = 0;
    move_count++;
  }

  CopyableNum& operator=(CopyableNum&& o) noexcept {
    if (this != &o) {
      n_ = o.n_;
      o.n_ = 0;
      move_count++;
    }
    return *this;
  }

  int Get() const { return n_; }
};

int CopyableNum::copy_count = 0;
int CopyableNum::move_count = 0;
int CopyableNum::make_count = 0;

struct UncopyableObject {
 public:
  bool value;

  explicit UncopyableObject(bool v) : value(v) {}

  UncopyableObject(const UncopyableObject&) = delete;
  UncopyableObject(UncopyableObject&&);
  UncopyableObject& operator=(const UncopyableObject&) = delete;
  UncopyableObject& operator=(UncopyableObject&&) = delete;
};

struct UncopyableHolder {
  std::list<UncopyableObject> objs;

  explicit UncopyableHolder() {
    objs.emplace_back(true);
    objs.emplace_back(true);
  }

  sequence::Sequence<UncopyableObject> auto iterate() const {
    return sequence::Reference(objs);
  }
};

}  // namespace

TEST(SequenceTest, ListOfUncopyableObjects) {
  std::list<UncopyableObject> objs;
  objs.emplace_back(true);
  objs.emplace_back(true);

  for (const UncopyableObject& obj : sequence::Reference(objs)) {
    EXPECT_TRUE(obj.value);
  }

  UncopyableHolder holder;
  for (const UncopyableObject& obj : holder.iterate()) {
    EXPECT_TRUE(obj.value);
  }
}

TEST(SequenceTest, ConcatAvoidsCopiesUsingViews) {
  std::vector<CopyableNum> v1;
  v1.reserve(2);
  v1.emplace_back(1);
  v1.emplace_back(2);

  ASSERT_EQ(CopyableNum::copy_count, 0);
  ASSERT_EQ(CopyableNum::move_count, 0);
  ASSERT_EQ(CopyableNum::make_count, 2);

  std::vector<CopyableNum> v2;
  v2.emplace_back(3);
  v2.emplace_back(4);

  // v2 gets resized after adding the second element, which triggered a move.
  ASSERT_EQ(CopyableNum::copy_count, 0);
  ASSERT_EQ(CopyableNum::move_count, 1);
  ASSERT_EQ(CopyableNum::make_count, 4);

  // Reset counter
  CopyableNum::copy_count = 0;
  CopyableNum::move_count = 0;
  CopyableNum::make_count = 0;

  std::vector<int> actual;
  std::vector<int> expected = {1, 2, 3, 4};

  for (const auto& impl : sequence::Concat(v1, v2)) {
    actual.push_back(impl.Get());
  }

  // Because sequence::Concat took just rvalues, no moves or copies took place.
  EXPECT_EQ(actual, expected);
  EXPECT_EQ(CopyableNum::copy_count, 0);
  EXPECT_EQ(CopyableNum::move_count, 0);
  EXPECT_EQ(CopyableNum::make_count, 0);

  actual.clear();

  for (const auto& impl : sequence::Concat(std::move(v1), v2)) {
    actual.push_back(impl.Get());
  }

  // Still no copies or moves!
  EXPECT_EQ(actual, expected);
  EXPECT_EQ(CopyableNum::copy_count, 0);
  EXPECT_EQ(CopyableNum::move_count, 0);
  EXPECT_EQ(CopyableNum::make_count, 0);
}

sequence::Sequence<int> auto GetAlternatingSequence(bool front) {
  if (front) {
    return sequence::Concat(sequence::Singlet(1), sequence::EmptySinglet<int>(),
                            sequence::Singlet(2),
                            sequence::EmptySinglet<int>());
  } else {
    return sequence::Concat(sequence::EmptySinglet<int>(), sequence::Singlet(1),
                            sequence::EmptySinglet<int>(),
                            sequence::Singlet(2));
  }
}

TEST(SequenceTest, SingletsAndEmptyAlternate) {
  sequence::Sequence<int> auto x = GetAlternatingSequence(true);
  sequence::Sequence<int> auto y = GetAlternatingSequence(false);

  auto x_it = x.begin();
  auto y_it = y.begin();

  while (x_it != x.end() && y_it != y.end()) {
    EXPECT_EQ(*x_it, *y_it);
    x_it++;
    y_it++;
  }

  EXPECT_EQ(x_it, x.end());
  EXPECT_EQ(y_it, y.end());
}

}  // namespace media
