// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/mock_native_pixmap_dmabuf.h"

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

// Creates a NativePixmapDmaBuf with valid strides and other metadata, but with
// FD's that reference a /dev/null instead of referencing a DMABuf.
scoped_refptr<const gfx::NativePixmapDmaBuf> CreateMockNativePixmapDmaBuf(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    uint64_t modifier) {
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
  handle.modifier = modifier;

  return base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
      coded_size, *buffer_format, std::move(handle));
}

}  // namespace media
