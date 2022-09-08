// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_COMMON_MOJO_SHARED_BUFFER_VIDEO_FRAME_H_
#define MEDIA_MOJO_COMMON_MOJO_SHARED_BUFFER_VIDEO_FRAME_H_

#include <stddef.h>
#include <stdint.h>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// A derived class of media::VideoFrame holding a mojo::SharedBufferHandle
// which is mapped on constructor and remains so for the lifetime of the
// object. These frames are ref-counted.
class MojoSharedBufferVideoFrame : public VideoFrame {
 public:
  // Callback called when this object is destructed. Ownership of the shared
  // memory is transferred to the callee.
  using MojoSharedBufferDoneCB =
      base::OnceCallback<void(base::UnsafeSharedMemoryRegion region)>;

  // Creates a new I420 or NV12 frame in shared memory with provided parameters
  // (coded_size() == natural_size() == visible_rect()), or returns nullptr.
  // Buffers for the frame are allocated but not initialized. The caller must
  // not make assumptions about the actual underlying sizes, but check the
  // returned VideoFrame instead. |format| must be either PIXEL_FORMAT_I420 or
  // PIXEL_FORMAT_NV12.
  static scoped_refptr<MojoSharedBufferVideoFrame> CreateDefaultForTesting(
      const VideoPixelFormat format,
      const gfx::Size& dimensions,
      base::TimeDelta timestamp);

  // Creates a YUV frame backed by shared memory from in-memory YUV frame.
  // Internally the data from in-memory YUV frame will be copied to a
  // consecutive block in shared memory. Will return null on failure.
  static scoped_refptr<MojoSharedBufferVideoFrame> CreateFromYUVFrame(
      VideoFrame& frame);

  // Creates a MojoSharedBufferVideoFrame that uses the memory in |handle|.
  // This will take ownership of |handle|, so the caller can no longer use it.
  // |mojo_shared_buffer_done_cb|, if not null, is called on destruction,
  // and is passed ownership of |handle|. |handle| must be writable. |offsets|
  // and |strides| should be in plane order.
  static scoped_refptr<MojoSharedBufferVideoFrame> Create(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::UnsafeSharedMemoryRegion region,
      base::span<const uint32_t> offsets,
      base::span<const int32_t> strides,
      base::TimeDelta timestamp);

  MojoSharedBufferVideoFrame(const MojoSharedBufferVideoFrame&) = delete;
  MojoSharedBufferVideoFrame& operator=(const MojoSharedBufferVideoFrame&) =
      delete;

  // Returns the offsets relative to the start of the shmem mapping for the
  // |plane| specified.
  size_t PlaneOffset(size_t plane) const;

  // Callers can `Duplicate()` the mapping to extend the lifetime of the region.
  const base::UnsafeSharedMemoryRegion& shmem_region() const { return region_; }

  // Sets the callback to be called to free the shmem region. If not null,
  // the callback is called when `this` is destroyed, and ownership of
  // `region_` is transferred to it.
  void SetMojoSharedBufferDoneCB(
      MojoSharedBufferDoneCB mojo_shared_buffer_done_cb);

 private:
  friend class MojoDecryptorService;

  MojoSharedBufferVideoFrame(const VideoFrameLayout& layout,
                             const gfx::Rect& visible_rect,
                             const gfx::Size& natural_size,
                             base::UnsafeSharedMemoryRegion region,
                             base::TimeDelta timestamp);
  ~MojoSharedBufferVideoFrame() override;

  // Initializes the MojoSharedBufferVideoFrame by creating a mapping onto
  // the shared memory, and then setting offsets as specified.
  bool Init(base::span<const uint32_t> offsets);

  uint8_t* shared_buffer_data() { return mapping_.GetMemoryAs<uint8_t>(); }

  // WritableSharedMemoryRegion has strict ownership and cannot be cloned. Since
  // the shared memory region may be reused and handed out to a producer
  // multiple times, this must use an UnsafeSharedMemoryRegion instead.
  base::UnsafeSharedMemoryRegion region_;
  base::WritableSharedMemoryMapping mapping_;
  size_t offsets_[kMaxPlanes];
  MojoSharedBufferDoneCB mojo_shared_buffer_done_cb_;
};

}  // namespace media

#endif  // MEDIA_MOJO_COMMON_MOJO_SHARED_BUFFER_VIDEO_FRAME_H_
