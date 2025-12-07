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

DXGI_FORMAT VideoPixelFormatToDxgiFormat(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_I420:
      return DXGI_FORMAT_420_OPAQUE;
    case PIXEL_FORMAT_NV12:
      return DXGI_FORMAT_NV12;
    case PIXEL_FORMAT_ARGB:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case PIXEL_FORMAT_P010LE:
      return DXGI_FORMAT_P010;
    default:
      return DXGI_FORMAT_UNKNOWN;
  }
}

bool IsRec709(const gfx::ColorSpace& color_space) {
  return color_space.GetPrimaryID() == gfx::ColorSpace::PrimaryID::BT709 &&
         color_space.GetTransferID() == gfx::ColorSpace::TransferID::BT709 &&
         color_space.GetMatrixID() == gfx::ColorSpace::MatrixID::BT709;
}

bool IsRec601(const gfx::ColorSpace& color_space) {
  return color_space.GetPrimaryID() == gfx::ColorSpace::PrimaryID::SMPTE170M &&
         color_space.GetTransferID() ==
             gfx::ColorSpace::TransferID::SMPTE170M &&
         color_space.GetMatrixID() == gfx::ColorSpace::MatrixID::SMPTE170M;
}

gfx::ColorSpace GetEncoderOutputColorSpaceFromInputColorSpace(
    const gfx::ColorSpace& input_color_space) {
  gfx::ColorSpace output_color_space = input_color_space;
  if (input_color_space.GetMatrixID() == gfx::ColorSpace::MatrixID::RGB) {
    if (input_color_space.GetPrimaryID() ==
        gfx::ColorSpace::PrimaryID::SMPTE170M) {
      output_color_space = input_color_space.GetWithMatrixAndRange(
          gfx::ColorSpace::MatrixID::SMPTE170M, input_color_space.GetRangeID());
    } else if (input_color_space.GetPrimaryID() ==
               gfx::ColorSpace::PrimaryID::BT2020) {
      output_color_space = input_color_space.GetWithMatrixAndRange(
          gfx::ColorSpace::MatrixID::BT2020_NCL,
          input_color_space.GetRangeID());
    } else {
      output_color_space = input_color_space.GetWithMatrixAndRange(
          gfx::ColorSpace::MatrixID::BT709, input_color_space.GetRangeID());
    }
  }
  return output_color_space;
}

}  // namespace media
