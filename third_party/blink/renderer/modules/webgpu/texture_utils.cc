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

TexelBlockInfo GetTexelBlockInfoForCopy(wgpu::TextureFormat format,
                                        wgpu::TextureAspect aspect) {
  constexpr TexelBlockInfo kInvalidTexelBlockInfo = {0, 0, 0};

  switch (aspect) {
    case wgpu::TextureAspect::All:
      switch (format) {
        case wgpu::TextureFormat::R8Unorm:
        case wgpu::TextureFormat::R8Snorm:
        case wgpu::TextureFormat::R8Uint:
        case wgpu::TextureFormat::R8Sint:
          return {1u, 1u, 1u};

        case wgpu::TextureFormat::R16Uint:
        case wgpu::TextureFormat::R16Sint:
        case wgpu::TextureFormat::R16Float:
        case wgpu::TextureFormat::RG8Unorm:
        case wgpu::TextureFormat::RG8Snorm:
        case wgpu::TextureFormat::RG8Uint:
        case wgpu::TextureFormat::RG8Sint:
          return {2u, 1u, 1u};

        case wgpu::TextureFormat::R32Float:
        case wgpu::TextureFormat::R32Uint:
        case wgpu::TextureFormat::R32Sint:
        case wgpu::TextureFormat::RG16Uint:
        case wgpu::TextureFormat::RG16Sint:
        case wgpu::TextureFormat::RG16Float:
        case wgpu::TextureFormat::RGBA8Unorm:
        case wgpu::TextureFormat::RGBA8UnormSrgb:
        case wgpu::TextureFormat::RGBA8Snorm:
        case wgpu::TextureFormat::RGBA8Uint:
        case wgpu::TextureFormat::RGBA8Sint:
        case wgpu::TextureFormat::BGRA8Unorm:
        case wgpu::TextureFormat::BGRA8UnormSrgb:
        case wgpu::TextureFormat::RGB10A2Uint:
        case wgpu::TextureFormat::RGB10A2Unorm:
        case wgpu::TextureFormat::RG11B10Ufloat:
        case wgpu::TextureFormat::RGB9E5Ufloat:
          return {4u, 1u, 1u};

        case wgpu::TextureFormat::RG32Float:
        case wgpu::TextureFormat::RG32Uint:
        case wgpu::TextureFormat::RG32Sint:
        case wgpu::TextureFormat::RGBA16Uint:
        case wgpu::TextureFormat::RGBA16Sint:
        case wgpu::TextureFormat::RGBA16Float:
          return {8u, 1u, 1u};

        case wgpu::TextureFormat::RGBA32Float:
        case wgpu::TextureFormat::RGBA32Uint:
        case wgpu::TextureFormat::RGBA32Sint:
          return {16u, 1u, 1u};

        case wgpu::TextureFormat::Depth16Unorm:
          return {2u, 1u, 1u};
        case wgpu::TextureFormat::Stencil8:
          return {1u, 1u, 1u};

        case wgpu::TextureFormat::BC1RGBAUnorm:
        case wgpu::TextureFormat::BC1RGBAUnormSrgb:
        case wgpu::TextureFormat::BC4RUnorm:
        case wgpu::TextureFormat::BC4RSnorm:
          return {8u, 4u, 4u};

        case wgpu::TextureFormat::BC2RGBAUnorm:
        case wgpu::TextureFormat::BC2RGBAUnormSrgb:
        case wgpu::TextureFormat::BC3RGBAUnorm:
        case wgpu::TextureFormat::BC3RGBAUnormSrgb:
        case wgpu::TextureFormat::BC5RGUnorm:
        case wgpu::TextureFormat::BC5RGSnorm:
        case wgpu::TextureFormat::BC6HRGBUfloat:
        case wgpu::TextureFormat::BC6HRGBFloat:
        case wgpu::TextureFormat::BC7RGBAUnorm:
        case wgpu::TextureFormat::BC7RGBAUnormSrgb:
          return {16u, 4u, 4u};

        case wgpu::TextureFormat::ETC2RGB8Unorm:
        case wgpu::TextureFormat::ETC2RGB8UnormSrgb:
        case wgpu::TextureFormat::ETC2RGB8A1Unorm:
        case wgpu::TextureFormat::ETC2RGB8A1UnormSrgb:
        case wgpu::TextureFormat::EACR11Unorm:
        case wgpu::TextureFormat::EACR11Snorm:
          return {8u, 4u, 4u};

        case wgpu::TextureFormat::ETC2RGBA8Unorm:
        case wgpu::TextureFormat::ETC2RGBA8UnormSrgb:
        case wgpu::TextureFormat::EACRG11Unorm:
        case wgpu::TextureFormat::EACRG11Snorm:
          return {16u, 4u, 4u};

        case wgpu::TextureFormat::ASTC4x4Unorm:
        case wgpu::TextureFormat::ASTC4x4UnormSrgb:
          return {16u, 4u, 4u};
        case wgpu::TextureFormat::ASTC5x4Unorm:
        case wgpu::TextureFormat::ASTC5x4UnormSrgb:
          return {16u, 5u, 4u};
        case wgpu::TextureFormat::ASTC5x5Unorm:
        case wgpu::TextureFormat::ASTC5x5UnormSrgb:
          return {16u, 5u, 5u};
        case wgpu::TextureFormat::ASTC6x5Unorm:
        case wgpu::TextureFormat::ASTC6x5UnormSrgb:
          return {16u, 6u, 5u};
        case wgpu::TextureFormat::ASTC6x6Unorm:
        case wgpu::TextureFormat::ASTC6x6UnormSrgb:
          return {16u, 6u, 6u};
        case wgpu::TextureFormat::ASTC8x5Unorm:
        case wgpu::TextureFormat::ASTC8x5UnormSrgb:
          return {16u, 8u, 5u};
        case wgpu::TextureFormat::ASTC8x6Unorm:
        case wgpu::TextureFormat::ASTC8x6UnormSrgb:
          return {16u, 8u, 6u};
        case wgpu::TextureFormat::ASTC8x8Unorm:
        case wgpu::TextureFormat::ASTC8x8UnormSrgb:
          return {16u, 8u, 8u};
        case wgpu::TextureFormat::ASTC10x5Unorm:
        case wgpu::TextureFormat::ASTC10x5UnormSrgb:
          return {16u, 10u, 5u};
        case wgpu::TextureFormat::ASTC10x6Unorm:
        case wgpu::TextureFormat::ASTC10x6UnormSrgb:
          return {16u, 10u, 6u};
        case wgpu::TextureFormat::ASTC10x8Unorm:
        case wgpu::TextureFormat::ASTC10x8UnormSrgb:
          return {16u, 10u, 8u};
        case wgpu::TextureFormat::ASTC10x10Unorm:
        case wgpu::TextureFormat::ASTC10x10UnormSrgb:
          return {16u, 10u, 10u};
        case wgpu::TextureFormat::ASTC12x10Unorm:
        case wgpu::TextureFormat::ASTC12x10UnormSrgb:
          return {16u, 12u, 10u};
        case wgpu::TextureFormat::ASTC12x12Unorm:
        case wgpu::TextureFormat::ASTC12x12UnormSrgb:
          return {16u, 12u, 12u};

        default:
          return kInvalidTexelBlockInfo;
      }

    // Copies to depth/stencil aspects are fairly restricted, see
    // https://gpuweb.github.io/gpuweb/#depth-formats so we only list
    // combinations of format and aspects that can be copied to with a
    // WriteTexture.
    case wgpu::TextureAspect::DepthOnly:
      switch (format) {
        case wgpu::TextureFormat::Depth16Unorm:
          return GetTexelBlockInfoForCopy(format, wgpu::TextureAspect::All);

        default:
          return kInvalidTexelBlockInfo;
      }

    case wgpu::TextureAspect::StencilOnly:
      switch (format) {
        case wgpu::TextureFormat::Depth24PlusStencil8:
        case wgpu::TextureFormat::Depth32FloatStencil8:
          return {1u, 1u, 1u};

        case wgpu::TextureFormat::Stencil8:
          return GetTexelBlockInfoForCopy(format, wgpu::TextureAspect::All);

        default:
          return kInvalidTexelBlockInfo;
      }

    default:
      NOTREACHED_IN_MIGRATION();
      return kInvalidTexelBlockInfo;
  }
}

}  // anonymous namespace

size_t EstimateWriteTextureBytesUpperBound(wgpu::TextureDataLayout layout,
                                           wgpu::Extent3D extent,
                                           wgpu::TextureFormat format,
                                           wgpu::TextureAspect aspect) {
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
  // bytesPerRow or rowsPerImage are wgpu::kCopyStrideUndefined and used, the
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
