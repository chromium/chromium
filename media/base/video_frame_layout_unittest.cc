// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_layout.h"

#include <stddef.h>
#include <stdint.h>

#include <sstream>
#include <string>
#include <utility>

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {

namespace {

std::vector<ColorPlaneLayout> CreatePlanes(const std::vector<int32_t>& strides,
                                           const std::vector<size_t>& offsets,
                                           const std::vector<size_t>& sizes) {
  LOG_ASSERT(strides.size() == offsets.size());
  std::vector<ColorPlaneLayout> planes(strides.size());
  for (size_t i = 0; i < strides.size(); i++) {
    planes[i].stride = strides[i];
    planes[i].offset = offsets[i];
    planes[i].size = sizes[i];
  }
  return planes;
}

}  // namespace

TEST(VideoFrameLayout, CreateI420) {
  gfx::Size coded_size = gfx::Size(320, 180);
  auto layout = VideoFrameLayout::Create(PIXEL_FORMAT_I420, coded_size);
  ASSERT_TRUE(layout.has_value());

  auto num_of_planes = VideoFrame::NumPlanes(PIXEL_FORMAT_I420);
  EXPECT_EQ(layout->format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout->coded_size(), coded_size);
  EXPECT_EQ(layout->num_planes(), num_of_planes);
  EXPECT_EQ(layout->is_multi_planar(), false);
  for (size_t i = 0; i < num_of_planes; ++i) {
    EXPECT_EQ(layout->planes()[i].stride, 0);
    EXPECT_EQ(layout->planes()[i].offset, 0u);
    EXPECT_EQ(layout->planes()[i].size, 0u);
  }
}

TEST(VideoFrameLayout, CreateNV12) {
  gfx::Size coded_size = gfx::Size(320, 180);
  auto layout = VideoFrameLayout::Create(PIXEL_FORMAT_NV12, coded_size);
  ASSERT_TRUE(layout.has_value());

  auto num_of_planes = VideoFrame::NumPlanes(PIXEL_FORMAT_NV12);
  EXPECT_EQ(layout->format(), PIXEL_FORMAT_NV12);
  EXPECT_EQ(layout->coded_size(), coded_size);
  EXPECT_EQ(layout->num_planes(), num_of_planes);
  EXPECT_EQ(layout->is_multi_planar(), false);
  for (size_t i = 0; i < num_of_planes; ++i) {
    EXPECT_EQ(layout->planes()[i].stride, 0);
    EXPECT_EQ(layout->planes()[i].offset, 0u);
    EXPECT_EQ(layout->planes()[i].size, 0u);
  }
}

TEST(VideoFrameLayout, CreateWithStrides) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  auto layout = VideoFrameLayout::CreateWithStrides(PIXEL_FORMAT_I420,
                                                    coded_size, strides);
  ASSERT_TRUE(layout.has_value());

  EXPECT_EQ(layout->format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout->coded_size(), coded_size);
  EXPECT_EQ(layout->num_planes(), 3u);
  EXPECT_EQ(layout->is_multi_planar(), false);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(layout->planes()[i].stride, strides[i]);
    EXPECT_EQ(layout->planes()[i].offset, 0u);
    EXPECT_EQ(layout->planes()[i].size, 0u);
  }
}

TEST(VideoFrameLayout, CreateWithPlanes) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 0, 200};
  std::vector<size_t> sizes = {200, 100, 100};
  auto layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets, sizes));
  ASSERT_TRUE(layout.has_value());

  EXPECT_EQ(layout->format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout->coded_size(), coded_size);
  EXPECT_EQ(layout->num_planes(), 3u);
  EXPECT_EQ(layout->is_multi_planar(), false);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(layout->planes()[i].stride, strides[i]);
    EXPECT_EQ(layout->planes()[i].offset, offsets[i]);
    EXPECT_EQ(layout->planes()[i].size, sizes[i]);
  }
}

TEST(VideoFrameLayout, CreateMultiPlanar) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 100, 200};
  std::vector<size_t> sizes = {90, 40, 40};
  auto layout = VideoFrameLayout::CreateMultiPlanar(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets, sizes));
  ASSERT_TRUE(layout.has_value());

  EXPECT_EQ(layout->format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout->coded_size(), coded_size);
  EXPECT_EQ(layout->num_planes(), 3u);
  EXPECT_EQ(layout->is_multi_planar(), true);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(layout->planes()[i].stride, strides[i]);
    EXPECT_EQ(layout->planes()[i].offset, offsets[i]);
    EXPECT_EQ(layout->planes()[i].size, sizes[i]);
  }
}

TEST(VideoFrameLayout, CopyConstructor) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 0, 200};
  std::vector<size_t> sizes = {200, 100, 100};
  auto layout = VideoFrameLayout::CreateMultiPlanar(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets, sizes));
  ASSERT_TRUE(layout.has_value());

  VideoFrameLayout layout_clone(*layout);
  EXPECT_EQ(layout_clone.format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout_clone.coded_size(), coded_size);
  EXPECT_EQ(layout_clone.num_planes(), 3u);
  EXPECT_EQ(layout_clone.is_multi_planar(), true);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(layout_clone.planes()[i].stride, strides[i]);
    EXPECT_EQ(layout_clone.planes()[i].offset, offsets[i]);
    EXPECT_EQ(layout_clone.planes()[i].size, sizes[i]);
  }
}

TEST(VideoFrameLayout, CopyAssignmentOperator) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 100, 200};
  std::vector<size_t> sizes = {90, 45, 45};
  auto layout = VideoFrameLayout::CreateMultiPlanar(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets, sizes));
  ASSERT_TRUE(layout.has_value());

  VideoFrameLayout layout_clone = *layout;
  EXPECT_EQ(layout_clone.format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout_clone.coded_size(), coded_size);
  EXPECT_EQ(layout_clone.num_planes(), 3u);
  EXPECT_EQ(layout_clone.is_multi_planar(), true);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(layout_clone.planes()[i].stride, strides[i]);
    EXPECT_EQ(layout_clone.planes()[i].offset, offsets[i]);
    EXPECT_EQ(layout_clone.planes()[i].size, sizes[i]);
  }
}

TEST(VideoFrameLayout, MoveConstructor) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 0, 100};
  std::vector<size_t> sizes = {90, 45, 45};
  auto layout = VideoFrameLayout::CreateMultiPlanar(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets, sizes));
  ASSERT_TRUE(layout.has_value());

  VideoFrameLayout layout_move(std::move(*layout));

  EXPECT_EQ(layout_move.format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout_move.coded_size(), coded_size);
  EXPECT_EQ(layout_move.num_planes(), 3u);
  EXPECT_EQ(layout_move.is_multi_planar(), true);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(layout_move.planes()[i].stride, strides[i]);
    EXPECT_EQ(layout_move.planes()[i].offset, offsets[i]);
    EXPECT_EQ(layout_move.planes()[i].size, sizes[i]);
  }

  // Members in object being moved are cleared except const members.
  EXPECT_EQ(layout->format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout->coded_size(), coded_size);
  EXPECT_EQ(layout->num_planes(), 0u);
}

TEST(VideoFrameLayout, ToStringWithPlanes) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  auto layout = VideoFrameLayout::CreateWithStrides(PIXEL_FORMAT_I420,
                                                    coded_size, strides);
  ASSERT_TRUE(layout.has_value());

  std::ostringstream ostream;
  ostream << *layout;
  const std::string kNoModifier =
      std::to_string(gfx::NativePixmapHandle::kNoModifier);
  EXPECT_EQ(ostream.str(),
            "VideoFrameLayout(format: PIXEL_FORMAT_I420, coded_size: 320x180, "
            "planes (stride, offset, size): [(384, 0, 0), (192, 0, 0), "
            "(192, 0, 0)], is_multi_planar: 0, buffer_addr_align: 32, "
            "modifier: " +
                kNoModifier + ")");
}

TEST(VideoFrameLayout, ToStringMultiPlanar) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192};
  std::vector<size_t> offsets = {0, 100};
  std::vector<size_t> sizes = {100, 100};
  auto layout = VideoFrameLayout::CreateMultiPlanar(
      PIXEL_FORMAT_NV12, coded_size, CreatePlanes(strides, offsets, sizes));
  ASSERT_TRUE(layout.has_value());

  std::ostringstream ostream;
  ostream << *layout;
  const std::string kNoModifier =
      std::to_string(gfx::NativePixmapHandle::kNoModifier);
  EXPECT_EQ(ostream.str(),
            "VideoFrameLayout(format: PIXEL_FORMAT_NV12, coded_size: 320x180, "
            "planes (stride, offset, size): [(384, 0, 100), (192, 100, 100)], "
            "is_multi_planar: 1, buffer_addr_align: 32, "
            "modifier: " +
                kNoModifier + ")");
}

TEST(VideoFrameLayout, ToString) {
  gfx::Size coded_size = gfx::Size(320, 180);
  auto layout = VideoFrameLayout::Create(PIXEL_FORMAT_NV12, coded_size);
  ASSERT_TRUE(layout.has_value());

  std::ostringstream ostream;
  ostream << *layout;
  const std::string kNoModifier =
      std::to_string(gfx::NativePixmapHandle::kNoModifier);
  EXPECT_EQ(ostream.str(),
            "VideoFrameLayout(format: PIXEL_FORMAT_NV12, coded_size: 320x180, "
            "planes (stride, offset, size): [(0, 0, 0), (0, 0, 0)], "
            "is_multi_planar: 0, buffer_addr_align: 32, "
            "modifier: " +
                kNoModifier + ")");
}

TEST(VideoFrameLayout, EqualOperator) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 200, 300};
  std::vector<size_t> sizes = {200, 100, 100};
  const size_t align = VideoFrameLayout::kBufferAddressAlignment;
  const uint64_t modifier = 1;

  auto layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets, sizes),
      align, modifier);
  ASSERT_TRUE(layout.has_value());

  auto same_layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets, sizes),
      align, modifier);
  ASSERT_TRUE(same_layout.has_value());
  EXPECT_EQ(*layout, *same_layout);

  std::vector<size_t> another_sizes = {190, 100, 100};
  auto different_layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_I420, coded_size,
      CreatePlanes(strides, offsets, another_sizes), align, modifier);
  ASSERT_TRUE(different_layout.has_value());
  EXPECT_NE(*layout, *different_layout);

  different_layout = VideoFrameLayout::CreateMultiPlanar(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets, sizes),
      align, modifier);
  ASSERT_TRUE(different_layout.has_value());
  EXPECT_NE(*layout, *different_layout);

  const size_t another_align = 0x1000;
  different_layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets, sizes),
      another_align, modifier);
  ASSERT_TRUE(different_layout.has_value());
  EXPECT_NE(*layout, *different_layout);

  const size_t another_modifier = 2;
  different_layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets, sizes),
      align, another_modifier);
  ASSERT_TRUE(different_layout.has_value());
  EXPECT_NE(*layout, *different_layout);
}

}  // namespace media
