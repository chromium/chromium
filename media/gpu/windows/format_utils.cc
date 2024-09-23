// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/format_utils.h"

#include "base/notreached.h"

namespace media {

size_t GetFormatPlaneCount(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_NV11:
      return 2;
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_YUY2:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return 1;
    default:
      NOTREACHED();
  }
}

const char* DxgiFormatToString(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_Y416:
      return "Y416";
    case DXGI_FORMAT_Y216:
      return "Y216";
    case DXGI_FORMAT_P016:
      return "P016";
    case DXGI_FORMAT_NV12:
      return "NV12";
    case DXGI_FORMAT_P010:
      return "P010";
    case DXGI_FORMAT_Y210:
      return "Y210";
    case DXGI_FORMAT_AYUV:
      return "AYUV";
    case DXGI_FORMAT_Y410:
      return "Y410";
    case DXGI_FORMAT_YUY2:
      return "YUY2";
    default:
      return "UNKNOWN";
  }
}

}  // namespace media
