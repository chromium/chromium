// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "media/gpu/linux/platform_video_frame_utils.h"
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
  const base::Optional<VideoFrameLayout> layout =
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

  const base::Optional<gfx::BufferFormat> gfx_format =
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
    EXPECT_NE(native_pixmap->GetDmaBufFd(i), video_frame->DmabufFds()[i].get());
    EXPECT_EQ(native_pixmap->GetDmaBufPitch(i),
              base::checked_cast<uint32_t>(plane.stride));
    EXPECT_EQ(native_pixmap->GetDmaBufOffset(i), plane.offset);
    EXPECT_EQ(native_pixmap->GetDmaBufPlaneSize(i), plane.size);
  }
}

}  // namespace media
