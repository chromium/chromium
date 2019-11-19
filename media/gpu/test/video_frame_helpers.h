// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_FRAME_HELPERS_H_
#define MEDIA_GPU_TEST_VIDEO_FRAME_HELPERS_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class GpuMemoryBufferFactory;
}

namespace media {
namespace test {

class Image;

// The video frame processor defines an abstract interface for classes that are
// interested in processing video frames (e.g. FrameValidator,...).
class VideoFrameProcessor {
 public:
  virtual ~VideoFrameProcessor() = default;

  // Process the specified |video_frame|. This can e.g. validate the frame,
  // calculate the frame's checksum, write the frame to file,... The
  // |frame_index| is the index of the video frame in display order. The caller
  // should not modify the video frame while a reference is being held by the
  // VideoFrameProcessor. This function should always be called on the same
  // thread.
  virtual void ProcessVideoFrame(scoped_refptr<const VideoFrame> video_frame,
                                 size_t frame_index) = 0;

  // Wait until all currently scheduled frames have been processed. Returns
  // whether processing was successful.
  virtual bool WaitUntilDone() = 0;
};

// Convert and copy the |src_frame| to the specified |dst_frame|. Supported
// input formats are I420, NV12 and YV12. Supported output formats are I420 and
// ARGB. All mappable output storages types are supported, but writing into
// non-owned memory might produce unexpected side effects.
bool ConvertVideoFrame(const VideoFrame* src_frame, VideoFrame* dst_frame);

// Convert and copy the |src_frame| to a new video frame with specified format.
// Supported input formats are I420, NV12 and YV12. Supported output formats are
// I420 and ARGB.
scoped_refptr<VideoFrame> ConvertVideoFrame(const VideoFrame* src_frame,
                                            VideoPixelFormat dst_pixel_format);

// Copy |src_frame| into a new VideoFrame.
// If |dst_storage_type| is STORAGE_DMABUFS, this function creates DMABUF-backed
// VideoFrame with |dst_layout|. If |dst_storage_type| is STORAGE_OWNED_MEMORY,
// this function creates memory-backed VideoFrame with |dst_layout|.
// |dst_buffer_usage| and |gpu_memory_buffer_factory| must be specified if
// |dst_storage_type| is STORAGE_DMABUFS or STORAGE_GPU_MEMORY_BUFFER, and in
// that case the |gpu_memory_buffer_factory| will be used for the allocation to
// create a graphic buffer with the requested usage.
// The created VideoFrame's content is the same as |src_frame|. The created
// VideoFrame owns the buffer. Returns nullptr on failure.
scoped_refptr<VideoFrame> CloneVideoFrame(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    const VideoFrame* const src_frame,
    const VideoFrameLayout& dst_layout,
    VideoFrame::StorageType dst_storage_type = VideoFrame::STORAGE_OWNED_MEMORY,
    base::Optional<gfx::BufferUsage> dst_buffer_usage = base::nullopt);

// Create GpuMemoryBuffer-based VideoFrame from |frame|. The created VideoFrame
// doesn't depend on |frame|'s lifetime.
// |frame| should be a DMABUF-backed VideoFrame. |buffer_usage| is a
// GpuMemoryBuffer's buffer usage. |frame| must be created following the
// |buffer_usage|.
// This function works on ChromeOS only.
scoped_refptr<VideoFrame> CreateGpuMemoryBufferVideoFrame(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    const VideoFrame* const frame,
    gfx::BufferUsage buffer_usage);

// Get VideoFrame that contains Load()ed data. The returned VideoFrame doesn't
// own the data and thus must not be changed.
scoped_refptr<const VideoFrame> CreateVideoFrameFromImage(const Image& image);

// Create a video frame layout for the specified |pixel_format| and
// |coded_size|. The created VideoFrameLayout represents all the planes are
//  stored in a single physical buffer.
base::Optional<VideoFrameLayout> CreateVideoFrameLayout(
    VideoPixelFormat pixel_format,
    const gfx::Size& size);

// Compare each byte of two VideoFrames, |frame1| and |frame2|, allowing the
// error up to |tolerance|. Return number of bytes a difference of which is more
// than |tolerance|.
size_t CompareFramesWithErrorDiff(const VideoFrame& frame1,
                                  const VideoFrame& frame2,
                                  uint8_t tolerance);

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_FRAME_HELPERS_H_
