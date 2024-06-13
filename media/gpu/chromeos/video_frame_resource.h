// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_VIDEO_FRAME_RESOURCE_H_
#define MEDIA_GPU_CHROMEOS_VIDEO_FRAME_RESOURCE_H_

#include "base/time/time.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_frame_metadata.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Implements a FrameResource that is backed by a media::VideoFrame.
class VideoFrameResource : public FrameResource {
 public:
  VideoFrameResource(const VideoFrameResource&) = delete;
  VideoFrameResource& operator=(const VideoFrameResource&) = delete;

  // Const and non-const factory functions.
  static scoped_refptr<VideoFrameResource> Create(
      scoped_refptr<VideoFrame> frame);
  static scoped_refptr<const VideoFrameResource> CreateConst(
      scoped_refptr<const VideoFrame> frame);

  // FrameResource implementation.
  VideoFrameResource* AsVideoFrameResource() override;
  bool IsMappable() const override;
  const uint8_t* data(size_t plane) const override;
  uint8_t* writable_data(size_t plane) override;
  const uint8_t* visible_data(size_t plane) const override;
  uint8_t* GetWritableVisibleData(size_t plane) override;
  size_t NumDmabufFds() const override;
  int GetDmabufFd(size_t i) const override;
  scoped_refptr<const gfx::NativePixmapDmaBuf> GetNativePixmapDmaBuf()
      const override;
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle() const override;
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
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandleForTesting()
      const override;

  // GetMutableVideoFrame() and GetVideoFrame() return a pointer to the
  // underlying VideoFrame. This lets VideoFrameResource be used to adapt code
  // to work on both VideoFrame and FrameResource types without code
  // duplication. The methods share ownership of the underlying VideoFrame, so
  // the VideoFrame pointed to by the returned scoped_refptr can outlive |this|.
  // Conversely, the underlying VideoFrame is guaranteed to remain alive as long
  // as |this| lives.
  scoped_refptr<VideoFrame> GetMutableVideoFrame();
  scoped_refptr<const VideoFrame> GetVideoFrame() const;

 private:
  explicit VideoFrameResource(scoped_refptr<const VideoFrame> frame);
  ~VideoFrameResource() override;

  const scoped_refptr<const VideoFrame> frame_;
};

}  // namespace media
#endif  // MEDIA_GPU_CHROMEOS_VIDEO_FRAME_RESOURCE_H_
