// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/native_pixmap_frame_resource.h"

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

// Creates a vector of file descriptors. Used to simulate a vector of DMABuf
// FDs.
std::vector<base::ScopedFD> CreateMockDMABufs(size_t num_planes) {
  std::vector<base::ScopedFD> dmabuf_fds;
  for (size_t i = 0; i < num_planes; i++) {
    // Instead of creating an FD of a DMABuf, this makes an FD for /dev/null.
    base::File file(base::FilePath("/dev/null"),
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file.IsValid()) {
      LOG(ERROR) << "Failed to open a file";
      return std::vector<base::ScopedFD>();
    }
    dmabuf_fds.emplace_back(file.TakePlatformFile());
    if (!dmabuf_fds.back().is_valid()) {
      LOG(ERROR) << "The FD taken from file is not valid";
      return std::vector<base::ScopedFD>();
    }
  }
  return dmabuf_fds;
}

// Creates a NativePixmapDmaBuf with valid strides and other metadata, but with
// FD's that reference a /dev/null instead of referencing a DMABuf.
scoped_refptr<const gfx::NativePixmapDmaBuf> CreateMockNativePixmapDmaBuf(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size) {
  // Uses VideoFrameLayout::Create() to create a VideoFrameLayout with correct
  // planes and strides. The values from |layout| will be used to construct a
  // gfx::NativePixmapHandle.
  const std::optional<VideoFrameLayout> layout =
      VideoFrameLayout::Create(pixel_format, coded_size);
  if (!layout.has_value()) {
    LOG(ERROR) << "Failed to create video frame layout";
    return nullptr;
  }

  // This converts |layout|'s VideoPixelFormat to a gfx::BufferFormat, which is
  // needed by the NativePixmapDmaBuf constructor.
  auto buffer_format = VideoPixelFormatToGfxBufferFormat(pixel_format);
  if (!buffer_format) {
    LOG(ERROR) << "Unable to convert pixel format " << pixel_format
               << " to BufferFormat";
    return nullptr;
  }

  gfx::NativePixmapHandle handle;
  const size_t num_planes = layout->num_planes();
  handle.planes.reserve(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    const auto& plane = layout->planes()[i];
    // For the NativePixmapHandle FD's this creates an FD to "/dev/null".
    base::File file(base::FilePath("/dev/null"),
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file.IsValid()) {
      LOG(ERROR) << "Failed to open a file";
      return nullptr;
    }
    handle.planes.emplace_back(plane.stride, plane.offset, plane.size,
                               base::ScopedFD(file.TakePlatformFile()));
  }
  handle.modifier = layout->modifier();

  return base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
      coded_size, *buffer_format, std::move(handle));
}

}  // namespace

class CreateTest : public ::testing::Test,
                   public ::testing::WithParamInterface<
                       std::tuple<gfx::Size, gfx::Rect, gfx::Size, bool>> {
 public:
  void SetUp() override {}
  void TearDown() override {}

  // The parameters.
  static constexpr size_t kCodedSizeIndex = 0;
  static constexpr size_t kVisibleRectIndex = 1;
  static constexpr size_t kNaturalSizeIndex = 2;
  static constexpr size_t kShouldSucceedIndex = 3;
};

// Tests the size checks of NativePixmapFrameResource's DMABuf FD-based factory
// function.
TEST_P(CreateTest, CreateFromDMABufs) {
  constexpr VideoPixelFormat kPixelFormat = PIXEL_FORMAT_NV12;
  constexpr base::TimeDelta kTimestamp;

  const gfx::Size coded_size = std::get<kCodedSizeIndex>(GetParam());
  const gfx::Rect visible_rect = std::get<kVisibleRectIndex>(GetParam());
  const gfx::Size natural_size = std::get<kNaturalSizeIndex>(GetParam());
  const bool should_succeed = std::get<kShouldSucceedIndex>(GetParam());

  const std::optional<VideoFrameLayout> layout =
      VideoFrameLayout::Create(kPixelFormat, coded_size);
  ASSERT_TRUE(layout.has_value()) << "Failed to create video frame layout";

  auto dmabuf_fds = CreateMockDMABufs(layout->num_planes());
  ASSERT_FALSE(dmabuf_fds.empty());

  auto frame = NativePixmapFrameResource::Create(
      *layout, visible_rect, natural_size, std::move(dmabuf_fds), kTimestamp);
  if (should_succeed) {
    EXPECT_TRUE(!!frame);
  } else {
    EXPECT_EQ(frame, nullptr);
  }
}

// Tests the size checks of NativePixmapFrameResource's NativePixmapDmaBuf-based
// factory function.
TEST_P(CreateTest, CreateFromNativePixmapDmabuf) {
  constexpr VideoPixelFormat kPixelFormat = PIXEL_FORMAT_NV12;
  constexpr gfx::BufferUsage kBufferUsage = gfx::BufferUsage::GPU_READ;
  constexpr base::TimeDelta kTimestamp;

  const gfx::Size coded_size = std::get<kCodedSizeIndex>(GetParam());
  const gfx::Rect visible_rect = std::get<kVisibleRectIndex>(GetParam());
  const gfx::Size natural_size = std::get<kNaturalSizeIndex>(GetParam());
  const bool should_succeed = std::get<kShouldSucceedIndex>(GetParam());

  scoped_refptr<const gfx::NativePixmapDmaBuf> pixmap =
      CreateMockNativePixmapDmaBuf(kPixelFormat, coded_size);
  ASSERT_TRUE(!!pixmap);
  auto frame = NativePixmapFrameResource::Create(
      visible_rect, natural_size, kTimestamp, kBufferUsage, std::move(pixmap));
  if (should_succeed) {
    EXPECT_TRUE(!!frame);
  } else {
    EXPECT_EQ(frame, nullptr);
  }
}

INSTANTIATE_TEST_SUITE_P(NativePixmapFrameResourceValidSize,
                         CreateTest,
                         ::testing::Values(std::make_tuple(gfx::Size(320, 240),
                                                           gfx::Rect(320, 240),
                                                           gfx::Size(320, 240),
                                                           true)));

INSTANTIATE_TEST_SUITE_P(NativePixmapFrameResourceInvalidSize,
                         CreateTest,
                         ::testing::Values(std::make_tuple(gfx::Size(),
                                                           gfx::Rect(),
                                                           gfx::Size(),
                                                           false),
                                           std::make_tuple(gfx::Size(320, 240),
                                                           gfx::Rect(),
                                                           gfx::Size(320, 240),
                                                           false),
                                           std::make_tuple(gfx::Size(320, 240),
                                                           gfx::Rect(320, 240),
                                                           gfx::Size(),
                                                           false)));

}  // namespace media
