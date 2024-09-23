// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/platform_video_frame_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {

namespace {

// Creates mock FDs and wrap them into a VideoFrame.
scoped_refptr<VideoFrame> CreateMockDmaBufVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size) {
  const std::optional<VideoFrameLayout> layout =
      VideoFrameLayout::Create(pixel_format, coded_size);
  if (!layout) {
    LOG(ERROR) << "Failed to create video frame layout";
    return nullptr;
  }
  std::vector<base::ScopedFD> dmabuf_fds;
  for (size_t i = 0; i < layout->num_planes(); i++) {
    base::File file(base::FilePath("/dev/null"),
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file.IsValid()) {
      LOG(ERROR) << "Failed to open a file";
      return nullptr;
    }
    dmabuf_fds.emplace_back(file.TakePlatformFile());
    if (!dmabuf_fds.back().is_valid()) {
      LOG(ERROR) << "The FD taken from file is not valid";
      return nullptr;
    }
  }
  return VideoFrame::WrapExternalDmabufs(*layout, visible_rect, natural_size,
                                         std::move(dmabuf_fds),
                                         base::TimeDelta());
}
}  // namespace

TEST(PlatformVideoFrameUtilsTest, CreateNativePixmapDmaBuf) {
  constexpr VideoPixelFormat kPixelFormat = PIXEL_FORMAT_NV12;
  constexpr gfx::Size kCodedSize(320, 240);

  const std::optional<gfx::BufferFormat> gfx_format =
      VideoPixelFormatToGfxBufferFormat(kPixelFormat);
  ASSERT_TRUE(gfx_format) << "Invalid pixel format: " << kPixelFormat;

  scoped_refptr<VideoFrame> video_frame = CreateMockDmaBufVideoFrame(
      kPixelFormat, kCodedSize, gfx::Rect(kCodedSize), kCodedSize);
  ASSERT_TRUE(video_frame);

  // Create a native pixmap and verify its metadata.
  scoped_refptr<gfx::NativePixmapDmaBuf> native_pixmap =
      CreateNativePixmapDmaBuf(video_frame.get());
  ASSERT_TRUE(native_pixmap);
  EXPECT_EQ(native_pixmap->GetBufferFormat(), *gfx_format);
  EXPECT_EQ(native_pixmap->GetBufferFormatModifier(),
            video_frame->layout().modifier());

  // Verify the DMA Buf layouts are the same.
  const size_t num_planes = video_frame->layout().num_planes();
  ASSERT_EQ(native_pixmap->ExportHandle().planes.size(), num_planes);
  for (size_t i = 0; i < num_planes; i++) {
    const ColorPlaneLayout& plane = video_frame->layout().planes()[i];
    // The original and duplicated FDs should be different.
    EXPECT_NE(native_pixmap->GetDmaBufFd(i), video_frame->GetDmabufFd(i));
    EXPECT_EQ(native_pixmap->GetDmaBufPitch(i),
              base::checked_cast<uint32_t>(plane.stride));
    EXPECT_EQ(native_pixmap->GetDmaBufOffset(i), plane.offset);
    EXPECT_EQ(native_pixmap->GetDmaBufPlaneSize(i), plane.size);
  }
}

// TODO(b/230370976): remove this #if/#endif guard. To do so, we need to be able
// to mock/fake the allocator used by CreatePlatformVideoFrame() and
// CreateGpuMemoryBufferVideoFrame() so that those functions return a
// non-nullptr frame on platforms where allocating NV12 buffers is not
// supported.
#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST(PlatformVideoFrameUtilsTest, CreateVideoFrame) {
  constexpr VideoPixelFormat kPixelFormat = PIXEL_FORMAT_NV12;
  constexpr gfx::Size kCodedSize(320, 240);
  constexpr gfx::Rect kVisibleRect(kCodedSize);
  constexpr gfx::Size kNaturalSize(kCodedSize);
  constexpr auto kTimeStamp = base::Milliseconds(1234);
  constexpr gfx::BufferUsage kBufferUsage =
      gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE;

  const VideoFrame::StorageType storage_types[] = {
      VideoFrame::STORAGE_DMABUFS,
      VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
  };
  for (const auto& storage_type : storage_types) {
    scoped_refptr<VideoFrame> frame;
    switch (storage_type) {
      case VideoFrame::STORAGE_DMABUFS:
        frame =
            CreatePlatformVideoFrame(kPixelFormat, kCodedSize, kVisibleRect,
                                     kNaturalSize, kTimeStamp, kBufferUsage);
        break;
      case VideoFrame::STORAGE_GPU_MEMORY_BUFFER:
        frame = CreateGpuMemoryBufferVideoFrame(kPixelFormat, kCodedSize,
                                                kVisibleRect, kNaturalSize,
                                                kTimeStamp, kBufferUsage);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    };

    ASSERT_TRUE(frame);
    EXPECT_EQ(frame->format(), kPixelFormat);
    EXPECT_EQ(frame->coded_size(), kCodedSize);
    EXPECT_EQ(frame->visible_rect(), kVisibleRect);
    EXPECT_EQ(frame->natural_size(), kNaturalSize);
    EXPECT_EQ(frame->timestamp(), kTimeStamp);
    EXPECT_EQ(frame->storage_type(), storage_type);

    switch (storage_type) {
      case VideoFrame::STORAGE_DMABUFS:
        EXPECT_FALSE(frame->NumDmabufFds() == 0);
        break;
      case VideoFrame::STORAGE_GPU_MEMORY_BUFFER:
        EXPECT_TRUE(frame->GetGpuMemoryBufferForTesting());
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    };
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace media
