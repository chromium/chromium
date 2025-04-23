// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_GMB_VIDEO_FRAME_CONVERTER_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_GMB_VIDEO_FRAME_CONVERTER_H_

#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/chromeos/frame_resource_converter.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// GmbVideoFrameConverter converts a NativePixmapFrameResource to a GMB-backed
// VideoFrame. It is used for decoder tests.
class MEDIA_GPU_EXPORT GmbVideoFrameConverter : public FrameResourceConverter {
 public:
  static std::unique_ptr<FrameResourceConverter> CreateForTesting();

  GmbVideoFrameConverter(const GmbVideoFrameConverter&) = delete;
  GmbVideoFrameConverter& operator=(const GmbVideoFrameConverter&) = delete;

 private:
  GmbVideoFrameConverter();
  ~GmbVideoFrameConverter() override = default;

  // FrameConverter overrides.
  void ConvertFrameImpl(scoped_refptr<FrameResource> frame) override;
};

}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_GMB_VIDEO_FRAME_CONVERTER_H_
