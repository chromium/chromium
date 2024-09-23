// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/buffer_view.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <type_traits>
#include <vector>

#include "base/test/gtest_util.h"
#include "components/zucchini/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

class BufferViewTest : public testing::Test {
 protected:
  // Some tests might modify this.
  std::vector<uint8_t> bytes_ = ParseHexString("10 32 54 76 98 BA DC FE 10 00");
};

TEST_F(BufferViewTest, Size) {
  for (size_t len = 0; len <= bytes_.size(); ++len) {
    EXPECT_EQ(len, ConstBufferView(bytes_.data(), len).size());
    EXPECT_EQ(len, MutableBufferView(bytes_.data(), len).size());
  }
}

TEST_F(BufferViewTest, Empty) {
  // Empty view.
  EXPECT_TRUE(ConstBufferView(bytes_.data(), 0).empty());
  EXPECT_TRUE(MutableBufferView(bytes_.data(), 0).empty());

  for (size_t len = 1; len <= bytes_.size(); ++len) {
    EXPECT_FALSE(ConstBufferView(bytes_.data(), len).empty());
    EXPECT_FALSE(MutableBufferView(bytes_.data(), len).empty());
  }
}

TEST_F(BufferViewTest, FromRange) {
  constexpr size_t kSize = 10;
  uint8_t raw_data[kSize] = {0x10, 0x32, 0x54, 0x76, 0x98,
                             0xBA, 0xDC, 0xFE, 0x10, 0x00};
  ConstBufferView buffer =
      ConstBufferView::FromRange(std::begin(raw_data), std::end(raw_data));
  EXPECT_EQ(bytes_.size(), buffer.size());
  EXPECT_EQ(std::begin(raw_data), buffer.begin());

  MutableBufferView mutable_buffer =
      MutableBufferView::FromRange(std::begin(raw_data), std::end(raw_data));
  EXPECT_EQ(bytes_.size(), mutable_buffer.size());
  EXPECT_EQ(std::begin(raw_data), mutable_buffer.begin());

#if GTEST_HAS_DEATH_TEST
  EXPECT_DCHECK_DEATH(
      ConstBufferView::FromRange(std::end(raw_data), std::begin(raw_data)));

  EXPECT_DCHECK_DEATH(MutableBufferView::FromRange(std::begin(raw_data) + 1,
                                                   std::begin(raw_data)));
#endif
}

TEST_F(BufferViewTest, Subscript) {
  ConstBufferView view(bytes_.data(), bytes_.size());

  EXPECT_EQ(0x10, view[0]);
  static_assert(!std::is_assignable<decltype(view[0]), uint8_t>::value,
                "BufferView values should not be mutable.");

  MutableBufferView mutable_view(bytes_.data(), bytes_.size());

  EXPECT_EQ(bytes_.data(), &mutable_view[0]);
  mutable_view[0] = 42;
  EXPECT_EQ(42, mutable_view[0]);
}

TEST_F(BufferViewTest, SubRegion) {
  ConstBufferView view(bytes_.data(), bytes_.size());

  ConstBufferView sub_view = view[{2, 4}];
  EXPECT_EQ(view.begin() + 2, sub_view.begin());
  EXPECT_EQ(size_t(4), sub_view.size());
}

TEST_F(BufferViewTest, Shrink) {
  ConstBufferView buffer(bytes_.data(), bytes_.size());

  buffer.shrink(bytes_.size());
  EXPECT_EQ(bytes_.size(), buffer.size());
  buffer.shrink(2);
  EXPECT_EQ(size_t(2), buffer.size());
#if GTEST_HAS_DEATH_TEST
  EXPECT_DCHECK_DEATH(buffer.shrink(bytes_.size()));
#endif
}

TEST_F(BufferViewTest, Read) {
  ConstBufferView buffer(bytes_.data(), bytes_.size());

  EXPECT_EQ(0x10U, buffer.read<uint8_t>(0));
  EXPECT_EQ(0x54U, buffer.read<uint8_t>(2));

  EXPECT_EQ(0x3210U, buffer.read<uint16_t>(0));
  EXPECT_EQ(0x7654U, buffer.read<uint16_t>(2));

  EXPECT_EQ(0x76543210U, buffer.read<uint32_t>(0));
  EXPECT_EQ(0xBA987654U, buffer.read<uint32_t>(2));

  EXPECT_EQ(0xFEDCBA9876543210ULL, buffer.read<uint64_t>(0));

  EXPECT_EQ(0x00, buffer.read<uint8_t>(9));
#if GTEST_HAS_DEATH_TEST
  EXPECT_DEATH(buffer.read<uint8_t>(10), "");
#endif

  EXPECT_EQ(0x0010FEDCU, buffer.read<uint32_t>(6));
#if GTEST_HAS_DEATH_TEST
  EXPECT_DEATH(buffer.read<uint32_t>(7), "");
#endif
}

TEST_F(BufferViewTest, Write) {
  MutableBufferView buffer(bytes_.data(), bytes_.size());

  buffer.write<uint32_t>(0, 0x01234567);
  buffer.write<uint32_t>(4, 0x89ABCDEF);
  EXPECT_EQ(ParseHexString("67 45 23 01 EF CD AB 89 10 00"),
            std::vector<uint8_t>(buffer.begin(), buffer.end()));

  buffer.write<uint8_t>(9, 0xFF);
#if GTEST_HAS_DEATH_TEST
  EXPECT_DEATH(buffer.write<uint8_t>(10, 0xFF), "");
#endif

  buffer.write<uint32_t>(6, 0xFFFFFFFF);
#if GTEST_HAS_DEATH_TEST
  EXPECT_DEATH(buffer.write<uint32_t>(7, 0xFFFFFFFF), "");
#endif
}

TEST_F(BufferViewTest, CanAccess) {
  MutableBufferView buffer(bytes_.data(), bytes_.size());
  EXPECT_TRUE(buffer.can_access<uint32_t>(0));
  EXPECT_TRUE(buffer.can_access<uint32_t>(6));
  EXPECT_FALSE(buffer.can_access<uint32_t>(7));
  EXPECT_FALSE(buffer.can_access<uint32_t>(10));
  EXPECT_FALSE(buffer.can_access<uint32_t>(0xFFFFFFFFU));

  EXPECT_TRUE(buffer.can_access<uint8_t>(0));
  EXPECT_TRUE(buffer.can_access<uint8_t>(7));
  EXPECT_TRUE(buffer.can_access<uint8_t>(9));
  EXPECT_FALSE(buffer.can_access<uint8_t>(10));
  EXPECT_FALSE(buffer.can_access<uint8_t>(0xFFFFFFFF));
}

TEST_F(BufferViewTest, LocalRegion) {
  ConstBufferView view(bytes_.data(), bytes_.size());

  BufferRegion region = view.local_region();
  EXPECT_EQ(0U, region.offset);
  EXPECT_EQ(bytes_.size(), region.size);
}

TEST_F(BufferViewTest, Covers) {
  EXPECT_TRUE(ConstBufferView().covers({0, 0}));
  EXPECT_FALSE(ConstBufferView().covers({0, 1}));

  ConstBufferView view(bytes_.data(), bytes_.size());

  EXPECT_TRUE(view.covers({0, 0}));
  EXPECT_TRUE(view.covers({0, 1}));
  EXPECT_TRUE(view.covers({0, bytes_.size()}));
  EXPECT_FALSE(view.covers({0, bytes_.size() + 1}));
  EXPECT_FALSE(view.covers({1, bytes_.size()}));

  EXPECT_TRUE(view.covers({bytes_.size() - 1, 0}));
  EXPECT_TRUE(view.covers({bytes_.size() - 1, 1}));
  EXPECT_FALSE(view.covers({bytes_.size() - 1, 2}));
  EXPECT_TRUE(view.covers({bytes_.size(), 0}));
  EXPECT_FALSE(view.covers({bytes_.size(), 1}));
  EXPECT_FALSE(view.covers({bytes_.size() + 1, 0}));
  EXPECT_FALSE(view.covers({bytes_.size() + 1, 1}));

  EXPECT_FALSE(view.covers({1, size_t(-1)}));
  EXPECT_FALSE(view.covers({size_t(-1), 1}));
  EXPECT_FALSE(view.covers({size_t(-1), size_t(-1)}));
}

TEST_F(BufferViewTest, CoversArray) {
  ConstBufferView view(bytes_.data(), bytes_.size());

  for (uint32_t i = 1; i <= bytes_.size(); ++i) {
    EXPECT_TRUE(view.covers_array(0, 1, i));
    EXPECT_TRUE(view.covers_array(0, i, 1));
    EXPECT_TRUE(view.covers_array(0, i, bytes_.size() / i));
    EXPECT_TRUE(view.covers_array(0, bytes_.size() / i, i));
    if (i < bytes_.size()) {
      EXPECT_TRUE(view.covers_array(i, 1, bytes_.size() - i));
      EXPECT_TRUE(view.covers_array(i, bytes_.size() - i, 1));
    }
    EXPECT_TRUE(view.covers_array(bytes_.size() - (bytes_.size() / i) * i, 1,
                                  bytes_.size() / i));
  }

  EXPECT_TRUE(view.covers_array(0, 0, bytes_.size()));
  EXPECT_TRUE(view.covers_array(bytes_.size() - 1, 0, bytes_.size()));
  EXPECT_TRUE(view.covers_array(bytes_.size(), 0, bytes_.size()));
  EXPECT_TRUE(view.covers_array(0, 0, 0x10000));
  EXPECT_TRUE(view.covers_array(bytes_.size() - 1, 0, 0x10000));
  EXPECT_TRUE(view.covers_array(bytes_.size(), 0, 0x10000));

  EXPECT_FALSE(view.covers_array(0, 1, bytes_.size() + 1));
  EXPECT_FALSE(view.covers_array(0, 2, bytes_.size()));
  EXPECT_FALSE(view.covers_array(0, bytes_.size() + 11, 1));
  EXPECT_FALSE(view.covers_array(0, bytes_.size(), 2));
  EXPECT_FALSE(view.covers_array(1, bytes_.size(), 1));

  EXPECT_FALSE(view.covers_array(bytes_.size(), 1, 1));
  EXPECT_TRUE(view.covers_array(bytes_.size(), 0, 1));
  EXPECT_FALSE(view.covers_array(0, 0x10000, 0x10000));
}

TEST_F(BufferViewTest, Equals) {
  // Almost identical to |bytes_|, except at 2 places:         v  v
  std::vector<uint8_t> bytes2 = ParseHexString("10 32 54 76 98 AB CD FE 10 00");
  ConstBufferView view1(bytes_.data(), bytes_.size());
  ConstBufferView view2(&bytes2[0], bytes2.size());

  EXPECT_TRUE(view1.equals(view1));
  EXPECT_TRUE(view2.equals(view2));
  EXPECT_FALSE(view1.equals(view2));
  EXPECT_FALSE(view2.equals(view1));

  EXPECT_TRUE((view1[{0, 0}]).equals(view2[{0, 0}]));
  EXPECT_TRUE((view1[{0, 0}]).equals(view2[{5, 0}]));
  EXPECT_TRUE((view1[{0, 5}]).equals(view2[{0, 5}]));
  EXPECT_FALSE((view1[{0, 6}]).equals(view2[{0, 6}]));
  EXPECT_FALSE((view1[{0, 7}]).equals(view1[{0, 6}]));
  EXPECT_TRUE((view1[{5, 3}]).equals(view1[{5, 3}]));
  EXPECT_FALSE((view1[{5, 1}]).equals(view1[{5, 3}]));
  EXPECT_TRUE((view2[{0, 1}]).equals(view2[{8, 1}]));
  EXPECT_FALSE((view2[{1, 1}]).equals(view2[{8, 1}]));
}

TEST_F(BufferViewTest, AlignOn) {
  using size_type = ConstBufferView::size_type;
  ConstBufferView image(bytes_.data(), bytes_.size());
  ConstBufferView view = image;
  ASSERT_EQ(10U, view.size());

  auto get_pos = [&image, &view]() -> size_type {
    EXPECT_TRUE(view.begin() >= image.begin());  // Iterator compare.
    return static_cast<size_type>(view.begin() - image.begin());
  };

  EXPECT_EQ(0U, get_pos());
  view.remove_prefix(1U);
  EXPECT_EQ(1U, get_pos());
  view.remove_prefix(4U);
  EXPECT_EQ(5U, get_pos());

  // Align.
  EXPECT_TRUE(view.AlignOn(image, 1U));  // Trival case.
  EXPECT_EQ(5U, get_pos());

  EXPECT_TRUE(view.AlignOn(image, 2U));
  EXPECT_EQ(6U, get_pos());
  EXPECT_TRUE(view.AlignOn(image, 2U));
  EXPECT_EQ(6U, get_pos());

  EXPECT_TRUE(view.AlignOn(image, 4U));
  EXPECT_EQ(8U, get_pos());
  EXPECT_TRUE(view.AlignOn(image, 2U));
  EXPECT_EQ(8U, get_pos());

  view.remove_prefix(1U);
  EXPECT_EQ(9U, get_pos());

  // Pos is at 9, align to 4 would yield 12, but size is 10, so this fails.
  EXPECT_FALSE(view.AlignOn(image, 4U));
  EXPECT_EQ(9U, get_pos());
  EXPECT_TRUE(view.AlignOn(image, 2U));
  EXPECT_EQ(10U, get_pos());
}

}  // namespace zucchini
