// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/video_frame_resource.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

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

TEST(VideoFrameResourceTest, WrappingLifecycle) {
  const VideoPixelFormat kPixelFormat = PIXEL_FORMAT_NV12;
  constexpr gfx::Size kCodedSize(320, 240);
  scoped_refptr<FrameResource> orig =
      VideoFrameResource::Create(CreateMockDmaBufVideoFrame(
          kPixelFormat, kCodedSize, /*visible_rect=*/gfx::Rect(kCodedSize),
          /*natural_size=*/kCodedSize));
  ASSERT_TRUE(orig);
  ASSERT_TRUE(orig->HasOneRef());

  // Create a wrapping frame. This should increase the reference count to
  // |orig|.
  scoped_refptr<FrameResource> wrapping_frame = orig->CreateWrappingFrame();
  ASSERT_TRUE(wrapping_frame);

  // The following asserts ensure that orig has at least two references. If
  // CreateWrappingFrame() is working correctly, there will be exactly two. This
  // ensures that |orig| will not be destroyed as long as |wrapping_frame|
  // lives.
  ASSERT_TRUE(orig->HasAtLeastOneRef());
  ASSERT_FALSE(orig->HasOneRef());
}
}  // namespace media
