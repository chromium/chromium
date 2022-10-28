// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/third_party/bsdiff/paged_array.h"

#include <stdint.h>

#include <iterator>
#include <random>
#include <vector>

#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if !defined(ADDRESS_SANITIZER) || !BUILDFLAG(IS_WIN)
// Total allocation of 4GB will fail in 32 bit programs if allocations are
// leaked.
const int kIterations = 20;
const int kSizeBig = 200 * 1024 * 1024 / sizeof(int);  // 200MB
#endif

const size_t kLogBlockSizeSmall = 10;
const size_t kBlockSizeSmall = 1 << kLogBlockSizeSmall;
const size_t kSizeList[] = {1,
                            16,
                            123,
                            kBlockSizeSmall,
                            kBlockSizeSmall + 1,
                            123 * kBlockSizeSmall + 567};

const size_t kIteratorTestDataSize = 7;
const int32_t kIteratorTestData[kIteratorTestDataSize] = {2,  3,  5, 7,
                                                          11, 13, 17};

}  // namespace

class PagedArrayTest : public testing::Test {
 public:
  template <typename Iterator>
  void TestIteratorBasic(Iterator begin, Iterator end) {
    EXPECT_FALSE(begin == nullptr);
    EXPECT_FALSE(end == nullptr);

    // Test TestPagedArray::iterator data read.
    Iterator it = begin;
    EXPECT_EQ(2, *it);
    EXPECT_EQ(3, *(it + 1));
    EXPECT_EQ(5, it[2]);  // Pseudo-pointer.
    EXPECT_EQ(7, (it + 1)[2]);
    EXPECT_EQ(3, *(++it));
    EXPECT_EQ(3, *it);
    EXPECT_EQ(3, *(it++));
    EXPECT_EQ(5, *it);
    EXPECT_EQ(11, *(it += 2));
    EXPECT_EQ(11, *it);
    EXPECT_FALSE(it == begin);
    EXPECT_TRUE(it != begin);
    EXPECT_TRUE(it == (begin + 4));
    EXPECT_FALSE(it != (begin + 4));
    EXPECT_EQ(static_cast<ptrdiff_t>(kIteratorTestDataSize), end - begin);
    it = begin + 5;
    EXPECT_EQ(begin, it - 5);
    EXPECT_EQ(13, *it);
    EXPECT_EQ(11, *(--it));
    EXPECT_EQ(11, *it);
    EXPECT_EQ(11, *(it--));
    EXPECT_EQ(7, *it);
    EXPECT_EQ(3, *(it -= 2));
    EXPECT_EQ(3, *it);

    // Test binary operators for every pair of iterator.
    EXPECT_TRUE(begin <= end);
    EXPECT_TRUE(begin < end);
    for (size_t i = 0; i < kIteratorTestDataSize; ++i) {
      Iterator it1 = begin + i;
      EXPECT_TRUE(it1 == it1);
      EXPECT_FALSE(it1 != it1);
      EXPECT_TRUE(begin <= it1);
      EXPECT_FALSE(begin > it1);
      EXPECT_TRUE(it1 >= begin);
      EXPECT_FALSE(it1 < begin);
      EXPECT_EQ(begin, it1 - i);
      EXPECT_EQ(end, it1 + (kIteratorTestDataSize - i));
      EXPECT_EQ(kIteratorTestData[i], *it1);
      EXPECT_EQ(kIteratorTestData[i], begin[i]);
      for (size_t j = 0; i + j < kIteratorTestDataSize; ++j) {
        Iterator it2 = it1 + j;
        EXPECT_TRUE(it1 <= it2);
        EXPECT_FALSE(it1 > it2);
        EXPECT_TRUE(it2 >= it1);
        EXPECT_FALSE(it2 < it1);
        if (j > 0) {
          EXPECT_TRUE(it1 < it2);
          EXPECT_FALSE(it1 >= it2);
          EXPECT_TRUE(it2 > it1);
          EXPECT_FALSE(it2 <= it1);
          EXPECT_FALSE(it1 == it2);
          EXPECT_TRUE(it1 != it2);
        }
        EXPECT_EQ(kIteratorTestData[i + j], it1[j]);  // Pseudo-pointer.
        EXPECT_EQ(kIteratorTestData[i + j], *it2);
        EXPECT_EQ(static_cast<ptrdiff_t>(j), it2 - it1);
        EXPECT_EQ(it1, it2 - j);
      }
      EXPECT_TRUE(it1 < end);
      EXPECT_FALSE(it1 >= end);
      EXPECT_TRUE(it1 <= end);
      EXPECT_FALSE(it1 > end);
      EXPECT_TRUE(end > it1);
      EXPECT_FALSE(end <= it1);
      EXPECT_TRUE(end >= it1);
      EXPECT_FALSE(end < it1);
      EXPECT_TRUE(it1 != end);
      EXPECT_FALSE(it1 == end);
    }

    // Test initialize with null.
    Iterator it_dummy;
    it_dummy = nullptr;
    EXPECT_TRUE(it_dummy == nullptr);

    // Test copy constructor.
    Iterator begin_copy(begin);
    EXPECT_TRUE(begin_copy == begin);

    // Test STL read-only usage.
    std::vector<typename std::iterator_traits<Iterator>::value_type> v(begin,
                                                                       end);
    EXPECT_TRUE(std::equal(begin, end, v.begin()));
    EXPECT_TRUE(std::equal(v.begin(), v.end(), begin));
  }
};

// AddressSanitizer on Windows adds additional memory overhead, which
// causes these tests to go OOM and fail.
#if !defined(ADDRESS_SANITIZER) || !BUILDFLAG(IS_WIN)
TEST_F(PagedArrayTest, TestManyAllocationsDestructorFree) {
  for (int i = 0; i < kIterations; ++i) {
    courgette::PagedArray<int> a;
    EXPECT_TRUE(a.Allocate(kSizeBig));
  }
}

TEST_F(PagedArrayTest, TestManyAllocationsManualFree) {
  courgette::PagedArray<int> a;
  for (int i = 0; i < kIterations; ++i) {
    EXPECT_TRUE(a.Allocate(kSizeBig));
    a.clear();
  }
}
#endif

TEST_F(PagedArrayTest, TestAccess) {
  for (size_t size : kSizeList) {
    courgette::PagedArray<int32_t, kLogBlockSizeSmall> a;
    EXPECT_TRUE(a.Allocate(size));
    for (size_t i = 0; i < size; ++i)
      a[i] = i;
    for (size_t i = 0; i < size; ++i)
      EXPECT_EQ(static_cast<int32_t>(i), a[i]);
  }
}

TEST_F(PagedArrayTest, TestIterator) {
  using TestPagedArray = courgette::PagedArray<int32_t, 1>;  // Page size = 2.

  TestPagedArray::const_iterator uninit_const_it;
  EXPECT_TRUE(uninit_const_it == nullptr);

  TestPagedArray::iterator uninit_it;
  EXPECT_TRUE(uninit_it == nullptr);

  TestPagedArray a;
  EXPECT_TRUE(a.Allocate(kIteratorTestDataSize));
  base::ranges::copy(kIteratorTestData, a.begin());
  const TestPagedArray& a_const = a;

  // Test TestPagedArray::const_iterator.
  TestIteratorBasic(a_const.begin(), a_const.end());

  // Test TestPagedArray::iterator.
  TestIteratorBasic(a.begin(), a.end());

  // Test equality of const and non-const.
  EXPECT_TRUE(a.begin() == a_const.begin());
  EXPECT_TRUE(a_const.begin() == a.begin());

  // Test casting from non-const to const.
  TestPagedArray::iterator non_const_it = a.begin();
  EXPECT_TRUE(non_const_it == a.begin());
  TestPagedArray::const_iterator const_it = non_const_it;
  EXPECT_TRUE(const_it == non_const_it);
  // The following should and will emit compile error:
  // non_const_it = a_const.begin();

  // Test copy constructor from non-const to const.
  TestPagedArray::iterator const_it2(a.begin());
  EXPECT_TRUE(const_it2 == a.begin());

  // Test pointer distance from non-const to const.
  EXPECT_EQ(static_cast<ptrdiff_t>(kIteratorTestDataSize),
            a.end() - a_const.begin());
  EXPECT_EQ(static_cast<ptrdiff_t>(kIteratorTestDataSize),
            a_const.end() - a.begin());

  // Test operator->().
  struct TestStruct {
    int32_t value = 0;
  };
  using TestStructPagedArray = courgette::PagedArray<TestStruct, 1>;
  TestStructPagedArray b;
  b.Allocate(3);
  b[0].value = 100;
  b[1].value = 200;
  b[2].value = 300;
  const TestStructPagedArray& b_const = b;

  EXPECT_EQ(100, b.begin()->value);
  EXPECT_EQ(100, b_const.begin()->value);
  EXPECT_EQ(200, (b.begin() + 1)->value);
  EXPECT_EQ(200, (b_const.begin() + 1)->value);
  (b.begin() + 2)->value *= -1;
  EXPECT_EQ(-300, (b.begin() + 2)->value);
  EXPECT_EQ(-300, (b_const.begin() + 2)->value);
}

// Test generic read-write of itrators by sorting pseudo-random numbers.
TEST_F(PagedArrayTest, TestSort) {
  std::minstd_rand pseudo_rand_gen;  // Deterministic, using defaults.
  for (size_t size : kSizeList) {
    std::vector<int32_t> v(size);
    courgette::PagedArray<int32_t, kLogBlockSizeSmall> a;
    EXPECT_TRUE(a.Allocate(size));
    for (size_t i = 0; i < size; ++i) {
      v[i] = pseudo_rand_gen();
      a[i] = v[i];
    }
    std::sort(v.begin(), v.end());
    std::sort(a.begin(), a.end());
    for (size_t i = 0; i < size; ++i)
      EXPECT_EQ(v[i], a[i]);
  }
}
