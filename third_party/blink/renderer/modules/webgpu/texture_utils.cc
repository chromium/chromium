// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/texture_utils.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

namespace {

struct TexelBlockInfo {
  uint32_t byteSize;
  uint32_t width;
  uint32_t height;
};

bool ValidateFormatAndAspectForCopy(WGPUTextureFormat format,
                                    WGPUTextureAspect aspect) {
  switch (format) {
    // For depth/stencil formats, see the valid format and aspect combinations
    // for copy at https://gpuweb.github.io/gpuweb/#depth-formats
    case WGPUTextureFormat_Stencil8:
      return aspect != WGPUTextureAspect_DepthOnly;

    case WGPUTextureFormat_Depth16Unorm:
      return aspect != WGPUTextureAspect_StencilOnly;

    case WGPUTextureFormat_Depth24Plus:
    // Depth32Float is not copyable when it is used as copy dst in WriteTexture
    case WGPUTextureFormat_Depth32Float:
      return false;

    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth24UnormStencil8:
    // Depth aspect of Depth32FloatStencil8 is not copyable when it is used as
    // copy dst in WriteTexture
    case WGPUTextureFormat_Depth32FloatStencil8:
      return aspect == WGPUTextureAspect_StencilOnly;

    // These formats are not copyable in WriteTexture
    case WGPUTextureFormat_R8BG8Biplanar420Unorm:
    case WGPUTextureFormat_Force32:
    case WGPUTextureFormat_Undefined:
      return false;

    default:
      return aspect == WGPUTextureAspect_All;
  }
}

TexelBlockInfo GetTexelBlockInfoForCopy(WGPUTextureFormat format,
                                        WGPUTextureAspect aspect) {
  if (!ValidateFormatAndAspectForCopy(format, aspect)) {
    return {0u, 0u, 0u};
  }

  switch (format) {
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
      return {1u, 1u, 1u};

    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
      return {2u, 1u, 1u};

    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
      return {4u, 1u, 1u};

    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
      return {8u, 1u, 1u};

    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
      return {16u, 1u, 1u};

    case WGPUTextureFormat_Stencil8:
      return {1u, 1u, 1u};

    case WGPUTextureFormat_Depth16Unorm:
      return {2u, 1u, 1u};

    // Only stencil aspect is valid for WriteTexture
    case WGPUTextureFormat_Depth24UnormStencil8:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32FloatStencil8:
      return {1u, 1u, 1u};

    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
      return {8u, 4u, 4u};

    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
      return {16u, 4u, 4u};

    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
      return {8u, 4u, 4u};

    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
      return {16u, 4u, 4u};

    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
      return {16u, 4u, 4u};
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
      return {16u, 5u, 4u};
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
      return {16u, 5u, 5u};
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
      return {16u, 6u, 5u};
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
      return {16u, 6u, 6u};
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
      return {16u, 8u, 5u};
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
      return {16u, 8u, 6u};
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
      return {16u, 8u, 8u};
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
      return {16u, 10u, 5u};
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
      return {16u, 10u, 6u};
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
      return {16u, 10u, 8u};
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
      return {16u, 10u, 10u};
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
      return {16u, 12u, 10u};
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
      return {16u, 12u, 12u};

    default:
      NOTREACHED();
      return {0u, 0u, 0u};
  }
}

}  // anonymous namespace

bool ComputeAndValidateRequiredBytesInCopy(size_t data_size,
                                           WGPUTextureDataLayout layout,
                                           WGPUExtent3D extent,
                                           WGPUTextureFormat format,
                                           WGPUTextureAspect aspect,
                                           size_t* required_copy_size,
                                           GPUDevice* device) {
  TexelBlockInfo blockInfo = GetTexelBlockInfoForCopy(format, aspect);
  if (!blockInfo.byteSize) {
    device->InjectError(
        WGPUErrorType_Validation,
        "Format, aspect or the combination are not valid for WriteTexture");
    return false;
  }

  uint32_t widthInBlocks = extent.width / blockInfo.width;
  uint32_t heightInBlocks = extent.height / blockInfo.height;
  size_t lastRowBytes = widthInBlocks * blockInfo.byteSize;

  if (layout.bytesPerRow == WGPU_STRIDE_UNDEFINED &&
      (heightInBlocks > 1 || extent.depthOrArrayLayers > 1)) {
    device->InjectError(WGPUErrorType_Validation,
                        "bytesPerRow must be specified");
    return false;
  }

  if (layout.rowsPerImage == WGPU_STRIDE_UNDEFINED &&
      extent.depthOrArrayLayers > 1) {
    device->InjectError(WGPUErrorType_Validation,
                        "rowsPerImage must be specified");
    return false;
  }

  if (layout.bytesPerRow < lastRowBytes) {
    device->InjectError(WGPUErrorType_Validation,
                        "bytesPerRow in image data layout is too small");
    return false;
  }

  if (layout.rowsPerImage < heightInBlocks) {
    device->InjectError(WGPUErrorType_Validation,
                        "rowsPerImage in image data layout is too small");
    return false;
  }

  if (extent.depthOrArrayLayers == 0) {
    *required_copy_size = 0;
    return true;
  }

  base::CheckedNumeric<size_t> requiredBytesInCopy = 0;
  if (extent.depthOrArrayLayers > 1) {
    requiredBytesInCopy = layout.bytesPerRow;
    requiredBytesInCopy *= layout.rowsPerImage;
    requiredBytesInCopy *= (extent.depthOrArrayLayers - 1);
  }
  if (heightInBlocks != 0) {
    size_t lastImageBytes =
        layout.bytesPerRow * (heightInBlocks - 1) + lastRowBytes;
    requiredBytesInCopy += lastImageBytes;
  }

  if (!requiredBytesInCopy.IsValid()) {
    device->InjectError(WGPUErrorType_Validation,
                        "Required copy size overflows");
    return false;
  }

  *required_copy_size = requiredBytesInCopy.ValueOrDie();
  DCHECK(data_size >= layout.offset);
  if (*required_copy_size > data_size - layout.offset) {
    device->InjectError(
        WGPUErrorType_Validation,
        "Required copy size for texture layout exceed data size with offset");
    return false;
  }

  return true;
}

}  // namespace blink
