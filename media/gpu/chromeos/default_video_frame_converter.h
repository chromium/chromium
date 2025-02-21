// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_DEFAULT_VIDEO_FRAME_CONVERTER_H_
#define MEDIA_GPU_CHROMEOS_DEFAULT_VIDEO_FRAME_CONVERTER_H_

#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/chromeos/frame_resource_converter.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// DefaultFrameConverter uses the FrameResource built-in converters to handle
// conversion to VideoFrame objects. It is used by VideoDecoderPipeline when a
// client doesn't specify a FrameConverter.
class MEDIA_GPU_EXPORT DefaultFrameConverter : public FrameResourceConverter {
 public:
  static std::unique_ptr<FrameResourceConverter> Create();

  DefaultFrameConverter(const DefaultFrameConverter&) = delete;
  DefaultFrameConverter& operator=(const DefaultFrameConverter&) = delete;

 private:
  DefaultFrameConverter() = default;
  ~DefaultFrameConverter() override = default;

  // FrameConverter overrides.
  void ConvertFrameImpl(scoped_refptr<FrameResource> frame) override;
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_DEFAULT_VIDEO_FRAME_CONVERTER_H_
