// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_HELPERS_H_
#define MEDIA_CDM_CDM_HELPERS_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/media_export.h"
#include "media/base/video_color_space.h"
#include "media/cdm/api/content_decryption_module.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoFrame;

// File size limit is 512KB. Licenses saved by the CDM are typically several
// hundreds of bytes.
constexpr int64_t kMaxFileSizeBytes = 512 * 1024;

class DecryptedBlockImpl final : public cdm::DecryptedBlock {
 public:
  DecryptedBlockImpl();

  DecryptedBlockImpl(const DecryptedBlockImpl&) = delete;
  DecryptedBlockImpl& operator=(const DecryptedBlockImpl&) = delete;

  ~DecryptedBlockImpl() final;

  // cdm::DecryptedBlock implementation.
  void SetDecryptedBuffer(cdm::Buffer* buffer) final;
  cdm::Buffer* DecryptedBuffer() final;
  void SetTimestamp(int64_t timestamp) final;
  int64_t Timestamp() const final;

 private:
  raw_ptr<cdm::Buffer> buffer_;
  int64_t timestamp_;
};

class MEDIA_EXPORT VideoFrameImpl : public cdm::VideoFrame,
                                    public cdm::VideoFrame_2 {
 public:
  VideoFrameImpl();

  VideoFrameImpl(const VideoFrameImpl&) = delete;
  VideoFrameImpl& operator=(const VideoFrameImpl&) = delete;

  ~VideoFrameImpl() override;

  // cdm::VideoFrame and cdm::VideoFrame_2 common implementation.
  void SetFormat(cdm::VideoFormat format) final;
  cdm::VideoFormat Format() const final;
  void SetSize(cdm::Size size) final;
  cdm::Size Size() const final;
  void SetFrameBuffer(cdm::Buffer* frame_buffer) final;
  cdm::Buffer* FrameBuffer() final;
  void SetPlaneOffset(cdm::VideoPlane plane, uint32_t offset) final;
  uint32_t PlaneOffset(cdm::VideoPlane plane) final;
  void SetStride(cdm::VideoPlane plane, uint32_t stride) final;
  uint32_t Stride(cdm::VideoPlane plane) final;
  void SetTimestamp(int64_t timestamp) final;
  int64_t Timestamp() const final;

  // cdm::VideoFrame_2 specific implementation.
  void SetColorSpace(cdm::ColorSpace color_space) final;

  // Helper functions to get the ColorSpace.
  media::VideoColorSpace MediaColorSpace() const;

  // Create a media::VideoFrame based on the data contained in this object.
  // |natural_size| is the visible portion of the video frame, and is
  // provided separately as it comes from the configuration, not the CDM.
  // The returned object will own |frame_buffer_| and will be responsible for
  // calling Destroy() on it when the data is no longer needed.
  // Preconditions:
  // - |this| needs to be properly initialized.
  // Postconditions:
  // - |frame_buffer_| will be NULL (now owned by returned media::VideoFrame).
  virtual scoped_refptr<media::VideoFrame> TransformToVideoFrame(
      gfx::Size natural_size) = 0;

 protected:
  // The video buffer format.
  cdm::VideoFormat format_;

  cdm::ColorSpace color_space_;

  // Width and height of the video frame.
  cdm::Size size_;

  // The video frame buffer.
  raw_ptr<cdm::Buffer> frame_buffer_;

  // Array of data pointers to each plane in the video frame buffer.
  uint32_t plane_offsets_[cdm::kMaxPlanes];

  // Array of strides for each plane, typically greater or equal to the width
  // of the surface divided by the horizontal sampling period.  Note that
  // strides can be negative.
  uint32_t strides_[cdm::kMaxPlanes];

  // Presentation timestamp in microseconds.
  int64_t timestamp_;
};

class AudioFramesImpl final : public cdm::AudioFrames {
 public:
  AudioFramesImpl();

  AudioFramesImpl(const AudioFramesImpl&) = delete;
  AudioFramesImpl& operator=(const AudioFramesImpl&) = delete;

  ~AudioFramesImpl() final;

  // cdm::AudioFrames implementation.
  void SetFrameBuffer(cdm::Buffer* buffer) final;
  cdm::Buffer* FrameBuffer() final;
  void SetFormat(cdm::AudioFormat format) final;
  cdm::AudioFormat Format() const final;

  cdm::Buffer* PassFrameBuffer();

 private:
  raw_ptr<cdm::Buffer> buffer_;
  cdm::AudioFormat format_;
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_HELPERS_H_
