// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_LAYOUT_H_
#define MEDIA_BASE_VIDEO_FRAME_LAYOUT_H_

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "media/base/media_export.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// A class to describes how physical buffer is allocated for video frame.
// In stores format, coded size of the frame and size of physical buffers
// which can be used to allocate buffer(s) hardware expected.
// It also stores stride (bytes per line) and offset per color plane as Plane.
// stride is to calculate each color plane's size (note that a buffer may
// contains multiple color planes.)
// offset is to describe a start point of each plane from buffer's dmabuf fd.
// Note that it is copyable.
class MEDIA_EXPORT VideoFrameLayout {
 public:
  struct Plane {
    Plane() = default;
    Plane(int32_t stride, size_t offset) : stride(stride), offset(offset) {}

    bool operator==(const Plane& rhs) const {
      return stride == rhs.stride && offset == rhs.offset;
    }

    // Strides of a plane, typically greater or equal to the
    // width of the surface divided by the horizontal sampling period. Note that
    // strides can be negative if the image layout is bottom-up.
    int32_t stride = 0;

    // Offset of a plane, which stands for the offset of a start point of a
    // color plane from a buffer fd.
    size_t offset = 0;
  };

  // Factory functions.
  // |format| and |coded_size| must be specified.
  // |strides|, |planes| and |buffer_sizes| are optional, whereas they should
  //  be specified if |buffer_sizes| are given.
  // The size of |buffer_sizes| must be less than or equal to |planes|.
  // Unless they are specified, num_planes() is NumPlanes(|format|) and
  // num_buffers() is 0.
  // The returned base::Optional will be base::nullopt if the configured values
  // are invalid.
  static base::Optional<VideoFrameLayout> Create(VideoPixelFormat format,
                                                 const gfx::Size& coded_size);

  // The size of |strides| must be NumPlanes(|format|). Planes' offset will be
  // 0.
  static base::Optional<VideoFrameLayout> CreateWithStrides(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      std::vector<int32_t> strides,
      std::vector<size_t> buffer_sizes = {});

  // The size of |planes| must be NumPlanes(|format|).
  static base::Optional<VideoFrameLayout> CreateWithPlanes(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      std::vector<Plane> planes,
      std::vector<size_t> buffer_sizes = {});

  VideoFrameLayout() = delete;
  VideoFrameLayout(const VideoFrameLayout&);
  VideoFrameLayout(VideoFrameLayout&&);
  VideoFrameLayout& operator=(const VideoFrameLayout&);
  ~VideoFrameLayout();

  static size_t NumPlanes(VideoPixelFormat format);

  VideoPixelFormat format() const { return format_; }
  const gfx::Size& coded_size() const { return coded_size_; }

  // Return number of buffers. Note that num_planes >= num_buffers.
  size_t num_buffers() const { return buffer_sizes_.size(); }

  // Returns number of planes. Note that num_planes >= num_buffers.
  size_t num_planes() const { return planes_.size(); }

  const std::vector<Plane>& planes() const { return planes_; }
  const std::vector<size_t>& buffer_sizes() const { return buffer_sizes_; }

  // Returns sum of bytes of all buffers.
  size_t GetTotalBufferSize() const;

  // Composes VideoFrameLayout as human readable string.
  std::string ToString() const;

 private:
  VideoFrameLayout(VideoPixelFormat format,
                   const gfx::Size& coded_size,
                   std::vector<Plane> planes,
                   std::vector<size_t> buffer_sizes);

  VideoPixelFormat format_;

  // Width and height of the video frame in pixels. This must include pixel
  // data for the whole image; i.e. for YUV formats with subsampled chroma
  // planes, in the case that the visible portion of the image does not line up
  // on a sample boundary, |coded_size_| must be rounded up appropriately and
  // the pixel data provided for the odd pixels.
  gfx::Size coded_size_;

  // Layout property for each color planes, e.g. stride and buffer offset.
  std::vector<Plane> planes_;

  // Vector of sizes for each buffer, typically greater or equal to the area of
  // |coded_size_|.
  std::vector<size_t> buffer_sizes_;
};

// Outputs VideoFrameLayout::Plane to stream.
std::ostream& operator<<(std::ostream& ostream,
                         const VideoFrameLayout::Plane& plane);

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_LAYOUT_H_
