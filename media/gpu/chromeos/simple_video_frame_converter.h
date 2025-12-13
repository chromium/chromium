// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_SIMPLE_VIDEO_FRAME_CONVERTER_H_
#define MEDIA_GPU_CHROMEOS_SIMPLE_VIDEO_FRAME_CONVERTER_H_

#include "media/gpu/chromeos/frame_resource_converter.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// SimpleVideoFrameConverter is used to retrieve VideoFrame from the
// VideoFrameResource that is wrapped by OOPVideoDecoder.
class MEDIA_GPU_EXPORT SimpleVideoFrameConverter
    : public FrameResourceConverter {
 public:
  static std::unique_ptr<FrameResourceConverter> Create();

  SimpleVideoFrameConverter(const SimpleVideoFrameConverter&) = delete;
  SimpleVideoFrameConverter& operator=(const SimpleVideoFrameConverter&) =
      delete;

 private:
  SimpleVideoFrameConverter() = default;
  ~SimpleVideoFrameConverter() override = default;

  // FrameConverter overrides.
  void ConvertFrameImpl(scoped_refptr<FrameResource> frame) override;
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_SIMPLE_VIDEO_FRAME_CONVERTER_H_
