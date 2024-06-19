// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VIDEO_DEVICE_FORMAT_SUPPORT_H_
#define MEDIA_GPU_WINDOWS_D3D11_VIDEO_DEVICE_FORMAT_SUPPORT_H_

#include <d3d11_1.h>

#include "media/base/media_log.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d_com_defs.h"

namespace media {

// Helper class for Checking whether a video can be processed in any given
// DXGI_FORMAT.
class MEDIA_GPU_EXPORT FormatSupportChecker {
 public:
  // |device| may be null, mostly for tests.
  explicit FormatSupportChecker(ComD3D11Device device);

  FormatSupportChecker(const FormatSupportChecker&) = delete;
  FormatSupportChecker& operator=(const FormatSupportChecker&) = delete;

  virtual ~FormatSupportChecker();

  // Set up the device to be able to check format support.
  // Returns false if there is a failure.
  virtual bool Initialize();

  // Checks if the device's texture processing pipeline supports output textures
  virtual bool CheckOutputFormatSupport(DXGI_FORMAT format) const;

 private:
  ComD3D11Device device_;
  ComD3D11VideoProcessorEnumerator enumerator_;
  bool initialized_ = false;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VIDEO_DEVICE_FORMAT_SUPPORT_H_
