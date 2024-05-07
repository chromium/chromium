// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_LAYOUT_H_
#define MEDIA_BASE_VIDEO_FRAME_LAYOUT_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <ostream>
#include <utility>
#include <vector>

#include "media/base/color_plane_layout.h"
#include "media/base/media_export.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"

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
  // Default alignment for buffers.
  // Note: This value is dependent on what's used by ffmpeg, do not change
  // without inspecting av_frame_get_buffer() first.
  static constexpr size_t kBufferAddressAlignment = 32;

  // Factory functions.
  // |format| and |coded_size| must always be specified.
  // |planes| info is also optional but useful to represent the layout of a
  // video frame buffer correctly. When omitted, its information is all set
  // to zero, so clients should be wary not to use this information.
  // |buffer_addr_align| can be specified to request a specific buffer memory
  // alignment.
  // |modifier| is the additional information of |format|. It will become some
  // value else than gfx::NativePixmapHandle::kNoModifier when the underlying
  // buffer format is different from a standard |format| due to tiling.
  // The returned std::optional will be std::nullopt if the configured values
  // are invalid.

  // Create a layout suitable for |format| at |coded_size|. The stride, offsets
  // and size of all planes are set to 0, since that information cannot reliably
  // be inferred from the arguments.
  static std::optional<VideoFrameLayout> Create(VideoPixelFormat format,
                                                const gfx::Size& coded_size);

  // Create a layout suitable for |format| at |coded_size|, with the |strides|
  // for each plane specified. The offsets and size of all planes are set to 0.
  // The size of |strides| must be equal to NumPlanes(|format|).
  static std::optional<VideoFrameLayout> CreateWithStrides(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      std::vector<int32_t> strides,
      size_t buffer_addr_align = kBufferAddressAlignment,
      uint64_t modifier = gfx::NativePixmapHandle::kNoModifier);

  // Create a layout suitable for |format| at |coded_size|, with the |planes|
  // fully provided.
  // The size of |planes| must be equal to NumPlanes(|format|).
  static std::optional<VideoFrameLayout> CreateWithPlanes(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      std::vector<ColorPlaneLayout> planes,
      size_t buffer_addr_align = kBufferAddressAlignment,
      uint64_t modifier = gfx::NativePixmapHandle::kNoModifier);

  // This constructor should be called for situations where the frames using
  // this format are backed by multiple physical buffers, instead of having each
  // plane at different offsets of the same buffer. Currently only used by V4L2.
  static std::optional<VideoFrameLayout> CreateMultiPlanar(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      std::vector<ColorPlaneLayout> planes,
      size_t buffer_addr_align = kBufferAddressAlignment,
      uint64_t modifier = gfx::NativePixmapHandle::kNoModifier);

  VideoFrameLayout() = delete;
  VideoFrameLayout(const VideoFrameLayout&);
  VideoFrameLayout(VideoFrameLayout&&);
  VideoFrameLayout& operator=(const VideoFrameLayout&);
  ~VideoFrameLayout();

  static size_t NumPlanes(VideoPixelFormat format);

  VideoPixelFormat format() const { return format_; }
  const gfx::Size& coded_size() const { return coded_size_; }

  // Returns number of planes. Note that num_planes >= num_buffers.
  size_t num_planes() const { return planes_.size(); }

  const std::vector<ColorPlaneLayout>& planes() const { return planes_; }

  bool operator==(const VideoFrameLayout& rhs) const;
  bool operator!=(const VideoFrameLayout& rhs) const;

  // Return true when a format uses multiple backing buffers to store its
  // planes.
  bool is_multi_planar() const { return is_multi_planar_; }
  // Returns the required memory alignment for buffers.
  size_t buffer_addr_align() const { return buffer_addr_align_; }
  // Return the modifier of buffers.
  uint64_t modifier() const { return modifier_; }

  // Any constructible layout is valid in and of itself, it can only be invalid
  // if the backing memory is too small to contain it.
  //
  // Returns true if this VideoFrameLayout can fit in a contiguous buffer of
  // size `data_size` -- always false for multi-planar layouts.
  bool FitsInContiguousBufferOfSize(size_t data_size) const;

 private:
  VideoFrameLayout(VideoPixelFormat format,
                   const gfx::Size& coded_size,
                   std::vector<ColorPlaneLayout> planes,
                   bool is_multi_planar,
                   size_t buffer_addr_align,
                   uint64_t modifier);

  VideoPixelFormat format_;

  // Width and height of the video frame in pixels. This must include pixel
  // data for the whole image; i.e. for YUV formats with subsampled chroma
  // planes, in the case that the visible portion of the image does not line up
  // on a sample boundary, |coded_size_| must be rounded up appropriately and
  // the pixel data provided for the odd pixels.
  gfx::Size coded_size_;

  // Layout property for each color planes, e.g. stride and buffer offset.
  std::vector<ColorPlaneLayout> planes_;

  // Set to true when a format uses multiple backing buffers to store its
  // planes. Used by code for V4L2 API at the moment.
  bool is_multi_planar_;

  // Memory address alignment of the buffers. This is only relevant when
  // allocating physical memory for the buffer, so it doesn't need to be
  // serialized when frames are passed through Mojo.
  size_t buffer_addr_align_;

  // Modifier of buffers. The modifier is retrieved from GBM library. This
  // can be a different value from kNoModifier only if the VideoFrame is created
  // by using NativePixmap.
  uint64_t modifier_;
};

// Outputs VideoFrameLayout to stream.
MEDIA_EXPORT std::ostream& operator<<(std::ostream& ostream,
                                      const VideoFrameLayout& layout);

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_LAYOUT_H_
