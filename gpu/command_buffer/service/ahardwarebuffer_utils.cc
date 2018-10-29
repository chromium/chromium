// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"

#include <android/hardware_buffer.h>

#include "base/logging.h"

namespace gpu {

bool AHardwareBufferSupportedFormat(viz::ResourceFormat format) {
  switch (format) {
    case viz::RGBA_8888:
    case viz::RGB_565:
    case viz::RGBA_F16:
    case viz::RGBX_8888:
    case viz::RGBX_1010102:
      return true;
    default:
      return false;
  }
}

unsigned int AHardwareBufferFormat(viz::ResourceFormat format) {
  DCHECK(AHardwareBufferSupportedFormat(format));
  switch (format) {
    case viz::RGBA_8888:
      return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    case viz::RGB_565:
      return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
    case viz::RGBA_F16:
      return AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
    case viz::RGBX_8888:
      return AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
    case viz::RGBX_1010102:
      return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
    default:
      NOTREACHED();
      return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
  }
}

}  // namespace gpu
