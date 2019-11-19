// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/container/btree_test.h"

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/internal/counting_allocator.h"
#include "absl/container/internal/test_instance_tracker.h"
#include "absl/flags/flag.h"
#include "absl/hash/hash_testing.h"
#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/compare.h"

ABSL_FLAG(int, test_values, 10000, "The number of values to use for tests");

namespace absl {
namespace container_internal {
namespace {

using ::absl::test_internal::InstanceTracker;
using ::absl::test_internal::MovableOnlyInstance;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Pair;

template <typename T, typename U>
void CheckPairEquals(const T &x, const U &y) {
  ABSL_INTERNAL_CHECK(x == y, "Values are unequal.");
}

template <typename T, typename U, typename V, typename W>
void CheckPairEquals(const std::pair<T, U> &x, const std::pair<V, W> &y) {
  CheckPairEquals(x.first, y.first);
  CheckPairEquals(x.second, y.second);
}
}  // namespace

// The base class for a sorted associative container checker. TreeType is the
// container type to check and CheckerType is the container type to check
// against. TreeType is expected to be btree_{set,map,multiset,multimap} and
// CheckerType is expected to be {set,map,multiset,multimap}.
template <typename TreeType, typename CheckerType>
class base_checker {
 public:
  using key_type = typename TreeType::key_type;
  using value_type = typename TreeType::value_type;
  using key_compare = typename TreeType::key_compare;
  using pointer = typename TreeType::pointer;
  using const_pointer = typename TreeType::const_pointer;
  using reference = typename TreeType::reference;
  using const_reference = typename TreeType::const_reference;
  using size_type = typename TreeType::size_type;
  using difference_type = typename TreeType::difference_type;
  using iterator = typename TreeType::iterator;
  using const_iterator = typename TreeType::const_iterator;
  using reverse_iterator = typename TreeType::reverse_iterator;
  using const_reverse_iterator = typename TreeType::const_reverse_iterator;

 public:
  base_checker() : const_tree_(tree_) {}
  base_checker(const base_checker &x)
      : tree_(x.tree_), const_tree_(tree_), checker_(x.checker_) {}
  template <typename InputIterator>
  base_checker(InputIterator b, InputIterator e)
      : tree_(b, e), const_tree_(tree_), checker_(b, e) {}

  iterator begin() { return tree_.begin(); }
  const_iterator begin() const { return tree_.begin(); }
  iterator end() { return tree_.end(); }
  const_iterator end() const { return tree_.end(); }
  reverse_iterator rbegin() { return tree_.rbegin(); }
  const_reverse_iterator rbegin() const { return tree_.rbegin(); }
  reverse_iterator rend() { return tree_.rend(); }
  const_reverse_iterator rend() const { return tree_.rend(); }

  template <typename IterType, typename CheckerIterType>
  IterType iter_check(IterType tree_iter, CheckerIterType checker_iter) const {
    if (tree_iter == tree_.end()) {
      ABSL_INTERNAL_CHECK(checker_iter == checker_.end(),
                          "Checker iterator not at end.");
    } else {
      CheckPairEquals(*tree_iter, *checker_iter);
    }
    return tree_iter;
  }
  template <typename IterType, typename CheckerIterType>
  IterType riter_check(IterType tree_iter, CheckerIterType checker_iter) const {
    if (tree_iter == tree_.rend()) {
      ABSL_INTERNAL_CHECK(checker_iter == checker_.rend(),
                          "Checker iterator not at rend.");
    } else {
      CheckPairEquals(*tree_iter, *checker_iter);
    }
    return tree_iter;
  }
  void value_check(const value_type &x) {
    typename KeyOfValue<typename TreeType::key_type,
                        typename TreeType::value_type>::type key_of_value;
    const key_type &key = key_of_value(x);
    CheckPairEquals(*find(key), x);
    lower_bound(key);
    upper_bound(key);
    equal_range(key);
    contains(key);
    count(key);
  }
  void erase_check(const key_type &key) {
    EXPECT_FALSE(tree_.contains(key));
    EXPECT_EQ(tree_.find(key), const_tree_.end());
    EXPECT_FALSE(const_tree_.contains(key));
    EXPECT_EQ(const_tree_.find(key), tree_.end());
    EXPECT_EQ(tree_.equal_range(key).first,
              const_tree_.equal_range(key).second);
  }

  iterator lower_bound(const key_type &key) {
    return iter_check(tree_.lower_bound(key), checker_.lower_bound(key));
  }
  const_iterator lower_bound(const key_type &key) const {
    return iter_check(tree_.lower_bound(key), checker_.lower_bound(key));
  }
  iterator upper_bound(const key_type &key) {
    return iter_check(tree_.upper_bound(key), checker_.upper_bound(key));
  }
  const_iterator upper_bound(const key_type &key) const {
    return iter_check(tree_.upper_bound(key), checker_.upper_bound(key));
  }
  std::pair<iterator, iterator> equal_range(const key_type &key) {
    std::pair<typename CheckerType::iterator, typename CheckerType::iterator>
        checker_res = checker_.equal_range(key);
    std::pair<iterator, iterator> tree_res = tree_.equal_range(key);
    iter_check(tree_res.first, checker_res.first);
    iter_check(tree_res.second, checker_res.second);
    return tree_res;
  }
  std::pair<const_iterator, const_iterator> equal_range(
      const key_type &key) const {
    std::pair<typename CheckerType::const_iterator,
              typename CheckerType::const_iterator>
        checker_res = checker_.equal_range(key);
    std::pair<const_iterator, const_iterator> tree_res = tree_.equal_range(key);
    iter_check(tree_res.first, checker_res.first);
    iter_check(tree_res.second, checker_res.second);
    return tree_res;
  }
  iterator find(const key_type &key) {
    return iter_check(tree_.find(key), checker_.find(key));
  }
  const_iterator find(const key_type &key) const {
    return iter_check(tree_.find(key), checker_.find(key));
  }
  bool contains(const key_type &key) const {
    return find(key) != end();
  }
  size_type count(const key_type &key) const {
    size_type res = checker_.count(key);
    EXPECT_EQ(res, tree_.count(key));
    return res;
  }

  base_checker &operator=(const base_checker &x) {
    tree_ = x.tree_;
    checker_ = x.checker_;
    return *this;
  }

  int erase(const key_type &key) {
    int size = tree_.size();
    int res = checker_.erase(key);
    EXPECT_EQ(res, tree_.count(key));
    EXPECT_EQ(res, tree_.erase(key));
    EXPECT_EQ(tree_.count(key), 0);
    EXPECT_EQ(tree_.size(), size - res);
    erase_check(key);
    return res;
  }
  iterator erase(iterator iter) {
    key_type key = iter.key();
    int size = tree_.size();
    int count = tree_.count(key);
    auto checker_iter = checker_.lower_bound(key);
    for (iterator tmp(tree_.lower_bound(key)); tmp != iter; ++tmp) {
      ++checker_iter;
    }
    auto checker_next = checker_iter;
    ++checker_next;
    checker_.erase(checker_iter);
    iter = tree_.erase(iter);
    EXPECT_EQ(tree_.size(), checker_.size());
    EXPECT_EQ(tree_.size(), size - 1);
    EXPECT_EQ(tree_.count(key), count - 1);
    if (count == 1) {
      erase_check(key);
    }
    return iter_check(iter, checker_next);
  }

  void erase(iterator begin, iterator end) {
    int size = tree_.size();
    int count = std::distance(begin, end);
    auto checker_begin = checker_.lower_bound(begin.key());
    for (iterator tmp(tree_.lower_bound(begin.key())); tmp != begin; ++tmp) {
      ++checker_begin;
    }
    auto checker_end =
        end == tree_.end() ? checker_.end() : checker_.lower_bound(end.key());
    if (end != tree_.end()) {
      for (iterator tmp(tree_.lower_bound(end.key())); tmp != end; ++tmp) {
        ++checker_end;
      }
    }
    checker_.erase(checker_begin, checker_end);
    tree_.erase(begin, end);
    EXPECT_EQ(tree_.size(), checker_.size());
    EXPECT_EQ(tree_.size(), size - count);
  }

  void clear() {
    tree_.clear();
    checker_.clear();
  }
  void swap(base_checker &x) {
    tree_.swap(x.tree_);
    checker_.swap(x.checker_);
  }

  void verify() const {
    tree_.verify();
    EXPECT_EQ(tree_.size(), checker_.size());

    // Move through the forward iterators using increment.
    auto checker_iter = checker_.begin();
    const_iterator tree_iter(tree_.begin());
    for (; tree_iter != tree_.end(); ++tree_iter, ++checker_iter) {
      CheckPairEquals(*tree_iter, *checker_iter);
    }

    // Move through the forward iterators using decrement.
    for (int n = tree_.size() - 1; n >= 0; --n) {
      iter_check(tree_iter, checker_iter);
      --tree_iter;
      --checker_iter;
    }
    EXPECT_EQ(tree_iter, tree_.begin());
    EXPECT_EQ(checker_iter, checker_.begin());

    // Move through the reverse iterators using increment.
    auto checker_riter = checker_.rbegin();
    const_reverse_iterator tree_riter(tree_.rbegin());
    for (; tree_riter != tree_.rend(); ++tree_riter, ++checker_riter) {
      CheckPairEquals(*tree_riter, *checker_riter);
    }

    // Move through the reverse iterators using decrement.
    for (int n = tree_.size() - 1; n >= 0; --n) {
      riter_check(tree_riter, checker_riter);
      --tree_riter;
      --checker_riter;
    }
    EXPECT_EQ(tree_riter, tree_.rbegin());
    EXPECT_EQ(checker_riter, checker_.rbegin());
  }

  const TreeType &tree() const { return tree_; }

  size_type size() const {
    EXPECT_EQ(tree_.size(), checker_.size());
    return tree_.size();
  }
  size_type max_size() const { return tree_.max_size(); }
  bool empty() const {
    EXPECT_EQ(tree_.empty(), checker_.empty());
    return tree_.empty();
  }

 protected:
  TreeType tree_;
  const TreeType &const_tree_;
  CheckerType checker_;
};

namespace {
// A checker for unique sorted associative containers. TreeType is expected to
// be btree_{set,map} and CheckerType is expected to be {set,map}.
template <typename TreeType, typename CheckerType>
class unique_checker : public base_checker<TreeType, CheckerType> {
  using super_type = base_checker<TreeType, CheckerType>;

 public:
  using iterator = typename super_type::iterator;
  using value_type = typename super_type::value_type;

 public:
  unique_checker() : super_type() {}
  unique_checker(const unique_checker &x) : super_type(x) {}
  template <class InputIterator>
  unique_checker(InputIterator b, InputIterator e) : super_type(b, e) {}

  // Insertion routines.
  std::pair<iterator, bool> insert(const value_type &x) {
    int size = this->tree_.size();
    std::pair<typename CheckerType::iterator, bool> checker_res =
        this->checker_.insert(x);
    std::pair<iterator, bool> tree_res = this->tree_.insert(x);
    CheckPairEquals(*tree_res.first, *checker_res.first);
    EXPECT_EQ(tree_res.second, checker_res.second);
    EXPECT_EQ(this->tree_.size(), this->checker_.size());
    EXPECT_EQ(this->tree_.size(), size + tree_res.second);
    return tree_res;
  }
  iterator insert(iterator position, const value_type &x) {
    int size = this->tree_.size();
    std::pair<typename CheckerType::iterator, bool> checker_res =
        this->checker_.insert(x);
    iterator tree_res = this->tree_.insert(position, x);
    CheckPairEquals(*tree_res, *checker_res.first);
    EXPECT_EQ(this->tree_.size(), this->checker_.size());
    EXPECT_EQ(this->tree_.size(), size + checker_res.second);
    return tree_res;
  }
  template <typename InputIterator>
  void insert(InputIterator b, InputIterator e) {
    for (; b != e; ++b) {
      insert(*b);
    }
  }
};

// A checker for multiple sorted associative containers. TreeType is expected
// to be btree_{multiset,multimap} and CheckerType is expected to be
// {multiset,multimap}.
template <typename TreeType, typename CheckerType>
class multi_checker : public base_checker<TreeType, CheckerType> {
  using super_type = base_checker<TreeType, CheckerType>;

 public:
  using iterator = typename super_type::iterator;
  using value_type = typename super_type::value_type;

 public:
  multi_checker() : super_type() {}
  multi_checker(const multi_checker &x) : super_type(x) {}
  template <class InputIterator>
  multi_checker(InputIterator b, InputIterator e) : super_type(b, e) {}

  // Insertion routines.
  iterator insert(const value_type &x) {
    int size = this->tree_.size();
    auto checker_res = this->checker_.insert(x);
    iterator tree_res = this->tree_.insert(x);
    CheckPairEquals(*tree_res, *checker_res);
    EXPECT_EQ(this->tree_.size(), this->checker_.size());
    EXPECT_EQ(this->tree_.size(), size + 1);
    return tree_res;
  }
  iterator insert(iterator position, const value_type &x) {
    int size = this->tree_.size();
    auto checker_res = this->checker_.insert(x);
    iterator tree_res = this->tree_.insert(position, x);
    CheckPairEquals(*tree_res, *checker_res);
    EXPECT_EQ(this->tree_.size(), this->checker_.size());
    EXPECT_EQ(this->tree_.size(), size + 1);
    return tree_res;
  }
  template <typename InputIterator>
  void insert(InputIterator b, InputIterator e) {
    for (; b != e; ++b) {
      insert(*b);
    }
  }
};

template <typename T, typename V>
void DoTest(const char *name, T *b, const std::vector<V> &values) {
  typename KeyOfValue<typename T::key_type, V>::type key_of_value;

  T &mutable_b = *b;
  const T &const_b = *b;

  // Test insert.
  for (int i = 0; i < values.size(); ++i) {
    mutable_b.insert(values[i]);
    mutable_b.value_check(values[i]);
  }
  ASSERT_EQ(mutable_b.size(), values.size());

  const_b.verify();

  // Test copy constructor.
  T b_copy(const_b);
  EXPECT_EQ(b_copy.size(), const_b.size());
  for (int i = 0; i < values.size(); ++i) {
    CheckPairEquals(*b_copy.find(key_of_value(values[i])), values[i]);
  }

  // Test range constructor.
  T b_range(const_b.begin(), const_b.end());
  EXPECT_EQ(b_range.size(), const_b.size());
  for (int i = 0; i < values.size(); ++i) {
    CheckPairEquals(*b_range.find(key_of_value(values[i])), values[i]);
  }

  // Test range insertion for values that already exist.
  b_range.insert(b_copy.begin(), b_copy.end());
  b_range.verify();

  // Test range insertion for new values.
  b_range.clear();
  b_range.insert(b_copy.begin(), b_copy.end());
  EXPECT_EQ(b_range.size(), b_copy.size());
  for (int i = 0; i < values.size(); ++i) {
    CheckPairEquals(*b_range.find(key_of_value(values[i])), values[i]);
  }

  // Test assignment to self. Nothing should change.
  b_range.operator=(b_range);
  EXPECT_EQ(b_range.size(), b_copy.size());

  // Test assignment of new values.
  b_range.clear();
  b_range = b_copy;
  EXPECT_EQ(b_range.size(), b_copy.size());

  // Test swap.
  b_range.clear();
  b_range.swap(b_copy);
  EXPECT_EQ(b_copy.size(), 0);
  EXPECT_EQ(b_range.size(), const_b.size());
  for (int i = 0; i < values.size(); ++i) {
    CheckPairEquals(*b_range.find(key_of_value(values[i])), values[i]);
  }
  b_range.swap(b_copy);

  // Test non-member function swap.
  swap(b_range, b_copy);
  EXPECT_EQ(b_copy.size(), 0);
  EXPECT_EQ(b_range.size(), const_b.size());
  for (int i = 0; i < values.size(); ++i) {
    CheckPairEquals(*b_range.find(key_of_value(values[i])), values[i]);
  }
  swap(b_range, b_copy);

  // Test erase via values.
  for (int i = 0; i < values.size(); ++i) {
    mutable_b.erase(key_of_value(values[i]));
    // Erasing a non-existent key should have no effect.
    ASSERT_EQ(mutable_b.erase(key_of_value(values[i])), 0);
  }

  const_b.verify();
  EXPECT_EQ(const_b.size(), 0);

  // Test erase via iterators.
  mutable_b = b_copy;
  for (int i = 0; i < values.size(); ++i) {
    mutable_b.erase(mutable_b.find(key_of_value(values[i])));
  }

  const_b.verify();
  EXPECT_EQ(const_b.size(), 0);

  // Test insert with hint.
  for (int i = 0; i < values.size(); i++) {
    mutable_b.insert(mutable_b.upper_bound(key_of_value(values[i])), values[i]);
  }

  const_b.verify();

  // Test range erase.
  mutable_b.erase(mutable_b.begin(), mutable_b.end());
  EXPECT_EQ(mutable_b.size(), 0);
  const_b.verify();

  // First half.
  mutable_b = b_copy;
  typename T::iterator mutable_iter_end = mutable_b.begin();
  for (int i = 0; i < values.size() / 2; ++i) ++mutable_iter_end;
  mutable_b.erase(mutable_b.begin(), mutable_iter_end);
  EXPECT_EQ(mutable_b.size(), values.size() - values.size() / 2);
  const_b.verify();

  // Second half.
  mutable_b = b_copy;
  typename T::iterator mutable_iter_begin = mutable_b.begin();
  for (int i = 0; i < values.size() / 2; ++i) ++mutable_iter_begin;
  mutable_b.erase(mutable_iter_begin, mutable_b.end());
  EXPECT_EQ(mutable_b.size(), values.size() / 2);
  const_b.verify();

  // Second quarter.
  mutable_b = b_copy;
  mutable_iter_begin = mutable_b.begin();
  for (int i = 0; i < values.size() / 4; ++i) ++mutable_iter_begin;
  mutable_iter_end = mutable_iter_begin;
  for (int i = 0; i < values.size() / 4; ++i) ++mutable_iter_end;
  mutable_b.erase(mutable_iter_begin, mutable_iter_end);
  EXPECT_EQ(mutable_b.size(), values.size() - values.size() / 4);
  const_b.verify();

  mutable_b.clear();
}

template <typename T>
void ConstTest() {
  using value_type = typename T::value_type;
  typename KeyOfValue<typename T::key_type, value_type>::type key_of_value;

  T mutable_b;
  const T &const_b = mutable_b;

  // Insert a single value into the container and test looking it up.
  value_type value = Generator<value_type>(2)(2);
  mutable_b.insert(value);
  EXPECT_TRUE(mutable_b.contains(key_of_value(value)));
  EXPECT_NE(mutable_b.find(key_of_value(value)), const_b.end());
  EXPECT_TRUE(const_b.contains(key_of_value(value)));
  EXPECT_NE(const_b.find(key_of_value(value)), mutable_b.end());
  EXPECT_EQ(*const_b.lower_bound(key_of_value(value)), value);
  EXPECT_EQ(const_b.upper_bound(key_of_value(value)), const_b.end());
  EXPECT_EQ(*const_b.equal_range(key_of_value(value)).first, value);

  // We can only create a non-const iterator from a non-const container.
  typename T::iterator mutable_iter(mutable_b.begin());
  EXPECT_EQ(mutable_iter, const_b.begin());
  EXPECT_NE(mutable_iter, const_b.end());
  EXPECT_EQ(const_b.begin(), mutable_iter);
  EXPECT_NE(const_b.end(), mutable_iter);
  typename T::reverse_iterator mutable_riter(mutable_b.rbegin());
  EXPECT_EQ(mutable_riter, const_b.rbegin());
  EXPECT_NE(mutable_riter, const_b.rend());
  EXPECT_EQ(const_b.rbegin(), mutable_riter);
  EXPECT_NE(const_b.rend(), mutable_riter);

  // We can create a const iterator from a non-const iterator.
  typename T::const_iterator const_iter(mutable_iter);
  EXPECT_EQ(const_iter, mutable_b.begin());
  EXPECT_NE(const_iter, mutable_b.end());
  EXPECT_EQ(mutable_b.begin(), const_iter);
  EXPECT_NE(mutable_b.end(), const_iter);
  typename T::const_reverse_iterator const_riter(mutable_riter);
  EXPECT_EQ(const_riter, mutable_b.rbegin());
  EXPECT_NE(const_riter, mutable_b.rend());
  EXPECT_EQ(mutable_b.rbegin(), const_riter);
  EXPECT_NE(mutable_b.rend(), const_riter);

  // Make sure various methods can be invoked on a const container.
  const_b.verify();
  ASSERT_TRUE(!const_b.empty());
  EXPECT_EQ(const_b.size(), 1);
  EXPECT_GT(const_b.max_size(), 0);
  EXPECT_TRUE(const_b.contains(key_of_value(value)));
  EXPECT_EQ(const_b.count(key_of_value(value)), 1);
}

template <typename T, typename C>
void BtreeTest() {
  ConstTest<T>();

  using V = typename remove_pair_const<typename T::value_type>::type;
  const std::vector<V> random_values = GenerateValuesWithSeed<V>(
      absl::GetFlag(FLAGS_test_values), 4 * absl::GetFlag(FLAGS_test_values),
      testing::GTEST_FLAG(random_seed));

  unique_checker<T, C> container;

  // Test key insertion/deletion in sorted order.
  std::vector<V> sorted_values(random_values);
  std::sort(sorted_values.begin(), sorted_values.end());
  DoTest("sorted:    ", &container, sorted_values);

  // Test key insertion/deletion in reverse sorted order.
  std::reverse(sorted_values.begin(), sorted_values.end());
  DoTest("rsorted:   ", &container, sorted_values);

  // Test key insertion/deletion in random order.
  DoTest("random:    ", &container, random_values);
}

template <typename T, typename C>
void BtreeMultiTest() {
  ConstTest<T>();

  using V = typename remove_pair_const<typename T::value_type>::type;
  const std::vector<V> random_values = GenerateValuesWithSeed<V>(
      absl::GetFlag(FLAGS_test_values), 4 * absl::GetFlag(FLAGS_test_values),
      testing::GTEST_FLAG(random_seed));

  multi_checker<T, C> container;

  // Test keys in sorted order.
  std::vector<V> sorted_values(random_values);
  std::sort(sorted_values.begin(), sorted_values.end());
  DoTest("sorted:    ", &container, sorted_values);

  // Test keys in reverse sorted order.
  std::reverse(sorted_values.begin(), sorted_values.end());
  DoTest("rsorted:   ", &container, sorted_values);

  // Test keys in random order.
  DoTest("random:    ", &container, random_values);

  // Test keys in random order w/ duplicates.
  std::vector<V> duplicate_values(random_values);
  duplicate_values.insert(duplicate_values.end(), random_values.begin(),
                          random_values.end());
  DoTest("duplicates:", &container, duplicate_values);

  // Test all identical keys.
  std::vector<V> identical_values(100);
  std::fill(identical_values.begin(), identical_values.end(),
            Generator<V>(2)(2));
  DoTest("identical: ", &container, identical_values);
}

template <typename T>
struct PropagatingCountingAlloc : public CountingAllocator<T> {
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  using Base = CountingAllocator<T>;
  using Base::Base;

  template <typename U>
  explicit PropagatingCountingAlloc(const PropagatingCountingAlloc<U> &other)
      : Base(other.bytes_used_) {}

  template <typename U>
  struct rebind {
    using other = PropagatingCountingAlloc<U>;
  };
};

template <typename T>
void BtreeAllocatorTest() {
  using value_type = typename T::value_type;

  int64_t bytes1 = 0, bytes2 = 0;
  PropagatingCountingAlloc<T> allocator1(&bytes1);
  PropagatingCountingAlloc<T> allocator2(&bytes2);
  Generator<value_type> generator(1000);

  // Test that we allocate properly aligned memory. If we don't, then Layout
  // will assert fail.
  auto unused1 = allocator1.allocate(1);
  auto unused2 = allocator2.allocate(1);

  // Test copy assignment
  {
    T b1(typename T::key_compare(), allocator1);
    T b2(typename T::key_compare(), allocator2);

    int64_t original_bytes1 = bytes1;
    b1.insert(generator(0));
    EXPECT_GT(bytes1, original_bytes1);

    // This should propagate the allocator.
    b1 = b2;
    EXPECT_EQ(b1.size(), 0);
    EXPECT_EQ(b2.size(), 0);
    EXPECT_EQ(bytes1, original_bytes1);

    for (int i = 1; i < 1000; i++) {
      b1.insert(generator(i));
    }

    // We should have allocated out of allocator2.
    EXPECT_GT(bytes2, bytes1);
  }

  // Test move assignment
  {
    T b1(typename T::key_compare(), allocator1);
    T b2(typename T::key_compare(), allocator2);

    int64_t original_bytes1 = bytes1;
    b1.insert(generator(0));
    EXPECT_GT(bytes1, original_bytes1);

    // This should propagate the allocator.
    b1 = std::move(b2);
    EXPECT_EQ(b1.size(), 0);
    EXPECT_EQ(bytes1, original_bytes1);

    for (int i = 1; i < 1000; i++) {
      b1.insert(generator(i));
    }

    // We should have allocated out of allocator2.
    EXPECT_GT(bytes2, bytes1);
  }

  // Test swap
  {
    T b1(typename T::key_compare(), allocator1);
    T b2(typename T::key_compare(), allocator2);

    int64_t original_bytes1 = bytes1;
    b1.insert(generator(0));
    EXPECT_GT(bytes1, original_bytes1);

    // This should swap the allocators.
    swap(b1, b2);
    EXPECT_EQ(b1.size(), 0);
    EXPECT_EQ(b2.size(), 1);
    EXPECT_GT(bytes1, original_bytes1);

    for (int i = 1; i < 1000; i++) {
      b1.insert(generator(i));
    }

    // We should have allocated out of allocator2.
    EXPECT_GT(bytes2, bytes1);
  }

  allocator1.deallocate(unused1, 1);
  allocator2.deallocate(unused2, 1);
}

template <typename T>
void BtreeMapTest() {
  using value_type = typename T::value_type;
  using mapped_type = typename T::mapped_type;

  mapped_type m = Generator<mapped_type>(0)(0);
  (void)m;

  T b;

  // Verify we can insert using operator[].
  for (int i = 0; i < 1000; i++) {
    value_type v = Generator<value_type>(1000)(i);
    b[v.first] = v.second;
  }
  EXPECT_EQ(b.size(), 1000);

  // Test whether we can use the "->" operator on iterators and
  // reverse_iterators. This stresses the btree_map_params::pair_pointer
  // mechanism.
  EXPECT_EQ(b.begin()->first, Generator<value_type>(1000)(0).first);
  EXPECT_EQ(b.begin()->second, Generator<value_type>(1000)(0).second);
  EXPECT_EQ(b.rbegin()->first, Generator<value_type>(1000)(999).first);
  EXPECT_EQ(b.rbegin()->second, Generator<value_type>(1000)(999).second);
}

template <typename T>
void BtreeMultiMapTest() {
  using mapped_type = typename T::mapped_type;
  mapped_type m = Generator<mapped_type>(0)(0);
  (void)m;
}

template <typename K, int N = 256>
void SetTest() {
  EXPECT_EQ(
      sizeof(absl::btree_set<K>),
      2 * sizeof(void *) + sizeof(typename absl::btree_set<K>::size_type));
  using BtreeSet = absl::btree_set<K>;
  using CountingBtreeSet =
      absl::btree_set<K, std::less<K>, PropagatingCountingAlloc<K>>;
  BtreeTest<BtreeSet, std::set<K>>();
  BtreeAllocatorTest<CountingBtreeSet>();
}

template <typename K, int N = 256>
void MapTest() {
  EXPECT_EQ(
      sizeof(absl::btree_map<K, K>),
      2 * sizeof(void *) + sizeof(typename absl::btree_map<K, K>::size_type));
  using BtreeMap = absl::btree_map<K, K>;
  using CountingBtreeMap =
      absl::btree_map<K, K, std::less<K>,
                      PropagatingCountingAlloc<std::pair<const K, K>>>;
  BtreeTest<BtreeMap, std::map<K, K>>();
  BtreeAllocatorTest<CountingBtreeMap>();
  BtreeMapTest<BtreeMap>();
}

TEST(Btree, set_int32) { SetTest<int32_t>(); }
TEST(Btree, set_int64) { SetTest<int64_t>(); }
TEST(Btree, set_string) { SetTest<std::string>(); }
TEST(Btree, set_pair) { SetTest<std::pair<int, int>>(); }
TEST(Btree, map_int32) { MapTest<int32_t>(); }
TEST(Btree, map_int64) { MapTest<int64_t>(); }
TEST(Btree, map_string) { MapTest<std::string>(); }
TEST(Btree, map_pair) { MapTest<std::pair<int, int>>(); }

template <typename K, int N = 256>
void MultiSetTest() {
  EXPECT_EQ(
      sizeof(absl::btree_multiset<K>),
      2 * sizeof(void *) + sizeof(typename absl::btree_multiset<K>::size_type));
  using BtreeMSet = absl::btree_multiset<K>;
  using CountingBtreeMSet =
      absl::btree_multiset<K, std::less<K>, PropagatingCountingAlloc<K>>;
  BtreeMultiTest<BtreeMSet, std::multiset<K>>();
  BtreeAllocatorTest<CountingBtreeMSet>();
}

template <typename K, int N = 256>
void MultiMapTest() {
  EXPECT_EQ(sizeof(absl::btree_multimap<K, K>),
            2 * sizeof(void *) +
                sizeof(typename absl::btree_multimap<K, K>::size_type));
  using BtreeMMap = absl::btree_multimap<K, K>;
  using CountingBtreeMMap =
      absl::btree_multimap<K, K, std::less<K>,
                           PropagatingCountingAlloc<std::pair<const K, K>>>;
  BtreeMultiTest<BtreeMMap, std::multimap<K, K>>();
  BtreeMultiMapTest<BtreeMMap>();
  BtreeAllocatorTest<CountingBtreeMMap>();
}

TEST(Btree, multiset_int32) { MultiSetTest<int32_t>(); }
TEST(Btree, multiset_int64) { MultiSetTest<int64_t>(); }
TEST(Btree, multiset_string) { MultiSetTest<std::string>(); }
TEST(Btree, multiset_pair) { MultiSetTest<std::pair<int, int>>(); }
TEST(Btree, multimap_int32) { MultiMapTest<int32_t>(); }
TEST(Btree, multimap_int64) { MultiMapTest<int64_t>(); }
TEST(Btree, multimap_string) { MultiMapTest<std::string>(); }
TEST(Btree, multimap_pair) { MultiMapTest<std::pair<int, int>>(); }

struct CompareIntToString {
  bool operator()(const std::string &a, const std::string &b) const {
    return a < b;
  }
  bool operator()(const std::string &a, int b) const {
    return a < absl::StrCat(b);
  }
  bool operator()(int a, const std::string &b) const {
    return absl::StrCat(a) < b;
  }
  using is_transparent = void;
};

struct NonTransparentCompare {
  template <typename T, typename U>
  bool operator()(const T& t, const U& u) const {
    // Treating all comparators as transparent can cause inefficiencies (see
    // N3657 C++ proposal). Test that for comparators without 'is_transparent'
    // alias (like this one), we do not attempt heterogeneous lookup.
    EXPECT_TRUE((std::is_same<T, U>()));
    return t < u;
  }
};

template <typename T>
bool CanEraseWithEmptyBrace(T t, decltype(t.erase({})) *) {
  return true;
}

template <typename T>
bool CanEraseWithEmptyBrace(T, ...) {
  return false;
}

template <typename T>
void TestHeterogeneous(T table) {
  auto lb = table.lower_bound("3");
  EXPECT_EQ(lb, table.lower_bound(3));
  EXPECT_NE(lb, table.lower_bound(4));
  EXPECT_EQ(lb, table.lower_bound({"3"}));
  EXPECT_NE(lb, table.lower_bound({}));

  auto ub = table.upper_bound("3");
  EXPECT_EQ(ub, table.upper_bound(3));
  EXPECT_NE(ub, table.upper_bound(5));
  EXPECT_EQ(ub, table.upper_bound({"3"}));
  EXPECT_NE(ub, table.upper_bound({}));

  auto er = table.equal_range("3");
  EXPECT_EQ(er, table.equal_range(3));
  EXPECT_NE(er, table.equal_range(4));
  EXPECT_EQ(er, table.equal_range({"3"}));
  EXPECT_NE(er, table.equal_range({}));

  auto it = table.find("3");
  EXPECT_EQ(it, table.find(3));
  EXPECT_NE(it, table.find(4));
  EXPECT_EQ(it, table.find({"3"}));
  EXPECT_NE(it, table.find({}));

  EXPECT_TRUE(table.contains(3));
  EXPECT_FALSE(table.contains(4));
  EXPECT_TRUE(table.count({"3"}));
  EXPECT_FALSE(table.contains({}));

  EXPECT_EQ(1, table.count(3));
  EXPECT_EQ(0, table.count(4));
  EXPECT_EQ(1, table.count({"3"}));
  EXPECT_EQ(0, table.count({}));

  auto copy = table;
  copy.erase(3);
  EXPECT_EQ(table.size() - 1, copy.size());
  copy.erase(4);
  EXPECT_EQ(table.size() - 1, copy.size());
  copy.erase({"5"});
  EXPECT_EQ(table.size() - 2, copy.size());
  EXPECT_FALSE(CanEraseWithEmptyBrace(table, nullptr));

  // Also run it with const T&.
  if (std::is_class<T>()) TestHeterogeneous<const T &>(table);
}

TEST(Btree, HeterogeneousLookup) {
  TestHeterogeneous(btree_set<std::string, CompareIntToString>{"1", "3", "5"});
  TestHeterogeneous(btree_map<std::string, int, CompareIntToString>{
      {"1", 1}, {"3", 3}, {"5", 5}});
  TestHeterogeneous(
      btree_multiset<std::string, CompareIntToString>{"1", "3", "5"});
  TestHeterogeneous(btree_multimap<std::string, int, CompareIntToString>{
      {"1", 1}, {"3", 3}, {"5", 5}});

  // Only maps have .at()
  btree_map<std::string, int, CompareIntToString> map{
      {"", -1}, {"1", 1}, {"3", 3}, {"5", 5}};
  EXPECT_EQ(1, map.at(1));
  EXPECT_EQ(3, map.at({"3"}));
  EXPECT_EQ(-1, map.at({}));
  const auto &cmap = map;
  EXPECT_EQ(1, cmap.at(1));
  EXPECT_EQ(3, cmap.at({"3"}));
  EXPECT_EQ(-1, cmap.at({}));
}

TEST(Btree, NoHeterogeneousLookupWithoutAlias) {
  using StringSet = absl::btree_set<std::string, NonTransparentCompare>;
  StringSet s;
  ASSERT_TRUE(s.insert("hello").second);
  ASSERT_TRUE(s.insert("world").second);
  EXPECT_TRUE(s.end() == s.find("blah"));
  EXPECT_TRUE(s.begin() == s.lower_bound("hello"));
  EXPECT_EQ(1, s.count("world"));
  EXPECT_TRUE(s.contains("hello"));
  EXPECT_TRUE(s.contains("world"));
  EXPECT_FALSE(s.contains("blah"));

  using StringMultiSet =
      absl::btree_multiset<std::string, NonTransparentCompare>;
  StringMultiSet ms;
  ms.insert("hello");
  ms.insert("world");
  ms.insert("world");
  EXPECT_TRUE(ms.end() == ms.find("blah"));
  EXPECT_TRUE(ms.begin() == ms.lower_bound("hello"));
  EXPECT_EQ(2, ms.count("world"));
  EXPECT_TRUE(ms.contains("hello"));
  EXPECT_TRUE(ms.contains("world"));
  EXPECT_FALSE(ms.contains("blah"));
}

TEST(Btree, DefaultTransparent) {
  {
    // `int` does not have a default transparent comparator.
    // The input value is converted to key_type.
    btree_set<int> s = {1};
    double d = 1.1;
    EXPECT_EQ(s.begin(), s.find(d));
    EXPECT_TRUE(s.contains(d));
  }

  {
    // `std::string` has heterogeneous support.
    btree_set<std::string> s = {"A"};
    EXPECT_EQ(s.begin(), s.find(absl::string_view("A")));
    EXPECT_TRUE(s.contains(absl::string_view("A")));
  }
}

class StringLike {
 public:
  StringLike() = default;

  StringLike(const char* s) : s_(s) {  // NOLINT
    ++constructor_calls_;
  }

  bool operator<(const StringLike& a) const {
    return s_ < a.s_;
  }

  static void clear_constructor_call_count() {
    constructor_calls_ = 0;
  }

  static int constructor_calls() {
    return constructor_calls_;
  }

 private:
  static int constructor_calls_;
  std::string s_;
};

int StringLike::constructor_calls_ = 0;

TEST(Btree, HeterogeneousLookupDoesntDegradePerformance) {
  using StringSet = absl::btree_set<StringLike>;
  StringSet s;
  for (int i = 0; i < 100; ++i) {
    ASSERT_TRUE(s.insert(absl::StrCat(i).c_str()).second);
  }
  StringLike::clear_constructor_call_count();
  s.find("50");
  ASSERT_EQ(1, StringLike::constructor_calls());

  StringLike::clear_constructor_call_count();
  s.contains("50");
  ASSERT_EQ(1, StringLike::constructor_calls());

  StringLike::clear_constructor_call_count();
  s.count("50");
  ASSERT_EQ(1, StringLike::constructor_calls());

  StringLike::clear_constructor_call_count();
  s.lower_bound("50");
  ASSERT_EQ(1, StringLike::constructor_calls());

  StringLike::clear_constructor_call_count();
  s.upper_bound("50");
  ASSERT_EQ(1, StringLike::constructor_calls());

  StringLike::clear_constructor_call_count();
  s.equal_range("50");
  ASSERT_EQ(1, StringLike::constructor_calls());

  StringLike::clear_constructor_call_count();
  s.erase("50");
  ASSERT_EQ(1, StringLike::constructor_calls());
}

// Verify that swapping btrees swaps the key comparison functors and that we can
// use non-default constructible comparators.
struct SubstringLess {
  SubstringLess() = delete;
  explicit SubstringLess(int length) : n(length) {}
  bool operator()(const std::string &a, const std::string &b) const {
    return absl::string_view(a).substr(0, n) <
           absl::string_view(b).substr(0, n);
  }
  int n;
};

TEST(Btree, SwapKeyCompare) {
  using SubstringSet = absl::btree_set<std::string, SubstringLess>;
  SubstringSet s1(SubstringLess(1), SubstringSet::allocator_type());
  SubstringSet s2(SubstringLess(2), SubstringSet::allocator_type());

  ASSERT_TRUE(s1.insert("a").second);
  ASSERT_FALSE(s1.insert("aa").second);

  ASSERT_TRUE(s2.insert("a").second);
  ASSERT_TRUE(s2.insert("aa").second);
  ASSERT_FALSE(s2.insert("aaa").second);

  swap(s1, s2);

  ASSERT_TRUE(s1.insert("b").second);
  ASSERT_TRUE(s1.insert("bb").second);
  ASSERT_FALSE(s1.insert("bbb").second);

  ASSERT_TRUE(s2.insert("b").second);
  ASSERT_FALSE(s2.insert("bb").second);
}

TEST(Btree, UpperBoundRegression) {
  // Regress a bug where upper_bound would default-construct a new key_compare
  // instead of copying the existing one.
  using SubstringSet = absl::btree_set<std::string, SubstringLess>;
  SubstringSet my_set(SubstringLess(3));
  my_set.insert("aab");
  my_set.insert("abb");
  // We call upper_bound("aaa").  If this correctly uses the length 3
  // comparator, aaa < aab < abb, so we should get aab as the result.
  // If it instead uses the default-constructed length 2 comparator,
  // aa == aa < ab, so we'll get abb as our result.
  SubstringSet::iterator it = my_set.upper_bound("aaa");
  ASSERT_TRUE(it != my_set.end());
  EXPECT_EQ("aab", *it);
}

TEST(Btree, Comparison) {
  const int kSetSize = 1201;
  absl::btree_set<int64_t> my_set;
  for (int i = 0; i < kSetSize; ++i) {
    my_set.insert(i);
  }
  absl::btree_set<int64_t> my_set_copy(my_set);
  EXPECT_TRUE(my_set_copy == my_set);
  EXPECT_TRUE(my_set == my_set_copy);
  EXPECT_FALSE(my_set_copy != my_set);
  EXPECT_FALSE(my_set != my_set_copy);

  my_set.insert(kSetSize);
  EXPECT_FALSE(my_set_copy == my_set);
  EXPECT_FALSE(my_set == my_set_copy);
  EXPECT_TRUE(my_set_copy != my_set);
  EXPECT_TRUE(my_set != my_set_copy);

  my_set.erase(kSetSize - 1);
  EXPECT_FALSE(my_set_copy == my_set);
  EXPECT_FALSE(my_set == my_set_copy);
  EXPECT_TRUE(my_set_copy != my_set);
  EXPECT_TRUE(my_set != my_set_copy);

  absl::btree_map<std::string, int64_t> my_map;
  for (int i = 0; i < kSetSize; ++i) {
    my_map[std::string(i, 'a')] = i;
  }
  absl::btree_map<std::string, int64_t> my_map_copy(my_map);
  EXPECT_TRUE(my_map_copy == my_map);
  EXPECT_TRUE(my_map == my_map_copy);
  EXPECT_FALSE(my_map_copy != my_map);
  EXPECT_FALSE(my_map != my_map_copy);

  ++my_map_copy[std::string(7, 'a')];
  EXPECT_FALSE(my_map_copy == my_map);
  EXPECT_FALSE(my_map == my_map_copy);
  EXPECT_TRUE(my_map_copy != my_map);
  EXPECT_TRUE(my_map != my_map_copy);

  my_map_copy = my_map;
  my_map["hello"] = kSetSize;
  EXPECT_FALSE(my_map_copy == my_map);
  EXPECT_FALSE(my_map == my_map_copy);
  EXPECT_TRUE(my_map_copy != my_map);
  EXPECT_TRUE(my_map != my_map_copy);

  my_map.erase(std::string(kSetSize - 1, 'a'));
  EXPECT_FALSE(my_map_copy == my_map);
  EXPECT_FALSE(my_map == my_map_copy);
  EXPECT_TRUE(my_map_copy != my_map);
  EXPECT_TRUE(my_map != my_map_copy);
}

TEST(Btree, RangeCtorSanity) {
  std::vector<int> ivec;
  ivec.push_back(1);
  std::map<int, int> imap;
  imap.insert(std::make_pair(1, 2));
  absl::btree_multiset<int> tmset(ivec.begin(), ivec.end());
  absl::btree_multimap<int, int> tmmap(imap.begin(), imap.end());
  absl::btree_set<int> tset(ivec.begin(), ivec.end());
  absl::btree_map<int, int> tmap(imap.begin(), imap.end());
  EXPECT_EQ(1, tmset.size());
  EXPECT_EQ(1, tmmap.size());
  EXPECT_EQ(1, tset.size());
  EXPECT_EQ(1, tmap.size());
}

TEST(Btree, BtreeMapCanHoldMoveOnlyTypes) {
  absl::btree_map<std::string, std::unique_ptr<std::string>> m;

  std::unique_ptr<std::string> &v = m["A"];
  EXPECT_TRUE(v == nullptr);
  v.reset(new std::string("X"));

  auto iter = m.find("A");
  EXPECT_EQ("X", *iter->second);
}

TEST(Btree, InitializerListConstructor) {
  absl::btree_set<std::string> set({"a", "b"});
  EXPECT_EQ(set.count("a"), 1);
  EXPECT_EQ(set.count("b"), 1);

  absl::btree_multiset<int> mset({1, 1, 4});
  EXPECT_EQ(mset.count(1), 2);
  EXPECT_EQ(mset.count(4), 1);

  absl::btree_map<int, int> map({{1, 5}, {2, 10}});
  EXPECT_EQ(map[1], 5);
  EXPECT_EQ(map[2], 10);

  absl::btree_multimap<int, int> mmap({{1, 5}, {1, 10}});
  auto range = mmap.equal_range(1);
  auto it = range.first;
  ASSERT_NE(it, range.second);
  EXPECT_EQ(it->second, 5);
  ASSERT_NE(++it, range.second);
  EXPECT_EQ(it->second, 10);
  EXPECT_EQ(++it, range.second);
}

TEST(Btree, InitializerListInsert) {
  absl::btree_set<std::string> set;
  set.insert({"a", "b"});
  EXPECT_EQ(set.count("a"), 1);
  EXPECT_EQ(set.count("b"), 1);

  absl::btree_multiset<int> mset;
  mset.insert({1, 1, 4});
  EXPECT_EQ(mset.count(1), 2);
  EXPECT_EQ(mset.count(4), 1);

  absl::btree_map<int, int> map;
  map.insert({{1, 5}, {2, 10}});
  // Test that inserting one element using an initializer list also works.
  map.insert({3, 15});
  EXPECT_EQ(map[1], 5);
  EXPECT_EQ(map[2], 10);
  EXPECT_EQ(map[3], 15);

  absl::btree_multimap<int, int> mmap;
  mmap.insert({{1, 5}, {1, 10}});
  auto range = mmap.equal_range(1);
  auto it = range.first;
  ASSERT_NE(it, range.second);
  EXPECT_EQ(it->second, 5);
  ASSERT_NE(++it, range.second);
  EXPECT_EQ(it->second, 10);
  EXPECT_EQ(++it, range.second);
}

template <typename Compare, typename K>
void AssertKeyCompareToAdapted() {
  using Adapted = typename key_compare_to_adapter<Compare>::type;
  static_assert(!std::is_same<Adapted, Compare>::value,
                "key_compare_to_adapter should have adapted this comparator.");
  static_assert(
      std::is_same<absl::weak_ordering,
                   absl::result_of_t<Adapted(const K &, const K &)>>::value,
      "Adapted comparator should be a key-compare-to comparator.");
}
template <typename Compare, typename K>
void AssertKeyCompareToNotAdapted() {
  using Unadapted = typename key_compare_to_adapter<Compare>::type;
  static_assert(
      std::is_same<Unadapted, Compare>::value,
      "key_compare_to_adapter shouldn't have adapted this comparator.");
  static_assert(
      std::is_same<bool,
                   absl::result_of_t<Unadapted(const K &, const K &)>>::value,
      "Un-adapted comparator should return bool.");
}

TEST(Btree, KeyCompareToAdapter) {
  AssertKeyCompareToAdapted<std::less<std::string>, std::string>();
  AssertKeyCompareToAdapted<std::greater<std::string>, std::string>();
  AssertKeyCompareToAdapted<std::less<absl::string_view>, absl::string_view>();
  AssertKeyCompareToAdapted<std::greater<absl::string_view>,
                            absl::string_view>();
  AssertKeyCompareToNotAdapted<std::less<int>, int>();
  AssertKeyCompareToNotAdapted<std::greater<int>, int>();
}

TEST(Btree, RValueInsert) {
  InstanceTracker tracker;

  absl::btree_set<MovableOnlyInstance> set;
  set.insert(MovableOnlyInstance(1));
  set.insert(MovableOnlyInstance(3));
  MovableOnlyInstance two(2);
  set.insert(set.find(MovableOnlyInstance(3)), std::move(two));
  auto it = set.find(MovableOnlyInstance(2));
  ASSERT_NE(it, set.end());
  ASSERT_NE(++it, set.end());
  EXPECT_EQ(it->value(), 3);

  absl::btree_multiset<MovableOnlyInstance> mset;
  MovableOnlyInstance zero(0);
  MovableOnlyInstance zero2(0);
  mset.insert(std::move(zero));
  mset.insert(mset.find(MovableOnlyInstance(0)), std::move(zero2));
  EXPECT_EQ(mset.count(MovableOnlyInstance(0)), 2);

  absl::btree_map<int, MovableOnlyInstance> map;
  std::pair<const int, MovableOnlyInstance> p1 = {1, MovableOnlyInstance(5)};
  std::pair<const int, MovableOnlyInstance> p2 = {2, MovableOnlyInstance(10)};
  std::pair<const int, MovableOnlyInstance> p3 = {3, MovableOnlyInstance(15)};
  map.insert(std::move(p1));
  map.insert(std::move(p3));
  map.insert(map.find(3), std::move(p2));
  ASSERT_NE(map.find(2), map.end());
  EXPECT_EQ(map.find(2)->second.value(), 10);

  absl::btree_multimap<int, MovableOnlyInstance> mmap;
  std::pair<const int, MovableOnlyInstance> p4 = {1, MovableOnlyInstance(5)};
  std::pair<const int, MovableOnlyInstance> p5 = {1, MovableOnlyInstance(10)};
  mmap.insert(std::move(p4));
  mmap.insert(mmap.find(1), std::move(p5));
  auto range = mmap.equal_range(1);
  auto it1 = range.first;
  ASSERT_NE(it1, range.second);
  EXPECT_EQ(it1->second.value(), 10);
  ASSERT_NE(++it1, range.second);
  EXPECT_EQ(it1->second.value(), 5);
  EXPECT_EQ(++it1, range.second);

  EXPECT_EQ(tracker.copies(), 0);
  EXPECT_EQ(tracker.swaps(), 0);
}

}  // namespace

class BtreeNodePeer {
 public:
  // Yields the size of a leaf node with a specific number of values.
  template <typename ValueType>
  constexpr static size_t GetTargetNodeSize(size_t target_values_per_node) {
    return btree_node<
        set_params<ValueType, std::less<ValueType>, std::allocator<ValueType>,
                   /*TargetNodeSize=*/256,  // This parameter isn't used here.
                   /*Multi=*/false>>::SizeWithNValues(target_values_per_node);
  }

  // Yields the number of values in a (non-root) leaf node for this set.
  template <typename Set>
  constexpr static size_t GetNumValuesPerNode() {
    return btree_node<typename Set::params_type>::kNodeValues;
  }
};

namespace {

// A btree set with a specific number of values per node.
template <typename Key, int TargetValuesPerNode, typename Cmp = std::less<Key>>
class SizedBtreeSet
    : public btree_set_container<btree<
          set_params<Key, Cmp, std::allocator<Key>,
                     BtreeNodePeer::GetTargetNodeSize<Key>(TargetValuesPerNode),
                     /*Multi=*/false>>> {
  using Base = typename SizedBtreeSet::btree_set_container;

 public:
  SizedBtreeSet() {}
  using Base::Base;
};

template <typename Set>
void ExpectOperationCounts(const int expected_moves,
                           const int expected_comparisons,
                           const std::vector<int> &values,
                           InstanceTracker *tracker, Set *set) {
  for (const int v : values) set->insert(MovableOnlyInstance(v));
  set->clear();
  EXPECT_EQ(tracker->moves(), expected_moves);
  EXPECT_EQ(tracker->comparisons(), expected_comparisons);
  EXPECT_EQ(tracker->copies(), 0);
  EXPECT_EQ(tracker->swaps(), 0);
  tracker->ResetCopiesMovesSwaps();
}

// Note: when the values in this test change, it is expected to have an impact
// on performance.
TEST(Btree, MovesComparisonsCopiesSwapsTracking) {
  InstanceTracker tracker;
  // Note: this is minimum number of values per node.
  SizedBtreeSet<MovableOnlyInstance, /*TargetValuesPerNode=*/3> set3;
  // Note: this is the default number of values per node for a set of int32s
  // (with 64-bit pointers).
  SizedBtreeSet<MovableOnlyInstance, /*TargetValuesPerNode=*/61> set61;
  SizedBtreeSet<MovableOnlyInstance, /*TargetValuesPerNode=*/100> set100;

  // Don't depend on flags for random values because then the expectations will
  // fail if the flags change.
  std::vector<int> values =
      GenerateValuesWithSeed<int>(10000, 1 << 22, /*seed=*/23);

  EXPECT_EQ(BtreeNodePeer::GetNumValuesPerNode<decltype(set3)>(), 3);
  EXPECT_EQ(BtreeNodePeer::GetNumValuesPerNode<decltype(set61)>(), 61);
  EXPECT_EQ(BtreeNodePeer::GetNumValuesPerNode<decltype(set100)>(), 100);
  if (sizeof(void *) == 8) {
    EXPECT_EQ(BtreeNodePeer::GetNumValuesPerNode<absl::btree_set<int32_t>>(),
              BtreeNodePeer::GetNumValuesPerNode<decltype(set61)>());
  }

  // Test key insertion/deletion in random order.
  ExpectOperationCounts(45281, 132551, values, &tracker, &set3);
  ExpectOperationCounts(386718, 129807, values, &tracker, &set61);
  ExpectOperationCounts(586761, 130310, values, &tracker, &set100);

  // Test key insertion/deletion in sorted order.
  std::sort(values.begin(), values.end());
  ExpectOperationCounts(26638, 92134, values, &tracker, &set3);
  ExpectOperationCounts(20208, 87757, values, &tracker, &set61);
  ExpectOperationCounts(20124, 96583, values, &tracker, &set100);

  // Test key insertion/deletion in reverse sorted order.
  std::reverse(values.begin(), values.end());
  ExpectOperationCounts(49951, 119325, values, &tracker, &set3);
  ExpectOperationCounts(338813, 118266, values, &tracker, &set61);
  ExpectOperationCounts(534529, 125279, values, &tracker, &set100);
}

struct MovableOnlyInstanceThreeWayCompare {
  absl::weak_ordering operator()(const MovableOnlyInstance &a,
                                 const MovableOnlyInstance &b) const {
    return a.compare(b);
  }
};

// Note: when the values in this test change, it is expected to have an impact
// on performance.
TEST(Btree, MovesComparisonsCopiesSwapsTrackingThreeWayCompare) {
  InstanceTracker tracker;
  // Note: this is minimum number of values per node.
  SizedBtreeSet<MovableOnlyInstance, /*TargetValuesPerNode=*/3,
                MovableOnlyInstanceThreeWayCompare>
      set3;
  // Note: this is the default number of values per node for a set of int32s
  // (with 64-bit pointers).
  SizedBtreeSet<MovableOnlyInstance, /*TargetValuesPerNode=*/61,
                MovableOnlyInstanceThreeWayCompare>
      set61;
  SizedBtreeSet<MovableOnlyInstance, /*TargetValuesPerNode=*/100,
                MovableOnlyInstanceThreeWayCompare>
      set100;

  // Don't depend on flags for random values because then the expectations will
  // fail if the flags change.
  std::vector<int> values =
      GenerateValuesWithSeed<int>(10000, 1 << 22, /*seed=*/23);

  EXPECT_EQ(BtreeNodePeer::GetNumValuesPerNode<decltype(set3)>(), 3);
  EXPECT_EQ(BtreeNodePeer::GetNumValuesPerNode<decltype(set61)>(), 61);
  EXPECT_EQ(BtreeNodePeer::GetNumValuesPerNode<decltype(set100)>(), 100);
  if (sizeof(void *) == 8) {
    EXPECT_EQ(BtreeNodePeer::GetNumValuesPerNode<absl::btree_set<int32_t>>(),
              BtreeNodePeer::GetNumValuesPerNode<decltype(set61)>());
  }

  // Test key insertion/deletion in random order.
  ExpectOperationCounts(45281, 122560, values, &tracker, &set3);
  ExpectOperationCounts(386718, 119816, values, &tracker, &set61);
  ExpectOperationCounts(586761, 120319, values, &tracker, &set100);

  // Test key insertion/deletion in sorted order.
  std::sort(values.begin(), values.end());
  ExpectOperationCounts(26638, 92134, values, &tracker, &set3);
  ExpectOperationCounts(20208, 87757, values, &tracker, &set61);
  ExpectOperationCounts(20124, 96583, values, &tracker, &set100);

  // Test key insertion/deletion in reverse sorted order.
  std::reverse(values.begin(), values.end());
  ExpectOperationCounts(49951, 109326, values, &tracker, &set3);
  ExpectOperationCounts(338813, 108267, values, &tracker, &set61);
  ExpectOperationCounts(534529, 115280, values, &tracker, &set100);
}

struct NoDefaultCtor {
  int num;
  explicit NoDefaultCtor(int i) : num(i) {}

  friend bool operator<(const NoDefaultCtor& a, const NoDefaultCtor& b) {
    return a.num < b.num;
  }
};

TEST(Btree, BtreeMapCanHoldNoDefaultCtorTypes) {
  absl::btree_map<NoDefaultCtor, NoDefaultCtor> m;

  for (int i = 1; i <= 99; ++i) {
    SCOPED_TRACE(i);
    EXPECT_TRUE(m.emplace(NoDefaultCtor(i), NoDefaultCtor(100 - i)).second);
  }
  EXPECT_FALSE(m.emplace(NoDefaultCtor(78), NoDefaultCtor(0)).second);

  auto iter99 = m.find(NoDefaultCtor(99));
  ASSERT_NE(iter99, m.end());
  EXPECT_EQ(iter99->second.num, 1);

  auto iter1 = m.find(NoDefaultCtor(1));
  ASSERT_NE(iter1, m.end());
  EXPECT_EQ(iter1->second.num, 99);

  auto iter50 = m.find(NoDefaultCtor(50));
  ASSERT_NE(iter50, m.end());
  EXPECT_EQ(iter50->second.num, 50);

  auto iter25 = m.find(NoDefaultCtor(25));
  ASSERT_NE(iter25, m.end());
  EXPECT_EQ(iter25->second.num, 75);
}

TEST(Btree, BtreeMultimapCanHoldNoDefaultCtorTypes) {
  absl::btree_multimap<NoDefaultCtor, NoDefaultCtor> m;

  for (int i = 1; i <= 99; ++i) {
    SCOPED_TRACE(i);
    m.emplace(NoDefaultCtor(i), NoDefaultCtor(100 - i));
  }

  auto iter99 = m.find(NoDefaultCtor(99));
  ASSERT_NE(iter99, m.end());
  EXPECT_EQ(iter99->second.num, 1);

  auto iter1 = m.find(NoDefaultCtor(1));
  ASSERT_NE(iter1, m.end());
  EXPECT_EQ(iter1->second.num, 99);

  auto iter50 = m.find(NoDefaultCtor(50));
  ASSERT_NE(iter50, m.end());
  EXPECT_EQ(iter50->second.num, 50);

  auto iter25 = m.find(NoDefaultCtor(25));
  ASSERT_NE(iter25, m.end());
  EXPECT_EQ(iter25->second.num, 75);
}

TEST(Btree, MapAt) {
  absl::btree_map<int, int> map = {{1, 2}, {2, 4}};
  EXPECT_EQ(map.at(1), 2);
  EXPECT_EQ(map.at(2), 4);
  map.at(2) = 8;
  const absl::btree_map<int, int> &const_map = map;
  EXPECT_EQ(const_map.at(1), 2);
  EXPECT_EQ(const_map.at(2), 8);
  try {
    map.at(3);
    FAIL() << "Exception not thrown";
  } catch (const std::out_of_range& e) {
    EXPECT_STREQ(e.what(), "absl::btree_map::at");
  }
}

TEST(Btree, BtreeMultisetEmplace) {
  const int value_to_insert = 123456;
  absl::btree_multiset<int> s;
  auto iter = s.emplace(value_to_insert);
  ASSERT_NE(iter, s.end());
  EXPECT_EQ(*iter, value_to_insert);
  auto iter2 = s.emplace(value_to_insert);
  EXPECT_NE(iter2, iter);
  ASSERT_NE(iter2, s.end());
  EXPECT_EQ(*iter2, value_to_insert);
  auto result = s.equal_range(value_to_insert);
  EXPECT_EQ(std::distance(result.first, result.second), 2);
}

TEST(Btree, BtreeMultisetEmplaceHint) {
  const int value_to_insert = 123456;
  absl::btree_multiset<int> s;
  auto iter = s.emplace(value_to_insert);
  ASSERT_NE(iter, s.end());
  EXPECT_EQ(*iter, value_to_insert);
  auto emplace_iter = s.emplace_hint(iter, value_to_insert);
  EXPECT_NE(emplace_iter, iter);
  ASSERT_NE(emplace_iter, s.end());
  EXPECT_EQ(*emplace_iter, value_to_insert);
}

TEST(Btree, BtreeMultimapEmplace) {
  const int key_to_insert = 123456;
  const char value0[] = "a";
  absl::btree_multimap<int, std::string> s;
  auto iter = s.emplace(key_to_insert, value0);
  ASSERT_NE(iter, s.end());
  EXPECT_EQ(iter->first, key_to_insert);
  EXPECT_EQ(iter->second, value0);
  const char value1[] = "b";
  auto iter2 = s.emplace(key_to_insert, value1);
  EXPECT_NE(iter2, iter);
  ASSERT_NE(iter2, s.end());
  EXPECT_EQ(iter2->first, key_to_insert);
  EXPECT_EQ(iter2->second, value1);
  auto result = s.equal_range(key_to_insert);
  EXPECT_EQ(std::distance(result.first, result.second), 2);
}

TEST(Btree, BtreeMultimapEmplaceHint) {
  const int key_to_insert = 123456;
  const char value0[] = "a";
  absl::btree_multimap<int, std::string> s;
  auto iter = s.emplace(key_to_insert, value0);
  ASSERT_NE(iter, s.end());
  EXPECT_EQ(iter->first, key_to_insert);
  EXPECT_EQ(iter->second, value0);
  const char value1[] = "b";
  auto emplace_iter = s.emplace_hint(iter, key_to_insert, value1);
  EXPECT_NE(emplace_iter, iter);
  ASSERT_NE(emplace_iter, s.end());
  EXPECT_EQ(emplace_iter->first, key_to_insert);
  EXPECT_EQ(emplace_iter->second, value1);
}

TEST(Btree, ConstIteratorAccessors) {
  absl::btree_set<int> set;
  for (int i = 0; i < 100; ++i) {
    set.insert(i);
  }

  auto it = set.cbegin();
  auto r_it = set.crbegin();
  for (int i = 0; i < 100; ++i, ++it, ++r_it) {
    ASSERT_EQ(*it, i);
    ASSERT_EQ(*r_it, 99 - i);
  }
  EXPECT_EQ(it, set.cend());
  EXPECT_EQ(r_it, set.crend());
}

TEST(Btree, StrSplitCompatible) {
  const absl::btree_set<std::string> split_set = absl::StrSplit("a,b,c", ',');
  const absl::btree_set<std::string> expected_set = {"a", "b", "c"};

  EXPECT_EQ(split_set, expected_set);
}

// We can't use EXPECT_EQ/etc. to compare absl::weak_ordering because they
// convert literal 0 to int and absl::weak_ordering can only be compared with
// literal 0. Defining this function allows for avoiding ClangTidy warnings.
bool Identity(const bool b) { return b; }

TEST(Btree, ValueComp) {
  absl::btree_set<int> s;
  EXPECT_TRUE(s.value_comp()(1, 2));
  EXPECT_FALSE(s.value_comp()(2, 2));
  EXPECT_FALSE(s.value_comp()(2, 1));

  absl::btree_map<int, int> m1;
  EXPECT_TRUE(m1.value_comp()(std::make_pair(1, 0), std::make_pair(2, 0)));
  EXPECT_FALSE(m1.value_comp()(std::make_pair(2, 0), std::make_pair(2, 0)));
  EXPECT_FALSE(m1.value_comp()(std::make_pair(2, 0), std::make_pair(1, 0)));

  absl::btree_map<std::string, int> m2;
  EXPECT_TRUE(Identity(
      m2.value_comp()(std::make_pair("a", 0), std::make_pair("b", 0)) < 0));
  EXPECT_TRUE(Identity(
      m2.value_comp()(std::make_pair("b", 0), std::make_pair("b", 0)) == 0));
  EXPECT_TRUE(Identity(
      m2.value_comp()(std::make_pair("b", 0), std::make_pair("a", 0)) > 0));
}

TEST(Btree, DefaultConstruction) {
  absl::btree_set<int> s;
  absl::btree_map<int, int> m;
  absl::btree_multiset<int> ms;
  absl::btree_multimap<int, int> mm;

  EXPECT_TRUE(s.empty());
  EXPECT_TRUE(m.empty());
  EXPECT_TRUE(ms.empty());
  EXPECT_TRUE(mm.empty());
}

TEST(Btree, SwissTableHashable) {
  static constexpr int kValues = 10000;
  std::vector<int> values(kValues);
  std::iota(values.begin(), values.end(), 0);
  std::vector<std::pair<int, int>> map_values;
  for (int v : values) map_values.emplace_back(v, -v);

  using set = absl::btree_set<int>;
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      set{},
      set{1},
      set{2},
      set{1, 2},
      set{2, 1},
      set(values.begin(), values.end()),
      set(values.rbegin(), values.rend()),
  }));

  using mset = absl::btree_multiset<int>;
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      mset{},
      mset{1},
      mset{1, 1},
      mset{2},
      mset{2, 2},
      mset{1, 2},
      mset{1, 1, 2},
      mset{1, 2, 2},
      mset{1, 1, 2, 2},
      mset(values.begin(), values.end()),
      mset(values.rbegin(), values.rend()),
  }));

  using map = absl::btree_map<int, int>;
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      map{},
      map{{1, 0}},
      map{{1, 1}},
      map{{2, 0}},
      map{{2, 2}},
      map{{1, 0}, {2, 1}},
      map(map_values.begin(), map_values.end()),
      map(map_values.rbegin(), map_values.rend()),
  }));

  using mmap = absl::btree_multimap<int, int>;
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      mmap{},
      mmap{{1, 0}},
      mmap{{1, 1}},
      mmap{{1, 0}, {1, 1}},
      mmap{{1, 1}, {1, 0}},
      mmap{{2, 0}},
      mmap{{2, 2}},
      mmap{{1, 0}, {2, 1}},
      mmap(map_values.begin(), map_values.end()),
      mmap(map_values.rbegin(), map_values.rend()),
  }));
}

TEST(Btree, ComparableSet) {
  absl::btree_set<int> s1 = {1, 2};
  absl::btree_set<int> s2 = {2, 3};
  EXPECT_LT(s1, s2);
  EXPECT_LE(s1, s2);
  EXPECT_LE(s1, s1);
  EXPECT_GT(s2, s1);
  EXPECT_GE(s2, s1);
  EXPECT_GE(s1, s1);
}

TEST(Btree, ComparableSetsDifferentLength) {
  absl::btree_set<int> s1 = {1, 2};
  absl::btree_set<int> s2 = {1, 2, 3};
  EXPECT_LT(s1, s2);
  EXPECT_LE(s1, s2);
  EXPECT_GT(s2, s1);
  EXPECT_GE(s2, s1);
}

TEST(Btree, ComparableMultiset) {
  absl::btree_multiset<int> s1 = {1, 2};
  absl::btree_multiset<int> s2 = {2, 3};
  EXPECT_LT(s1, s2);
  EXPECT_LE(s1, s2);
  EXPECT_LE(s1, s1);
  EXPECT_GT(s2, s1);
  EXPECT_GE(s2, s1);
  EXPECT_GE(s1, s1);
}

TEST(Btree, ComparableMap) {
  absl::btree_map<int, int> s1 = {{1, 2}};
  absl::btree_map<int, int> s2 = {{2, 3}};
  EXPECT_LT(s1, s2);
  EXPECT_LE(s1, s2);
  EXPECT_LE(s1, s1);
  EXPECT_GT(s2, s1);
  EXPECT_GE(s2, s1);
  EXPECT_GE(s1, s1);
}

TEST(Btree, ComparableMultimap) {
  absl::btree_multimap<int, int> s1 = {{1, 2}};
  absl::btree_multimap<int, int> s2 = {{2, 3}};
  EXPECT_LT(s1, s2);
  EXPECT_LE(s1, s2);
  EXPECT_LE(s1, s1);
  EXPECT_GT(s2, s1);
  EXPECT_GE(s2, s1);
  EXPECT_GE(s1, s1);
}

TEST(Btree, ComparableSetWithCustomComparator) {
  // As specified by
  // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3337.pdf section
  // [container.requirements.general].12, ordering associative containers always
  // uses default '<' operator
  // - even if otherwise the container uses custom functor.
  absl::btree_set<int, std::greater<int>> s1 = {1, 2};
  absl::btree_set<int, std::greater<int>> s2 = {2, 3};
  EXPECT_LT(s1, s2);
  EXPECT_LE(s1, s2);
  EXPECT_LE(s1, s1);
  EXPECT_GT(s2, s1);
  EXPECT_GE(s2, s1);
  EXPECT_GE(s1, s1);
}

TEST(Btree, EraseReturnsIterator) {
  absl::btree_set<int> set = {1, 2, 3, 4, 5};
  auto result_it = set.erase(set.begin(), set.find(3));
  EXPECT_EQ(result_it, set.find(3));
  result_it = set.erase(set.find(5));
  EXPECT_EQ(result_it, set.end());
}

TEST(Btree, ExtractAndInsertNodeHandleSet) {
  absl::btree_set<int> src1 = {1, 2, 3, 4, 5};
  auto nh = src1.extract(src1.find(3));
  EXPECT_THAT(src1, ElementsAre(1, 2, 4, 5));
  absl::btree_set<int> other;
  absl::btree_set<int>::insert_return_type res = other.insert(std::move(nh));
  EXPECT_THAT(other, ElementsAre(3));
  EXPECT_EQ(res.position, other.find(3));
  EXPECT_TRUE(res.inserted);
  EXPECT_TRUE(res.node.empty());

  absl::btree_set<int> src2 = {3, 4};
  nh = src2.extract(src2.find(3));
  EXPECT_THAT(src2, ElementsAre(4));
  res = other.insert(std::move(nh));
  EXPECT_THAT(other, ElementsAre(3));
  EXPECT_EQ(res.position, other.find(3));
  EXPECT_FALSE(res.inserted);
  ASSERT_FALSE(res.node.empty());
  EXPECT_EQ(res.node.value(), 3);
}

struct Deref {
  bool operator()(const std::unique_ptr<int> &lhs,
                  const std::unique_ptr<int> &rhs) const {
    return *lhs < *rhs;
  }
};

TEST(Btree, ExtractWithUniquePtr) {
  absl::btree_set<std::unique_ptr<int>, Deref> s;
  s.insert(absl::make_unique<int>(1));
  s.insert(absl::make_unique<int>(2));
  s.insert(absl::make_unique<int>(3));
  s.insert(absl::make_unique<int>(4));
  s.insert(absl::make_unique<int>(5));
  auto nh = s.extract(s.find(absl::make_unique<int>(3)));
  EXPECT_EQ(s.size(), 4);
  EXPECT_EQ(*nh.value(), 3);
  s.insert(std::move(nh));
  EXPECT_EQ(s.size(), 5);
}

TEST(Btree, ExtractAndInsertNodeHandleMultiSet) {
  absl::btree_multiset<int> src1 = {1, 2, 3, 3, 4, 5};
  auto nh = src1.extract(src1.find(3));
  EXPECT_THAT(src1, ElementsAre(1, 2, 3, 4, 5));
  absl::btree_multiset<int> other;
  auto res = other.insert(std::move(nh));
  EXPECT_THAT(other, ElementsAre(3));
  EXPECT_EQ(res, other.find(3));

  absl::btree_multiset<int> src2 = {3, 4};
  nh = src2.extract(src2.find(3));
  EXPECT_THAT(src2, ElementsAre(4));
  res = other.insert(std::move(nh));
  EXPECT_THAT(other, ElementsAre(3, 3));
  EXPECT_EQ(res, ++other.find(3));
}

TEST(Btree, ExtractAndInsertNodeHandleMap) {
  absl::btree_map<int, int> src1 = {{1, 2}, {3, 4}, {5, 6}};
  auto nh = src1.extract(src1.find(3));
  EXPECT_THAT(src1, ElementsAre(Pair(1, 2), Pair(5, 6)));
  absl::btree_map<int, int> other;
  absl::btree_map<int, int>::insert_return_type res =
      other.insert(std::move(nh));
  EXPECT_THAT(other, ElementsAre(Pair(3, 4)));
  EXPECT_EQ(res.position, other.find(3));
  EXPECT_TRUE(res.inserted);
  EXPECT_TRUE(res.node.empty());

  absl::btree_map<int, int> src2 = {{3, 6}};
  nh = src2.extract(src2.find(3));
  EXPECT_TRUE(src2.empty());
  res = other.insert(std::move(nh));
  EXPECT_THAT(other, ElementsAre(Pair(3, 4)));
  EXPECT_EQ(res.position, other.find(3));
  EXPECT_FALSE(res.inserted);
  ASSERT_FALSE(res.node.empty());
  EXPECT_EQ(res.node.key(), 3);
  EXPECT_EQ(res.node.mapped(), 6);
}

TEST(Btree, ExtractAndInsertNodeHandleMultiMap) {
  absl::btree_multimap<int, int> src1 = {{1, 2}, {3, 4}, {5, 6}};
  auto nh = src1.extract(src1.find(3));
  EXPECT_THAT(src1, ElementsAre(Pair(1, 2), Pair(5, 6)));
  absl::btree_multimap<int, int> other;
  auto res = other.insert(std::move(nh));
  EXPECT_THAT(other, ElementsAre(Pair(3, 4)));
  EXPECT_EQ(res, other.find(3));

  absl::btree_multimap<int, int> src2 = {{3, 6}};
  nh = src2.extract(src2.find(3));
  EXPECT_TRUE(src2.empty());
  res = other.insert(std::move(nh));
  EXPECT_THAT(other, ElementsAre(Pair(3, 4), Pair(3, 6)));
  EXPECT_EQ(res, ++other.begin());
}

// For multisets, insert with hint also affects correctness because we need to
// insert immediately before the hint if possible.
struct InsertMultiHintData {
  int key;
  int not_key;
  bool operator==(const InsertMultiHintData other) const {
    return key == other.key && not_key == other.not_key;
  }
};

struct InsertMultiHintDataKeyCompare {
  using is_transparent = void;
  bool operator()(const InsertMultiHintData a,
                  const InsertMultiHintData b) const {
    return a.key < b.key;
  }
  bool operator()(const int a, const InsertMultiHintData b) const {
    return a < b.key;
  }
  bool operator()(const InsertMultiHintData a, const int b) const {
    return a.key < b;
  }
};

TEST(Btree, InsertHintNodeHandle) {
  // For unique sets, insert with hint is just a performance optimization.
  // Test that insert works correctly when the hint is right or wrong.
  {
    absl::btree_set<int> src = {1, 2, 3, 4, 5};
    auto nh = src.extract(src.find(3));
    EXPECT_THAT(src, ElementsAre(1, 2, 4, 5));
    absl::btree_set<int> other = {0, 100};
    // Test a correct hint.
    auto it = other.insert(other.lower_bound(3), std::move(nh));
    EXPECT_THAT(other, ElementsAre(0, 3, 100));
    EXPECT_EQ(it, other.find(3));

    nh = src.extract(src.find(5));
    // Test an incorrect hint.
    it = other.insert(other.end(), std::move(nh));
    EXPECT_THAT(other, ElementsAre(0, 3, 5, 100));
    EXPECT_EQ(it, other.find(5));
  }

  absl::btree_multiset<InsertMultiHintData, InsertMultiHintDataKeyCompare> src =
      {{1, 2}, {3, 4}, {3, 5}};
  auto nh = src.extract(src.lower_bound(3));
  EXPECT_EQ(nh.value(), (InsertMultiHintData{3, 4}));
  absl::btree_multiset<InsertMultiHintData, InsertMultiHintDataKeyCompare>
      other = {{3, 1}, {3, 2}, {3, 3}};
  auto it = other.insert(--other.end(), std::move(nh));
  EXPECT_THAT(
      other, ElementsAre(InsertMultiHintData{3, 1}, InsertMultiHintData{3, 2},
                         InsertMultiHintData{3, 4}, InsertMultiHintData{3, 3}));
  EXPECT_EQ(it, --(--other.end()));

  nh = src.extract(src.find(3));
  EXPECT_EQ(nh.value(), (InsertMultiHintData{3, 5}));
  it = other.insert(other.begin(), std::move(nh));
  EXPECT_THAT(other,
              ElementsAre(InsertMultiHintData{3, 5}, InsertMultiHintData{3, 1},
                          InsertMultiHintData{3, 2}, InsertMultiHintData{3, 4},
                          InsertMultiHintData{3, 3}));
  EXPECT_EQ(it, other.begin());
}

struct IntCompareToCmp {
  absl::weak_ordering operator()(int a, int b) const {
    if (a < b) return absl::weak_ordering::less;
    if (a > b) return absl::weak_ordering::greater;
    return absl::weak_ordering::equivalent;
  }
};

TEST(Btree, MergeIntoUniqueContainers) {
  absl::btree_set<int, IntCompareToCmp> src1 = {1, 2, 3};
  absl::btree_multiset<int> src2 = {3, 4, 4, 5};
  absl::btree_set<int> dst;

  dst.merge(src1);
  EXPECT_TRUE(src1.empty());
  EXPECT_THAT(dst, ElementsAre(1, 2, 3));
  dst.merge(src2);
  EXPECT_THAT(src2, ElementsAre(3, 4));
  EXPECT_THAT(dst, ElementsAre(1, 2, 3, 4, 5));
}

TEST(Btree, MergeIntoUniqueContainersWithCompareTo) {
  absl::btree_set<int, IntCompareToCmp> src1 = {1, 2, 3};
  absl::btree_multiset<int> src2 = {3, 4, 4, 5};
  absl::btree_set<int, IntCompareToCmp> dst;

  dst.merge(src1);
  EXPECT_TRUE(src1.empty());
  EXPECT_THAT(dst, ElementsAre(1, 2, 3));
  dst.merge(src2);
  EXPECT_THAT(src2, ElementsAre(3, 4));
  EXPECT_THAT(dst, ElementsAre(1, 2, 3, 4, 5));
}

TEST(Btree, MergeIntoMultiContainers) {
  absl::btree_set<int, IntCompareToCmp> src1 = {1, 2, 3};
  absl::btree_multiset<int> src2 = {3, 4, 4, 5};
  absl::btree_multiset<int> dst;

  dst.merge(src1);
  EXPECT_TRUE(src1.empty());
  EXPECT_THAT(dst, ElementsAre(1, 2, 3));
  dst.merge(src2);
  EXPECT_TRUE(src2.empty());
  EXPECT_THAT(dst, ElementsAre(1, 2, 3, 3, 4, 4, 5));
}

TEST(Btree, MergeIntoMultiContainersWithCompareTo) {
  absl::btree_set<int, IntCompareToCmp> src1 = {1, 2, 3};
  absl::btree_multiset<int> src2 = {3, 4, 4, 5};
  absl::btree_multiset<int, IntCompareToCmp> dst;

  dst.merge(src1);
  EXPECT_TRUE(src1.empty());
  EXPECT_THAT(dst, ElementsAre(1, 2, 3));
  dst.merge(src2);
  EXPECT_TRUE(src2.empty());
  EXPECT_THAT(dst, ElementsAre(1, 2, 3, 3, 4, 4, 5));
}

TEST(Btree, MergeIntoMultiMapsWithDifferentComparators) {
  absl::btree_map<int, int, IntCompareToCmp> src1 = {{1, 1}, {2, 2}, {3, 3}};
  absl::btree_multimap<int, int, std::greater<int>> src2 = {
      {5, 5}, {4, 1}, {4, 4}, {3, 2}};
  absl::btree_multimap<int, int> dst;

  dst.merge(src1);
  EXPECT_TRUE(src1.empty());
  EXPECT_THAT(dst, ElementsAre(Pair(1, 1), Pair(2, 2), Pair(3, 3)));
  dst.merge(src2);
  EXPECT_TRUE(src2.empty());
  EXPECT_THAT(dst, ElementsAre(Pair(1, 1), Pair(2, 2), Pair(3, 3), Pair(3, 2),
                               Pair(4, 1), Pair(4, 4), Pair(5, 5)));
}

struct KeyCompareToWeakOrdering {
  template <typename T>
  absl::weak_ordering operator()(const T &a, const T &b) const {
    return a < b ? absl::weak_ordering::less
                 : a == b ? absl::weak_ordering::equivalent
                          : absl::weak_ordering::greater;
  }
};

struct KeyCompareToStrongOrdering {
  template <typename T>
  absl::strong_ordering operator()(const T &a, const T &b) const {
    return a < b ? absl::strong_ordering::less
                 : a == b ? absl::strong_ordering::equal
                          : absl::strong_ordering::greater;
  }
};

TEST(Btree, UserProvidedKeyCompareToComparators) {
  absl::btree_set<int, KeyCompareToWeakOrdering> weak_set = {1, 2, 3};
  EXPECT_TRUE(weak_set.contains(2));
  EXPECT_FALSE(weak_set.contains(4));

  absl::btree_set<int, KeyCompareToStrongOrdering> strong_set = {1, 2, 3};
  EXPECT_TRUE(strong_set.contains(2));
  EXPECT_FALSE(strong_set.contains(4));
}

TEST(Btree, TryEmplaceBasicTest) {
  absl::btree_map<int, std::string> m;

  // Should construct a std::string from the literal.
  m.try_emplace(1, "one");
  EXPECT_EQ(1, m.size());

  // Try other std::string constructors and const lvalue key.
  const int key(42);
  m.try_emplace(key, 3, 'a');
  m.try_emplace(2, std::string("two"));

  EXPECT_TRUE(std::is_sorted(m.begin(), m.end()));
  EXPECT_THAT(m, ElementsAreArray(std::vector<std::pair<int, std::string>>{
                     {1, "one"}, {2, "two"}, {42, "aaa"}}));
}

TEST(Btree, TryEmplaceWithHintWorks) {
  // Use a counting comparator here to verify that hint is used.
  int calls = 0;
  auto cmp = [&calls](int x, int y) {
    ++calls;
    return x < y;
  };
  using Cmp = decltype(cmp);

  absl::btree_map<int, int, Cmp> m(cmp);
  for (int i = 0; i < 128; ++i) {
    m.emplace(i, i);
  }

  // Sanity check for the comparator
  calls = 0;
  m.emplace(127, 127);
  EXPECT_GE(calls, 4);

  // Try with begin hint:
  calls = 0;
  auto it = m.try_emplace(m.begin(), -1, -1);
  EXPECT_EQ(129, m.size());
  EXPECT_EQ(it, m.begin());
  EXPECT_LE(calls, 2);

  // Try with end hint:
  calls = 0;
  std::pair<int, int> pair1024 = {1024, 1024};
  it = m.try_emplace(m.end(), pair1024.first, pair1024.second);
  EXPECT_EQ(130, m.size());
  EXPECT_EQ(it, --m.end());
  EXPECT_LE(calls, 2);

  // Try value already present, bad hint; ensure no duplicate added:
  calls = 0;
  it = m.try_emplace(m.end(), 16, 17);
  EXPECT_EQ(130, m.size());
  EXPECT_GE(calls, 4);
  EXPECT_EQ(it, m.find(16));

  // Try value already present, hint points directly to it:
  calls = 0;
  it = m.try_emplace(it, 16, 17);
  EXPECT_EQ(130, m.size());
  EXPECT_LE(calls, 2);
  EXPECT_EQ(it, m.find(16));

  m.erase(2);
  EXPECT_EQ(129, m.size());
  auto hint = m.find(3);
  // Try emplace in the middle of two other elements.
  calls = 0;
  m.try_emplace(hint, 2, 2);
  EXPECT_EQ(130, m.size());
  EXPECT_LE(calls, 2);

  EXPECT_TRUE(std::is_sorted(m.begin(), m.end()));
}

TEST(Btree, TryEmplaceWithBadHint) {
  absl::btree_map<int, int> m = {{1, 1}, {9, 9}};

  // Bad hint (too small), should still emplace:
  auto it = m.try_emplace(m.begin(), 2, 2);
  EXPECT_EQ(it, ++m.begin());
  EXPECT_THAT(m, ElementsAreArray(
                     std::vector<std::pair<int, int>>{{1, 1}, {2, 2}, {9, 9}}));

  // Bad hint, too large this time:
  it = m.try_emplace(++(++m.begin()), 0, 0);
  EXPECT_EQ(it, m.begin());
  EXPECT_THAT(m, ElementsAreArray(std::vector<std::pair<int, int>>{
                     {0, 0}, {1, 1}, {2, 2}, {9, 9}}));
}

TEST(Btree, TryEmplaceMaintainsSortedOrder) {
  absl::btree_map<int, std::string> m;
  std::pair<int, std::string> pair5 = {5, "five"};

  // Test both lvalue & rvalue emplace.
  m.try_emplace(10, "ten");
  m.try_emplace(pair5.first, pair5.second);
  EXPECT_EQ(2, m.size());
  EXPECT_TRUE(std::is_sorted(m.begin(), m.end()));

  int int100{100};
  m.try_emplace(int100, "hundred");
  m.try_emplace(1, "one");
  EXPECT_EQ(4, m.size());
  EXPECT_TRUE(std::is_sorted(m.begin(), m.end()));
}

TEST(Btree, TryEmplaceWithHintAndNoValueArgsWorks) {
  absl::btree_map<int, int> m;
  m.try_emplace(m.end(), 1);
  EXPECT_EQ(0, m[1]);
}

TEST(Btree, TryEmplaceWithHintAndMultipleValueArgsWorks) {
  absl::btree_map<int, std::string> m;
  m.try_emplace(m.end(), 1, 10, 'a');
  EXPECT_EQ(std::string(10, 'a'), m[1]);
}

TEST(Btree, MoveAssignmentAllocatorPropagation) {
  InstanceTracker tracker;

  int64_t bytes1 = 0, bytes2 = 0;
  PropagatingCountingAlloc<MovableOnlyInstance> allocator1(&bytes1);
  PropagatingCountingAlloc<MovableOnlyInstance> allocator2(&bytes2);
  std::less<MovableOnlyInstance> cmp;

  // Test propagating allocator_type.
  {
    absl::btree_set<MovableOnlyInstance, std::less<MovableOnlyInstance>,
                    PropagatingCountingAlloc<MovableOnlyInstance>>
        set1(cmp, allocator1), set2(cmp, allocator2);

    for (int i = 0; i < 100; ++i) set1.insert(MovableOnlyInstance(i));

    tracker.ResetCopiesMovesSwaps();
    set2 = std::move(set1);
    EXPECT_EQ(tracker.moves(), 0);
  }
  // Test non-propagating allocator_type with equal allocators.
  {
    absl::btree_set<MovableOnlyInstance, std::less<MovableOnlyInstance>,
                    CountingAllocator<MovableOnlyInstance>>
        set1(cmp, allocator1), set2(cmp, allocator1);

    for (int i = 0; i < 100; ++i) set1.insert(MovableOnlyInstance(i));

    tracker.ResetCopiesMovesSwaps();
    set2 = std::move(set1);
    EXPECT_EQ(tracker.moves(), 0);
  }
  // Test non-propagating allocator_type with different allocators.
  {
    absl::btree_set<MovableOnlyInstance, std::less<MovableOnlyInstance>,
                    CountingAllocator<MovableOnlyInstance>>
        set1(cmp, allocator1), set2(cmp, allocator2);

    for (int i = 0; i < 100; ++i) set1.insert(MovableOnlyInstance(i));

    tracker.ResetCopiesMovesSwaps();
    set2 = std::move(set1);
    EXPECT_GE(tracker.moves(), 100);
  }
}

}  // namespace
}  // namespace container_internal
}  // namespace absl
