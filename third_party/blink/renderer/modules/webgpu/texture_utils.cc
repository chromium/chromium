// Copyright 2022 The Chromium Authors
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

TexelBlockInfo GetTexelBlockInfoForCopy(WGPUTextureFormat format,
                                        WGPUTextureAspect aspect) {
  constexpr TexelBlockInfo kInvalidTexelBlockInfo = {0, 0, 0};

  switch (aspect) {
    case WGPUTextureAspect_All:
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

        case WGPUTextureFormat_Depth16Unorm:
          return {2u, 1u, 1u};
        case WGPUTextureFormat_Stencil8:
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
          return kInvalidTexelBlockInfo;
      }

    // Copies to depth/stencil aspects are fairly restricted, see
    // https://gpuweb.github.io/gpuweb/#depth-formats so we only list
    // combinations of format and aspects that can be copied to with a
    // WriteTexture.
    case WGPUTextureAspect_DepthOnly:
      switch (format) {
        case WGPUTextureFormat_Depth16Unorm:
          return GetTexelBlockInfoForCopy(format, WGPUTextureAspect_All);

        default:
          return kInvalidTexelBlockInfo;
      }

    case WGPUTextureAspect_StencilOnly:
      switch (format) {
        case WGPUTextureFormat_Depth24PlusStencil8:
        case WGPUTextureFormat_Depth32FloatStencil8:
          return {1u, 1u, 1u};

        case WGPUTextureFormat_Stencil8:
          return GetTexelBlockInfoForCopy(format, WGPUTextureAspect_All);

        default:
          return kInvalidTexelBlockInfo;
      }

    default:
      NOTREACHED();
      return kInvalidTexelBlockInfo;
  }
}

}  // anonymous namespace

size_t EstimateWriteTextureBytesUpperBound(WGPUTextureDataLayout layout,
                                           WGPUExtent3D extent,
                                           WGPUTextureFormat format,
                                           WGPUTextureAspect aspect) {
  // Check for empty copies because of depth first so we can early out. Note
  // that we can't early out because of height or width being 0 because padding
  // images still need to be accounted for.
  if (extent.depthOrArrayLayers == 0) {
    return 0;
  }

  TexelBlockInfo blockInfo = GetTexelBlockInfoForCopy(format, aspect);

  // Unknown format/aspect combination will be validated by the GPU process
  // again.
  if (blockInfo.byteSize == 0) {
    return 0;
  }

  // If the block size doesn't divide the extent, a validation error will be
  // produced on the GPU process side so we don't need to guard against it.
  uint32_t widthInBlocks = extent.width / blockInfo.width;
  uint32_t heightInBlocks = extent.height / blockInfo.height;

  // Use checked numerics even though the GPU process will guard against OOB
  // because otherwise UBSan will complain about overflows. Note that if
  // bytesPerRow or rowsPerImage are WGPU_COPY_STRIDE_UNDEFINED and used, the
  // GPU process will also create a validation error because it means that they
  // are used when copySize.height/depthOrArrayLayers > 1.
  base::CheckedNumeric<size_t> requiredBytesInCopy = 0;

  // WebGPU requires that the padding bytes for images are counted, even if the
  // copy is empty.
  if (extent.depthOrArrayLayers > 1) {
    requiredBytesInCopy = layout.bytesPerRow;
    requiredBytesInCopy *= layout.rowsPerImage;
    requiredBytesInCopy *= (extent.depthOrArrayLayers - 1);
  }

  if (heightInBlocks != 0) {
    base::CheckedNumeric<size_t> lastRowBytes = widthInBlocks;
    lastRowBytes *= blockInfo.byteSize;

    base::CheckedNumeric<size_t> lastImageBytes = layout.bytesPerRow;
    lastImageBytes *= (heightInBlocks - 1);
    lastImageBytes += lastRowBytes;

    requiredBytesInCopy += lastImageBytes;
  }

  return requiredBytesInCopy.ValueOrDefault(0);
}

}  // namespace blink
