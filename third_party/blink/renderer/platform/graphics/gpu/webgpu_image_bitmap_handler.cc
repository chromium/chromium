// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_image_bitmap_handler.h"

#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

namespace {
static constexpr uint64_t kDawnBytesPerRowAlignmentBits = 8;

// Calculate bytes per row for T2B/B2T copy
// TODO(shaobo.yan@intel.com): Using Dawn's constants once they are exposed
uint64_t AlignWebGPUBytesPerRow(uint64_t bytesPerRow) {
  return (((bytesPerRow - 1) >> kDawnBytesPerRowAlignmentBits) + 1)
         << kDawnBytesPerRowAlignmentBits;
}

bool IsPaintImageReadAllChannels(WGPUTextureFormat dawn_format) {
  switch (dawn_format) {
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_RG32Float:
      return false;
    default:
      return true;
  }
}

// SkImageInfo doesn't support R8Unorm, R16Float, R32Float and RG32Float.
// So we config these formats to the 2 or 4 channel compatible ones to read
// pixels.
WGPUTextureFormat DawnColorTypeToCreateSkImageInfo(
    WGPUTextureFormat dawn_format) {
  switch (dawn_format) {
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_RG32Float:
      return WGPUTextureFormat_RGBA32Float;
    case WGPUTextureFormat_R8Unorm:
      return WGPUTextureFormat_RG8Unorm;
    case WGPUTextureFormat_R16Float:
      return WGPUTextureFormat_RG16Float;
    default:
      return dawn_format;
  }
}

SkColorType DawnColorTypeToSkColorType(WGPUTextureFormat dawn_format) {
  switch (dawn_format) {
    case WGPUTextureFormat_RGBA8Unorm:
    // According to WebGPU spec, format with -srgb suffix will do color
    // space conversion when reading and writing in shader. In this uploading
    // path, we should keep the conversion happening in canvas color space and
    // leave the srgb color space conversion to the GPU.
    case WGPUTextureFormat_RGBA8UnormSrgb:
      return SkColorType::kRGBA_8888_SkColorType;
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
      return SkColorType::kBGRA_8888_SkColorType;
    case WGPUTextureFormat_RGB10A2Unorm:
      return SkColorType::kRGBA_1010102_SkColorType;
    case WGPUTextureFormat_RGBA16Float:
      return SkColorType::kRGBA_F16_SkColorType;
    case WGPUTextureFormat_RGBA32Float:
      return SkColorType::kRGBA_F32_SkColorType;
    case WGPUTextureFormat_RG8Unorm:
      return SkColorType::kR8G8_unorm_SkColorType;
    case WGPUTextureFormat_RG16Float:
      return SkColorType::kR16G16_float_SkColorType;
    default:
      return SkColorType::kUnknown_SkColorType;
  }
}

}  // anonymous namespace

WebGPUImageUploadSizeInfo ComputeImageBitmapWebGPUUploadSizeInfo(
    const gfx::Rect& rect,
    const WGPUTextureFormat& destination_format) {
  WebGPUImageUploadSizeInfo info;

  uint64_t bytes_per_pixel = DawnTextureFormatBytesPerPixel(destination_format);
  DCHECK_NE(bytes_per_pixel, 0u);

  uint64_t bytes_per_row =
      AlignWebGPUBytesPerRow(rect.width() * bytes_per_pixel);

  // Currently, bytes per row for buffer copy view in WebGPU is an uint32_t type
  // value and the maximum value is std::numeric_limits<uint32_t>::max().
  DCHECK(bytes_per_row <= std::numeric_limits<uint32_t>::max());

  info.wgpu_bytes_per_row = static_cast<uint32_t>(bytes_per_row);
  info.size_in_bytes = bytes_per_row * rect.height();

  return info;
}

bool CopyBytesFromImageBitmapForWebGPU(
    scoped_refptr<StaticBitmapImage> image,
    base::span<uint8_t> dst,
    const gfx::Rect& rect,
    const WGPUTextureFormat destination_format,
    bool premultipliedAlpha,
    bool flipY) {
  DCHECK(image);
  DCHECK_GT(dst.size(), static_cast<size_t>(0));
  DCHECK(image->width() - rect.x() >= rect.width());
  DCHECK(image->height() - rect.y() >= rect.height());
  DCHECK(rect.width());
  DCHECK(rect.height());

  WebGPUImageUploadSizeInfo dst_info =
      ComputeImageBitmapWebGPUUploadSizeInfo(rect, destination_format);
  DCHECK_EQ(static_cast<uint64_t>(dst.size()), dst_info.size_in_bytes);

  // Prepare extract data from SkImage.
  SkColorType sk_color_type = DawnColorTypeToSkColorType(
      DawnColorTypeToCreateSkImageInfo(destination_format));
  if (sk_color_type == kUnknown_SkColorType) {
    return false;
  }
  PaintImage paint_image = image->PaintImageForCurrentFrame();

  bool read_all_channels = IsPaintImageReadAllChannels(destination_format);

  // Read pixel request dst info.
  // TODO(crbug.com/1217153): Convert to user-provided color space.
  SkImageInfo info = SkImageInfo::Make(
      rect.width(), rect.height(), sk_color_type,
      premultipliedAlpha ? kPremul_SkAlphaType : kUnpremul_SkAlphaType,
      paint_image.GetSkImageInfo().refColorSpace());

  if (!flipY && read_all_channels) {
    return paint_image.readPixels(info, dst.data(), dst_info.wgpu_bytes_per_row,
                                  rect.x(), rect.y());
  } else {
    // Calculate size info if the destination format has been converted to the
    // compatible ones.
    WGPUTextureFormat compatible_dawn_format =
        DawnColorTypeToCreateSkImageInfo(destination_format);
    WebGPUImageUploadSizeInfo pixel_info =
        ComputeImageBitmapWebGPUUploadSizeInfo(rect, compatible_dawn_format);

    std::vector<uint8_t> paint_image_content;
    paint_image_content.resize(pixel_info.wgpu_bytes_per_row * rect.height());
    if (!paint_image.readPixels(info, paint_image_content.data(),
                                pixel_info.wgpu_bytes_per_row, rect.x(),
                                rect.y())) {
      return false;
    }
    // Do flipY for the bottom left image.
    if (flipY && read_all_channels) {
      for (int i = 0; i < rect.height(); ++i) {
        memcpy(
            dst.data() + (rect.height() - 1 - i) * dst_info.wgpu_bytes_per_row,
            paint_image_content.data() + i * dst_info.wgpu_bytes_per_row,
            dst_info.wgpu_bytes_per_row);
      }
    } else {
      // Copy from required channels pixel by pixel and do flipY if needed.
      uint32_t destination_format_pixel_bytes = static_cast<uint32_t>(
          DawnTextureFormatBytesPerPixel(destination_format));
      uint32_t paint_image_pixel_bytes = static_cast<uint32_t>(
          DawnTextureFormatBytesPerPixel(compatible_dawn_format));

      for (int i = 0; i < rect.height(); ++i) {
        uint32_t dst_height = flipY ? rect.height() - 1 - i : i;
        for (int j = 0; j < rect.width(); ++j) {
          memcpy(dst.data() + dst_height * dst_info.wgpu_bytes_per_row +
                     j * destination_format_pixel_bytes,
                 paint_image_content.data() +
                     i * pixel_info.wgpu_bytes_per_row +
                     j * paint_image_pixel_bytes,
                 destination_format_pixel_bytes);
        }
      }
    }
  }

  return true;
}

uint64_t DawnTextureFormatBytesPerPixel(const WGPUTextureFormat color_type) {
  switch (color_type) {
    case WGPUTextureFormat_R8Unorm:
      return 1;
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_R16Float:
      return 2;
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_R32Float:
      return 4;
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RG32Float:
      return 8;
    case WGPUTextureFormat_RGBA32Float:
      return 16;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace blink
