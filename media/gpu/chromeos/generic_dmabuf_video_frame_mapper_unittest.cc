// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/generic_dmabuf_video_frame_mapper.h"

#include <sys/mman.h>

#include <algorithm>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "media/base/color_plane_layout.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/video_frame_resource.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

scoped_refptr<VideoFrame> CreateDmaBufVideoFrame(VideoPixelFormat format,
                                                 const gfx::Size& coded_size) {
  std::vector<size_t> strides = VideoFrame::ComputeStrides(format, coded_size);
  std::vector<ColorPlaneLayout> planes;
  for (size_t i = 0; i < strides.size(); ++i) {
    planes.emplace_back(
        strides[i], 0u,
        VideoFrame::Rows(i, format, coded_size.height()) * strides[i]);
  }
  const std::optional<VideoFrameLayout> layout =
      VideoFrameLayout::CreateWithPlanes(format, coded_size, std::move(planes));
  if (!layout) {
    return nullptr;
  }

  const size_t num_planes = layout->num_planes();
  std::vector<base::ScopedFD> dmabuf_fds;

  for (size_t i = 0; i < num_planes; ++i) {
    base::UnsafeSharedMemoryRegion region =
        base::UnsafeSharedMemoryRegion::Create(layout->planes()[i].size +
                                               layout->planes()[i].offset);
    if (!region.IsValid()) {
      return nullptr;
    }
    base::subtle::PlatformSharedMemoryRegion platform_region =
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(region));
    auto handle = platform_region.PassPlatformHandle();
    dmabuf_fds.emplace_back(std::move(handle.fd));
  }

  return VideoFrame::WrapExternalDmabufs(*layout, gfx::Rect(coded_size),
                                         coded_size, std::move(dmabuf_fds),
                                         base::TimeDelta());
}

scoped_refptr<VideoFrame> CreateMultiPlaneSingleFdDmaBufVideoFrame(
    VideoPixelFormat format,
    const gfx::Size& coded_size) {
  std::vector<size_t> strides = VideoFrame::ComputeStrides(format, coded_size);
  std::vector<ColorPlaneLayout> planes;
  size_t offset = 0;
  for (size_t i = 0; i < strides.size(); ++i) {
    size_t plane_size =
        VideoFrame::Rows(i, format, coded_size.height()) * strides[i];
    planes.emplace_back(strides[i], offset, plane_size);
    offset += plane_size;
  }
  const std::optional<VideoFrameLayout> layout =
      VideoFrameLayout::CreateWithPlanes(format, coded_size, std::move(planes));
  if (!layout) {
    return nullptr;
  }

  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(offset);
  if (!region.IsValid()) {
    return nullptr;
  }

  std::vector<base::ScopedFD> dmabuf_fds;
  for (size_t i = 0; i < layout->num_planes(); ++i) {
    auto dup_region = region.Duplicate();
    if (!dup_region.IsValid()) {
      return nullptr;
    }
    base::subtle::PlatformSharedMemoryRegion platform_region =
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(dup_region));
    auto handle = platform_region.PassPlatformHandle();
    dmabuf_fds.emplace_back(std::move(handle.fd));
  }

  return VideoFrame::WrapExternalDmabufs(*layout, gfx::Rect(coded_size),
                                         coded_size, std::move(dmabuf_fds),
                                         base::TimeDelta());
}

}  // namespace

TEST(GenericDmaBufVideoFrameMapperTest, CreateAndMap) {
  const VideoPixelFormat format = PIXEL_FORMAT_NV12;
  auto mapper = GenericDmaBufVideoFrameMapper::Create(format);
  ASSERT_TRUE(mapper);

  gfx::Size coded_size(320, 240);
  auto video_frame = CreateDmaBufVideoFrame(format, coded_size);
  ASSERT_TRUE(video_frame);

  auto mapped_frame = mapper->MapFrame(VideoFrameResource::Create(video_frame),
                                       PROT_READ | PROT_WRITE);
  ASSERT_TRUE(mapped_frame);
  EXPECT_EQ(mapped_frame->format(), format);
  EXPECT_EQ(mapped_frame->coded_size(), coded_size);
  EXPECT_TRUE(mapped_frame->HasDirectCpuAccess());
  EXPECT_TRUE(mapped_frame->data(0));
  EXPECT_TRUE(mapped_frame->data(1));
}

TEST(GenericDmaBufVideoFrameMapperTest, CreateAndMapSingleFd) {
  const VideoPixelFormat format = PIXEL_FORMAT_NV12;
  auto mapper = GenericDmaBufVideoFrameMapper::Create(format);
  ASSERT_TRUE(mapper);

  gfx::Size coded_size(320, 240);
  auto video_frame =
      CreateMultiPlaneSingleFdDmaBufVideoFrame(format, coded_size);
  ASSERT_TRUE(video_frame);

  auto mapped_frame = mapper->MapFrame(VideoFrameResource::Create(video_frame),
                                       PROT_READ | PROT_WRITE);
  ASSERT_TRUE(mapped_frame);
  EXPECT_EQ(mapped_frame->format(), format);
  EXPECT_EQ(mapped_frame->coded_size(), coded_size);
  EXPECT_TRUE(mapped_frame->HasDirectCpuAccess());
  EXPECT_TRUE(mapped_frame->data(0));
  EXPECT_TRUE(mapped_frame->data(1));
}

TEST(GenericDmaBufVideoFrameMapperTest, UnsupportedFormat) {
  const VideoPixelFormat format = PIXEL_FORMAT_I422;
  auto mapper = GenericDmaBufVideoFrameMapper::Create(format);
  EXPECT_FALSE(mapper);
}

TEST(GenericDmaBufVideoFrameMapperTest, MapNullptr) {
  auto mapper = GenericDmaBufVideoFrameMapper::Create(PIXEL_FORMAT_NV12);
  ASSERT_TRUE(mapper);
  EXPECT_FALSE(mapper->MapFrame(nullptr, PROT_READ));
}

TEST(GenericDmaBufVideoFrameMapperTest, MapNonDmaBufFrame) {
  auto mapper = GenericDmaBufVideoFrameMapper::Create(PIXEL_FORMAT_NV12);
  ASSERT_TRUE(mapper);

  gfx::Size coded_size(320, 240);
  auto video_frame = VideoFrame::CreateFrame(PIXEL_FORMAT_NV12, coded_size,
                                             gfx::Rect(coded_size), coded_size,
                                             base::TimeDelta());
  ASSERT_TRUE(video_frame);
  EXPECT_EQ(video_frame->storage_type(), VideoFrame::STORAGE_OWNED_MEMORY);

  EXPECT_FALSE(
      mapper->MapFrame(VideoFrameResource::Create(video_frame), PROT_READ));
}

TEST(GenericDmaBufVideoFrameMapperTest, MapWrongFormat) {
  auto mapper = GenericDmaBufVideoFrameMapper::Create(PIXEL_FORMAT_NV12);
  ASSERT_TRUE(mapper);

  gfx::Size coded_size(320, 240);
  auto video_frame = CreateDmaBufVideoFrame(PIXEL_FORMAT_I420, coded_size);
  ASSERT_TRUE(video_frame);

  EXPECT_FALSE(
      mapper->MapFrame(VideoFrameResource::Create(video_frame), PROT_READ));
}

TEST(GenericDmaBufVideoFrameMapperTest, MapInvalidOffset) {
  auto mapper = GenericDmaBufVideoFrameMapper::Create(PIXEL_FORMAT_NV12);
  ASSERT_TRUE(mapper);

  gfx::Size coded_size(320, 240);
  std::vector<size_t> strides =
      VideoFrame::ComputeStrides(PIXEL_FORMAT_NV12, coded_size);
  std::vector<ColorPlaneLayout> planes;
  for (size_t i = 0; i < strides.size(); ++i) {
    // This is considered by GenericDmaBufVideoFrameMapper because it expects
    // the first plane of each dmabuf buffer to have an offset of 0.
    planes.emplace_back(
        strides[i], (i == 0 ? 1024u : 0u),
        VideoFrame::Rows(i, PIXEL_FORMAT_NV12, coded_size.height()) *
            strides[i]);
  }

  auto invalid_layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_NV12, coded_size, std::move(planes));
  ASSERT_TRUE(invalid_layout);

  std::vector<base::ScopedFD> dmabuf_fds;
  for (size_t i = 0; i < invalid_layout->num_planes(); ++i) {
    base::UnsafeSharedMemoryRegion region =
        base::UnsafeSharedMemoryRegion::Create(
            invalid_layout->planes()[i].size +
            invalid_layout->planes()[i].offset);
    ASSERT_TRUE(region.IsValid());

    base::subtle::PlatformSharedMemoryRegion platform_region =
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(region));
    auto handle = platform_region.PassPlatformHandle();
    dmabuf_fds.emplace_back(std::move(handle.fd));
  }

  auto video_frame = VideoFrame::WrapExternalDmabufs(
      *invalid_layout, gfx::Rect(coded_size), coded_size, std::move(dmabuf_fds),
      base::TimeDelta());
  ASSERT_TRUE(video_frame);

  EXPECT_FALSE(
      mapper->MapFrame(VideoFrameResource::Create(video_frame), PROT_READ));
}

TEST(GenericDmaBufVideoFrameMapperTest, MapPlaneExtendsPastBoundary) {
  auto mapper = GenericDmaBufVideoFrameMapper::Create(PIXEL_FORMAT_NV12);
  ASSERT_TRUE(mapper);

  gfx::Size coded_size(320, 240);
  std::vector<size_t> strides =
      VideoFrame::ComputeStrides(PIXEL_FORMAT_NV12, coded_size);

  // We want to trigger "plane_end > mapped_size".
  // mapped_size is calculated from the last plane of the buffer.
  // We'll create a single FD with two planes.
  // Plane 0: offset 0, size 2000
  // Plane 1: offset 100, size 100
  // mapped_size = 100 + 100 = 200.
  // plane_end[0] = 0 + 2000 = 2000.
  // 2000 > 200, so it should fail.

  std::vector<ColorPlaneLayout> planes;
  planes.emplace_back(strides[0], 0, 2000);
  planes.emplace_back(strides[1], 100, 100);

  auto invalid_layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_NV12, coded_size, std::move(planes));
  ASSERT_TRUE(invalid_layout);

  // Single FD for all planes.
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(2000);
  ASSERT_TRUE(region.IsValid());

  std::vector<base::ScopedFD> dmabuf_fds;
  for (size_t i = 0; i < invalid_layout->num_planes(); ++i) {
    auto dup_region = region.Duplicate();
    ASSERT_TRUE(dup_region.IsValid());
    base::subtle::PlatformSharedMemoryRegion platform_region =
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(dup_region));
    auto handle = platform_region.PassPlatformHandle();
    dmabuf_fds.emplace_back(std::move(handle.fd));
  }

  auto video_frame = VideoFrame::WrapExternalDmabufs(
      *invalid_layout, gfx::Rect(coded_size), coded_size, std::move(dmabuf_fds),
      base::TimeDelta());
  ASSERT_TRUE(video_frame);

  EXPECT_FALSE(
      mapper->MapFrame(VideoFrameResource::Create(video_frame), PROT_READ));
}

}  // namespace media
