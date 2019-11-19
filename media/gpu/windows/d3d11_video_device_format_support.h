// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VIDEO_DEVICE_FORMAT_SUPPORT_H_
#define MEDIA_GPU_WINDOWS_D3D11_VIDEO_DEVICE_FORMAT_SUPPORT_H_

#include <d3d11_1.h>
#include <vector>

#include "base/optional.h"
#include "media/base/media_log.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_com_defs.h"

namespace media {

// Helper class for Checking whether a video can be processed in any given
// DXVI_FORMAT.
class MEDIA_GPU_EXPORT FormatSupportChecker {
 public:
  explicit FormatSupportChecker(ComD3D11Device device);
  ~FormatSupportChecker();

  // Set up the device to be able to check format support.
  // Returns false if there is a failure.
  bool Initialize();

  // Checks if the device's texture processing pipeline supports output textures
  bool CheckOutputFormatSupport(DXGI_FORMAT format);

 private:
  ComD3D11Device device_;
  ComD3D11VideoProcessorEnumerator enumerator_;
  bool initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(FormatSupportChecker);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VIDEO_DEVICE_FORMAT_SUPPORT_H_
