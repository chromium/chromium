// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_layout.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

std::vector<VideoFrameLayout::Plane> CreatePlanes(
    const std::vector<int32_t>& strides,
    const std::vector<size_t>& offsets) {
  LOG_ASSERT(strides.size() == offsets.size());
  std::vector<VideoFrameLayout::Plane> planes(strides.size());
  for (size_t i = 0; i < strides.size(); i++) {
    planes[i].stride = strides[i];
    planes[i].offset = offsets[i];
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
  EXPECT_EQ(layout->GetTotalBufferSize(), 0u);
  EXPECT_EQ(layout->num_planes(), num_of_planes);
  EXPECT_EQ(layout->num_buffers(), 0u);
  for (size_t i = 0; i < num_of_planes; ++i) {
    EXPECT_EQ(layout->planes()[i].stride, 0);
    EXPECT_EQ(layout->planes()[i].offset, 0u);
  }
}

TEST(VideoFrameLayout, CreateNV12) {
  gfx::Size coded_size = gfx::Size(320, 180);
  auto layout = VideoFrameLayout::Create(PIXEL_FORMAT_NV12, coded_size);
  ASSERT_TRUE(layout.has_value());

  auto num_of_planes = VideoFrame::NumPlanes(PIXEL_FORMAT_NV12);
  EXPECT_EQ(layout->format(), PIXEL_FORMAT_NV12);
  EXPECT_EQ(layout->coded_size(), coded_size);
  EXPECT_EQ(layout->GetTotalBufferSize(), 0u);
  EXPECT_EQ(layout->num_planes(), num_of_planes);
  EXPECT_EQ(layout->num_buffers(), 0u);
  for (size_t i = 0; i < num_of_planes; ++i) {
    EXPECT_EQ(layout->planes()[i].stride, 0);
    EXPECT_EQ(layout->planes()[i].offset, 0u);
  }
}

TEST(VideoFrameLayout, CreateWithStrides) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> buffer_sizes = {73728, 18432, 18432};
  auto layout = VideoFrameLayout::CreateWithStrides(
      PIXEL_FORMAT_I420, coded_size, strides, buffer_sizes);
  ASSERT_TRUE(layout.has_value());

  EXPECT_EQ(layout->format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout->coded_size(), coded_size);
  EXPECT_EQ(layout->num_planes(), 3u);
  EXPECT_EQ(layout->num_buffers(), 3u);
  EXPECT_EQ(layout->GetTotalBufferSize(), 110592u);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(layout->planes()[i].stride, strides[i]);
    EXPECT_EQ(layout->planes()[i].offset, 0u);
    EXPECT_EQ(layout->buffer_sizes()[i], buffer_sizes[i]);
  }
}

TEST(VideoFrameLayout, CreateWithStridesNoBufferSizes) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  auto layout = VideoFrameLayout::CreateWithStrides(PIXEL_FORMAT_I420,
                                                    coded_size, strides);
  ASSERT_TRUE(layout.has_value());

  EXPECT_EQ(layout->format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout->coded_size(), coded_size);
  EXPECT_EQ(layout->num_planes(), 3u);
  EXPECT_EQ(layout->num_buffers(), 0u);
  EXPECT_EQ(layout->GetTotalBufferSize(), 0u);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(layout->planes()[i].stride, strides[i]);
    EXPECT_EQ(layout->planes()[i].offset, 0u);
  }
}

TEST(VideoFrameLayout, CreateWithPlanes) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 100, 200};
  std::vector<size_t> buffer_sizes = {73728, 18432, 18432};
  auto layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets),
      buffer_sizes);
  ASSERT_TRUE(layout.has_value());

  EXPECT_EQ(layout->format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout->coded_size(), coded_size);
  EXPECT_EQ(layout->num_planes(), 3u);
  EXPECT_EQ(layout->num_buffers(), 3u);
  EXPECT_EQ(layout->GetTotalBufferSize(), 110592u);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(layout->planes()[i].stride, strides[i]);
    EXPECT_EQ(layout->planes()[i].offset, offsets[i]);
    EXPECT_EQ(layout->buffer_sizes()[i], buffer_sizes[i]);
  }
}

TEST(VideoFrameLayout, CreateWithPlanesNoBufferSizes) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 100, 200};
  auto layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets));
  ASSERT_TRUE(layout.has_value());

  EXPECT_EQ(layout->format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout->coded_size(), coded_size);
  EXPECT_EQ(layout->num_planes(), 3u);
  EXPECT_EQ(layout->num_buffers(), 0u);
  EXPECT_EQ(layout->GetTotalBufferSize(), 0u);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(layout->planes()[i].stride, strides[i]);
    EXPECT_EQ(layout->planes()[i].offset, offsets[i]);
  }
}

TEST(VideoFrameLayout, CopyConstructor) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 100, 200};
  std::vector<size_t> buffer_sizes = {73728, 18432, 18432};
  auto layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets),
      buffer_sizes);
  ASSERT_TRUE(layout.has_value());

  VideoFrameLayout layout_clone(*layout);
  EXPECT_EQ(layout_clone.format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout_clone.coded_size(), coded_size);
  EXPECT_EQ(layout_clone.num_planes(), 3u);
  EXPECT_EQ(layout_clone.num_buffers(), 3u);
  EXPECT_EQ(layout_clone.GetTotalBufferSize(), 110592u);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(layout->planes()[i].stride, strides[i]);
    EXPECT_EQ(layout->planes()[i].offset, offsets[i]);
    EXPECT_EQ(layout_clone.buffer_sizes()[i], buffer_sizes[i]);
  }
}

TEST(VideoFrameLayout, CopyAssignmentOperator) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 100, 200};
  std::vector<size_t> buffer_sizes = {73728, 18432, 18432};
  auto layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets),
      buffer_sizes);
  ASSERT_TRUE(layout.has_value());

  VideoFrameLayout layout_clone = *layout;
  EXPECT_EQ(layout_clone.format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout_clone.coded_size(), coded_size);
  EXPECT_EQ(layout_clone.num_planes(), 3u);
  EXPECT_EQ(layout_clone.num_buffers(), 3u);
  EXPECT_EQ(layout_clone.GetTotalBufferSize(), 110592u);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(layout_clone.planes()[i].stride, strides[i]);
    EXPECT_EQ(layout_clone.planes()[i].offset, offsets[i]);
    EXPECT_EQ(layout_clone.buffer_sizes()[i], buffer_sizes[i]);
  }
}

TEST(VideoFrameLayout, MoveConstructor) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 100, 200};
  std::vector<size_t> buffer_sizes = {73728, 18432, 18432};
  auto layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_I420, coded_size, CreatePlanes(strides, offsets),
      buffer_sizes);
  ASSERT_TRUE(layout.has_value());

  VideoFrameLayout layout_move(std::move(*layout));

  EXPECT_EQ(layout_move.format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout_move.coded_size(), coded_size);
  EXPECT_EQ(layout_move.num_planes(), 3u);
  EXPECT_EQ(layout_move.num_buffers(), 3u);
  EXPECT_EQ(layout_move.GetTotalBufferSize(), 110592u);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(layout_move.planes()[i].stride, strides[i]);
    EXPECT_EQ(layout_move.planes()[i].offset, offsets[i]);
    EXPECT_EQ(layout_move.buffer_sizes()[i], buffer_sizes[i]);
  }

  // Members in object being moved are cleared except const members.
  EXPECT_EQ(layout->format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(layout->coded_size(), coded_size);
  EXPECT_EQ(layout->num_planes(), 0u);
  EXPECT_EQ(layout->num_buffers(), 0u);
  EXPECT_EQ(layout->GetTotalBufferSize(), 0u);
}

TEST(VideoFrameLayout, ToString) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> buffer_sizes = {73728, 18432, 18432};
  auto layout = VideoFrameLayout::CreateWithStrides(
      PIXEL_FORMAT_I420, coded_size, strides, buffer_sizes);
  ASSERT_TRUE(layout.has_value());

  EXPECT_EQ(layout->ToString(),
            "VideoFrameLayout format: PIXEL_FORMAT_I420, coded_size: 320x180, "
            "num_buffers: 3, buffer_sizes: [73728, 18432, 18432], "
            "num_planes: 3, "
            "planes (stride, offset): [(384, 0), (192, 0), (192, 0)]");
}

TEST(VideoFrameLayout, ToStringOneBuffer) {
  gfx::Size coded_size = gfx::Size(320, 180);
  std::vector<int32_t> strides = {384};
  std::vector<size_t> offsets = {100};
  std::vector<size_t> buffer_sizes = {122880};
  auto layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_NV12, coded_size, CreatePlanes(strides, offsets),
      buffer_sizes);
  ASSERT_TRUE(layout.has_value());

  EXPECT_EQ(layout->ToString(),
            "VideoFrameLayout format: PIXEL_FORMAT_NV12, coded_size: 320x180, "
            "num_buffers: 1, buffer_sizes: [122880], num_planes: 1, "
            "planes (stride, offset): [(384, 100)]");
}

TEST(VideoFrameLayout, ToStringNoBufferInfo) {
  gfx::Size coded_size = gfx::Size(320, 180);
  auto layout = VideoFrameLayout::Create(PIXEL_FORMAT_NV12, coded_size);
  ASSERT_TRUE(layout.has_value());

  EXPECT_EQ(layout->ToString(),
            "VideoFrameLayout format: PIXEL_FORMAT_NV12, coded_size: 320x180, "
            "num_buffers: 0, buffer_sizes: [], num_planes: 2, "
            "planes (stride, offset): [(0, 0), (0, 0)]");
}

}  // namespace media
