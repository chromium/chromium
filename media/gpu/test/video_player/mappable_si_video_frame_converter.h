// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_MAPPABLE_SI_VIDEO_FRAME_CONVERTER_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_MAPPABLE_SI_VIDEO_FRAME_CONVERTER_H_

#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/chromeos/frame_resource_converter.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// MappableSIVideoFrameConverter converts a NativePixmapFrameResource to a
// MappableSharedImage-backed VideoFrame. It is used for decoder tests.
class MEDIA_GPU_EXPORT MappableSIVideoFrameConverter
    : public FrameResourceConverter {
 public:
  static std::unique_ptr<FrameResourceConverter> CreateForTesting();

  MappableSIVideoFrameConverter(const MappableSIVideoFrameConverter&) = delete;
  MappableSIVideoFrameConverter& operator=(
      const MappableSIVideoFrameConverter&) = delete;

 private:
  MappableSIVideoFrameConverter();
  ~MappableSIVideoFrameConverter() override;

  // FrameConverter overrides.
  void ConvertFrameImpl(scoped_refptr<FrameResource> frame) override;

  scoped_refptr<gpu::TestSharedImageInterface> test_sii_;
};

}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_MAPPABLE_SI_VIDEO_FRAME_CONVERTER_H_
