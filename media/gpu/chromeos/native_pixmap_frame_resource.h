// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_NATIVE_PIXMAP_FRAME_RESOURCE_H_
#define MEDIA_GPU_CHROMEOS_NATIVE_PIXMAP_FRAME_RESOURCE_H_

#include <optional>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_frame_metadata.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"

namespace media {

// Implements a FrameResource that is backed by a gfx::NativePixmapDmaBuf. The
// frame's pixel content is only accessible by mapping the frame using a
// GenericDmaBufVideoFrameMapper. IsMappable() returns false and all data
// accessors return nullptr.
class NativePixmapFrameResource : public FrameResource {
 public:
  NativePixmapFrameResource() = delete;
  NativePixmapFrameResource(const NativePixmapFrameResource&) = delete;
  NativePixmapFrameResource& operator=(const NativePixmapFrameResource&) =
      delete;

  // Creates a NativePixmapFrameResource that assumes ownership of |dmabuf_fds|.
  // NOTE: This is only intended to be used to wrap DMA buffers that were not
  // allocated by miniGBM. If this changes, additional arguments for the buffer
  // modifier and whether WebGPU can directly import the handle to create
  // texture from it will need to be added.
  static scoped_refptr<NativePixmapFrameResource> Create(
      const media::VideoFrameLayout& layout,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      std::vector<base::ScopedFD> dmabuf_fds,
      base::TimeDelta timestamp);

  // Uses MiniGBM to allocate a NativePixmapFrameResource.
  static scoped_refptr<NativePixmapFrameResource> Create(
      media::VideoPixelFormat pixel_format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp,
      gfx::BufferUsage buffer_usage);

  // Creates a NativePixmapFrameResource from a NativePixmapDmaBuf.
  static scoped_refptr<NativePixmapFrameResource> Create(
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp,
      gfx::BufferUsage buffer_usage,
      scoped_refptr<const gfx::NativePixmapDmaBuf> pixmap);

  // FrameResource implementation.
  const NativePixmapFrameResource* AsNativePixmapFrameResource() const override;

  // IsMappable() returns false. There is no direct data access to the buffers
  // without use of a GenericVideoFrameMapper.
  bool IsMappable() const override;
  const uint8_t* data(size_t plane) const override;
  uint8_t* writable_data(size_t plane) override;
  const uint8_t* visible_data(size_t plane) const override;
  uint8_t* GetWritableVisibleData(size_t plane) override;
  size_t NumDmabufFds() const override;
  int GetDmabufFd(size_t i) const override;
  scoped_refptr<const gfx::NativePixmapDmaBuf> GetNativePixmapDmaBuf()
      const override;
  // CreateGpuMemoryBufferHandle() will duplicate file descriptors to make a
  // gfx::GpuMemoryBufferHandle. The GpuMemoryBufferId will match
  // GetSharedMemoryId(). Doing this helps with identification of original
  // FrameResource from a VideoFrame produced by CreateVideoFrame().
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle() const override;
  // Always returns nullptr.
  std::unique_ptr<VideoFrame::ScopedMapping> MapGMBOrSharedImage()
      const override;
  gfx::GenericSharedMemoryId GetSharedMemoryId() const override;
  const VideoFrameLayout& layout() const override;
  VideoPixelFormat format() const override;
  int stride(size_t plane) const override;
  VideoFrame::StorageType storage_type() const override;
  int row_bytes(size_t plane) const override;
  const gfx::Size& coded_size() const override;
  const gfx::Rect& visible_rect() const override;
  const gfx::Size& natural_size() const override;
  const VideoFrameMetadata& metadata() const override;
  VideoFrameMetadata& metadata() override;
  void set_metadata(const VideoFrameMetadata& metadata) override;
  gfx::ColorSpace ColorSpace() const override;
  void set_color_space(const gfx::ColorSpace& color_space) override;
  const std::optional<gfx::HDRMetadata>& hdr_metadata() const override;
  void set_hdr_metadata(
      const std::optional<gfx::HDRMetadata>& hdr_metadata) override;
  base::TimeDelta timestamp() const override;
  void set_timestamp(base::TimeDelta timestamp) override;
  void AddDestructionObserver(base::OnceClosure callback) override;
  scoped_refptr<FrameResource> CreateWrappingFrame(
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size) override;
  std::string AsHumanReadableString() const override;
  // Always returns empty handle.
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandleForTesting()
      const override;

  // CreateVideoFrame() is used to create a VideoFrame from the underlying
  // NativePixmap. The DMABuf FDs are duplicated and a VideoFrame with storage
  // type GPU_MEMORY_BUFFER is created. The GpuMemoryBufferId of the returned
  // frame equals |this->id_|. This is important to allow for frame pool frame
  // reclamation.
  scoped_refptr<VideoFrame> CreateVideoFrame() const;

 private:
  ~NativePixmapFrameResource() override;

  // The underlying NativePixmap is constructed from |handle|.
  NativePixmapFrameResource(const media::VideoFrameLayout& layout,
                            const gfx::Rect& visible_rect,
                            const gfx::Size& natural_size,
                            base::TimeDelta timestamp,
                            gfx::BufferFormat buffer_format,
                            gfx::GenericSharedMemoryId id,
                            std::optional<gfx::BufferUsage> buffer_usage,
                            gfx::NativePixmapHandle handle);

  NativePixmapFrameResource(
      const media::VideoFrameLayout& layout,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp,
      gfx::GenericSharedMemoryId id,
      std::optional<gfx::BufferUsage> buffer_usage,
      scoped_refptr<const gfx::NativePixmapDmaBuf> pixmap);

  // |pixmap_| is the underlying NativePixmap. It is is set by the constructors.
  const scoped_refptr<const gfx::NativePixmapDmaBuf> pixmap_;

  // |id_| is generated by the factory functions. It starts with 0. When a frame
  // is wrapped, |id_| is copied to the wrapping frame. The ID's will be unique
  // per underlying NativePixmapDmaBuf object, per process. The ID is generated
  // by and stored in NativePixmapFrameResource because the ID returned by
  // gxf::NativePixmapDmaBuf::GetUniqueId() is currently always zero.
  const gfx::GenericSharedMemoryId id_;

  // |buffer_usage_| affects how a buffer can be used. It is only set if it was
  // provided by the caller of Create(), or if the NativePixmap was allocated by
  // MiniGBM. If this not set, then CreateVideoFrame() will fail.
  const std::optional<gfx::BufferUsage> buffer_usage_;

  // VideoFrameLayout (includes format, coded_size, and strides). Per-plane
  // metadata is redundant with NativePixmapDmabuf::planes. Factory functions
  // ensure consistency between |layout_| and |pixmap_|.
  const media::VideoFrameLayout layout_;

  // Width, height, and offsets of the visible portion of the video frame. Must
  // be a subrect of |coded_size_|. Can be odd with respect to the sample
  // boundaries, e.g. for formats with subsampled chroma.
  const gfx::Rect visible_rect_;

  // Width and height of the visible portion of the video frame
  // (|visible_rect_.size()|) with aspect ratio taken into account.
  const gfx::Size natural_size_;

  media::VideoFrameMetadata metadata_;

  base::TimeDelta timestamp_;

  gfx::ColorSpace color_space_;
  std::optional<gfx::HDRMetadata> hdr_metadata_;

  // Callbacks are added by AddDestructionObserver(). It is unclear whether
  // guarding |done_callbacks_| is necessary. VideoFrame has a similar lock,
  // which is why |done_callbacks_lock_| was added to NativePixmapFrameResource.
  // VideoFrame's lock may not be necessary for the workflows where
  // NativePixmapFrameResource is used.
  // TODO(nhebert): Add a UMA to log concurrent access to
  // AddDestructionObserver().
  base::Lock done_callbacks_lock_;
  std::vector<base::OnceClosure> done_callbacks_
      GUARDED_BY(done_callbacks_lock_);
};

}  // namespace media
#endif  // MEDIA_GPU_CHROMEOS_NATIVE_PIXMAP_FRAME_RESOURCE_H_
