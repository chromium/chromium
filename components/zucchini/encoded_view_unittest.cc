// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/encoded_view.h"

#include <iterator>
#include <numeric>
#include <vector>

#include "components/zucchini/image_index.h"
#include "components/zucchini/test_disassembler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

constexpr size_t PADDING = kReferencePaddingProjection;

template <class It1, class It2>
void TestInputIterator(It1 first_expected,
                       It1 last_expected,
                       It2 first_input,
                       It2 last_input) {
  while (first_expected != last_expected && first_input != last_input) {
    EXPECT_EQ(*first_expected, *first_input);
    ++first_expected;
    ++first_input;
  }
  EXPECT_EQ(last_input, first_input);
  EXPECT_EQ(last_expected, first_expected);
}

template <class It1, class It2>
void TestForwardIterator(It1 first_expected,
                         It1 last_expected,
                         It2 first_input,
                         It2 last_input) {
  TestInputIterator(first_expected, last_expected, first_input, last_input);

  while (first_expected != last_expected && first_input != last_input) {
    EXPECT_EQ(*(first_expected++), *(first_input++));
  }
  EXPECT_EQ(last_input, first_input);
  EXPECT_EQ(last_expected, first_expected);
}

template <class It1, class It2>
void TestBidirectionalIterator(It1 first_expected,
                               It1 last_expected,
                               It2 first_input,
                               It2 last_input) {
  TestForwardIterator(first_expected, last_expected, first_input, last_input);

  while (first_expected != last_expected && first_input != last_input) {
    EXPECT_EQ(*(--last_expected), *(--last_input));
  }
  EXPECT_EQ(last_input, first_input);
  EXPECT_EQ(last_expected, first_expected);
}

template <class It1, class It2>
void TestRandomAccessIterator(It1 first_expected,
                              It1 last_expected,
                              It2 first_input,
                              It2 last_input) {
  TestBidirectionalIterator(first_expected, last_expected, first_input,
                            last_input);

  using difference_type = typename std::iterator_traits<It1>::difference_type;

  difference_type expected_size = last_expected - first_expected;
  difference_type input_size = last_input - first_input;
  EXPECT_EQ(expected_size, input_size);

  for (difference_type i = 0; i < expected_size; ++i) {
    EXPECT_EQ(*(first_expected + i), *(first_input + i));
    EXPECT_EQ(first_expected[i], first_input[i]);

    EXPECT_EQ(0 < i, first_input < first_input + i);
    EXPECT_EQ(0 > i, first_input > first_input + i);
    EXPECT_EQ(0 <= i, first_input <= first_input + i);
    EXPECT_EQ(0 >= i, first_input >= first_input + i);

    EXPECT_EQ(expected_size < i, last_input < first_input + i);
    EXPECT_EQ(expected_size > i, last_input > first_input + i);
    EXPECT_EQ(expected_size <= i, last_input <= first_input + i);
    EXPECT_EQ(expected_size >= i, last_input >= first_input + i);

    It2 input = first_input;
    input += i;
    EXPECT_EQ(*input, first_expected[i]);
    input -= i;
    EXPECT_EQ(first_input, input);
    input += i;

    EXPECT_EQ(0 < i, first_input < input);
    EXPECT_EQ(0 > i, first_input > input);
    EXPECT_EQ(0 <= i, first_input <= input);
    EXPECT_EQ(0 >= i, first_input >= input);

    EXPECT_EQ(expected_size < i, last_input < input);
    EXPECT_EQ(expected_size > i, last_input > input);
    EXPECT_EQ(expected_size <= i, last_input <= input);
    EXPECT_EQ(expected_size >= i, last_input >= input);
  }
}

}  // namespace

class EncodedViewTest : public testing::Test {
 protected:
  EncodedViewTest()
      : buffer_(20),
        image_index_(ConstBufferView(buffer_.data(), buffer_.size())) {
    std::iota(buffer_.begin(), buffer_.end(), 0);
    TestDisassembler disasm({2, TypeTag(0), PoolTag(0)},
                            {{1, 0}, {8, 1}, {10, 2}},
                            {4, TypeTag(1), PoolTag(0)}, {{3, 3}},
                            {3, TypeTag(2), PoolTag(1)}, {{12, 4}, {17, 5}});
    image_index_.Initialize(&disasm);
  }

  void CheckView(std::vector<size_t> expected,
                 const EncodedView& encoded_view) const {
    for (offset_t i = 0; i < encoded_view.size(); ++i) {
      EXPECT_EQ(expected[i], encoded_view.Projection(i)) << i;
    }
    TestRandomAccessIterator(expected.begin(), expected.end(),
                             encoded_view.begin(), encoded_view.end());
  }

  std::vector<uint8_t> buffer_;
  ImageIndex image_index_;
};

TEST_F(EncodedViewTest, Unlabeled) {
  EncodedView encoded_view(image_index_);

  encoded_view.SetLabels(PoolTag(0), {0, 0, 0, 0}, 1);
  encoded_view.SetLabels(PoolTag(1), {0, 0}, 1);

  std::vector<size_t> expected = {
      0,                                     // raw
      kBaseReferenceProjection + 0 + 0 * 3,  // ref 0
      PADDING,
      kBaseReferenceProjection + 1 + 0 * 3,  // ref 1
      PADDING,
      PADDING,
      PADDING,
      7,                                     // raw
      kBaseReferenceProjection + 0 + 0 * 3,  // ref 0
      PADDING,
      kBaseReferenceProjection + 0 + 0 * 3,  // ref 0
      PADDING,
      kBaseReferenceProjection + 2 + 0 * 3,  // ref 2
      PADDING,
      PADDING,
      15,  // raw
      16,
      kBaseReferenceProjection + 2 + 0 * 3,  // ref 2
      PADDING,
      PADDING,
  };
  EXPECT_EQ(kBaseReferenceProjection + 3 * 1, encoded_view.Cardinality());
  CheckView(expected, encoded_view);
}

TEST_F(EncodedViewTest, Labeled) {
  EncodedView encoded_view(image_index_);

  encoded_view.SetLabels(PoolTag(0), {0, 2, 1, 2}, 3);
  encoded_view.SetLabels(PoolTag(1), {0, 0}, 1);

  std::vector<size_t> expected = {
      0,                                     // raw
      kBaseReferenceProjection + 0 + 0 * 3,  // ref 0
      PADDING,
      kBaseReferenceProjection + 1 + 2 * 3,  // ref 1
      PADDING,
      PADDING,
      PADDING,
      7,                                     // raw
      kBaseReferenceProjection + 0 + 2 * 3,  // ref 0
      PADDING,
      kBaseReferenceProjection + 0 + 1 * 3,  // ref 0
      PADDING,
      kBaseReferenceProjection + 2 + 0 * 3,  // ref 2
      PADDING,
      PADDING,
      15,  // raw
      16,
      kBaseReferenceProjection + 2 + 0 * 3,  // ref 2
      PADDING,
      PADDING,
  };
  EXPECT_EQ(kBaseReferenceProjection + 3 * 3, encoded_view.Cardinality());
  CheckView(expected, encoded_view);
}

}  // namespace zucchini
