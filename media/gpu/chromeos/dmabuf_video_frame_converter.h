// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_DMABUF_VIDEO_FRAME_CONVERTER_H_
#define MEDIA_GPU_CHROMEOS_DMABUF_VIDEO_FRAME_CONVERTER_H_

#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/chromeos/frame_resource_converter.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// DmabufVideoFrameConverter can be used to convert a NativePixmapFrameResource
// to a STORAGE_DMABUFS VideoFrame. It is used by the decoder utility process to
// transport DMA buffer-backed VideoFrames to the OOPVideoDecoder.
class MEDIA_GPU_EXPORT DmabufVideoFrameConverter
    : public FrameResourceConverter {
 public:
  static std::unique_ptr<FrameResourceConverter> Create();

  DmabufVideoFrameConverter(const DmabufVideoFrameConverter&) = delete;
  DmabufVideoFrameConverter& operator=(const DmabufVideoFrameConverter&) =
      delete;

 private:
  DmabufVideoFrameConverter() = default;
  ~DmabufVideoFrameConverter() override = default;

  // FrameConverter overrides.
  void ConvertFrameImpl(scoped_refptr<FrameResource> frame) override;
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_DMABUF_VIDEO_FRAME_CONVERTER_H_
