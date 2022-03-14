// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"

#include "base/check.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_index_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_predefined_color_space.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

template <>
WGPUBufferBindingType AsDawnEnum<WGPUBufferBindingType>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "uniform") {
    return WGPUBufferBindingType_Uniform;
  }
  if (webgpu_enum == "storage") {
    return WGPUBufferBindingType_Storage;
  }
  if (webgpu_enum == "read-only-storage") {
    return WGPUBufferBindingType_ReadOnlyStorage;
  }
  NOTREACHED();
  return WGPUBufferBindingType_Force32;
}

template <>
WGPUSamplerBindingType AsDawnEnum<WGPUSamplerBindingType>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "filtering") {
    return WGPUSamplerBindingType_Filtering;
  }
  if (webgpu_enum == "non-filtering") {
    return WGPUSamplerBindingType_NonFiltering;
  }
  if (webgpu_enum == "comparison") {
    return WGPUSamplerBindingType_Comparison;
  }
  NOTREACHED();
  return WGPUSamplerBindingType_Force32;
}

template <>
WGPUTextureSampleType AsDawnEnum<WGPUTextureSampleType>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "float") {
    return WGPUTextureSampleType_Float;
  }
  if (webgpu_enum == "unfilterable-float") {
    return WGPUTextureSampleType_UnfilterableFloat;
  }
  if (webgpu_enum == "depth") {
    return WGPUTextureSampleType_Depth;
  }
  if (webgpu_enum == "sint") {
    return WGPUTextureSampleType_Sint;
  }
  if (webgpu_enum == "uint") {
    return WGPUTextureSampleType_Uint;
  }
  NOTREACHED();
  return WGPUTextureSampleType_Force32;
}

template <>
WGPUStorageTextureAccess AsDawnEnum<WGPUStorageTextureAccess>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "write-only") {
    return WGPUStorageTextureAccess_WriteOnly;
  }
  NOTREACHED();
  return WGPUStorageTextureAccess_Force32;
}

template <>
WGPUTextureComponentType AsDawnEnum<WGPUTextureComponentType>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "float") {
    return WGPUTextureComponentType_Float;
  }
  if (webgpu_enum == "uint") {
    return WGPUTextureComponentType_Uint;
  }
  if (webgpu_enum == "sint") {
    return WGPUTextureComponentType_Sint;
  }
  if (webgpu_enum == "depth-comparison") {
    return WGPUTextureComponentType_DepthComparison;
  }
  NOTREACHED();
  return WGPUTextureComponentType_Force32;
}

template <>
WGPUCompareFunction AsDawnEnum<WGPUCompareFunction>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "never") {
    return WGPUCompareFunction_Never;
  }
  if (webgpu_enum == "less") {
    return WGPUCompareFunction_Less;
  }
  if (webgpu_enum == "equal") {
    return WGPUCompareFunction_Equal;
  }
  if (webgpu_enum == "less-equal") {
    return WGPUCompareFunction_LessEqual;
  }
  if (webgpu_enum == "greater") {
    return WGPUCompareFunction_Greater;
  }
  if (webgpu_enum == "not-equal") {
    return WGPUCompareFunction_NotEqual;
  }
  if (webgpu_enum == "greater-equal") {
    return WGPUCompareFunction_GreaterEqual;
  }
  if (webgpu_enum == "always") {
    return WGPUCompareFunction_Always;
  }
  NOTREACHED();
  return WGPUCompareFunction_Force32;
}

template <>
WGPUQueryType AsDawnEnum<WGPUQueryType>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "occlusion") {
    return WGPUQueryType_Occlusion;
  }
  if (webgpu_enum == "pipeline-statistics") {
    return WGPUQueryType_PipelineStatistics;
  }
  if (webgpu_enum == "timestamp") {
    return WGPUQueryType_Timestamp;
  }
  NOTREACHED();
  return WGPUQueryType_Force32;
}

template <>
WGPUPipelineStatisticName AsDawnEnum<WGPUPipelineStatisticName>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "vertex-shader-invocations") {
    return WGPUPipelineStatisticName_VertexShaderInvocations;
  }
  if (webgpu_enum == "clipper-invocations") {
    return WGPUPipelineStatisticName_ClipperInvocations;
  }
  if (webgpu_enum == "clipper-primitives-out") {
    return WGPUPipelineStatisticName_ClipperPrimitivesOut;
  }
  if (webgpu_enum == "fragment-shader-invocations") {
    return WGPUPipelineStatisticName_FragmentShaderInvocations;
  }
  if (webgpu_enum == "compute-shader-invocations") {
    return WGPUPipelineStatisticName_ComputeShaderInvocations;
  }
  NOTREACHED();
  return WGPUPipelineStatisticName_Force32;
}

template <>
WGPUTextureFormat AsDawnEnum<WGPUTextureFormat>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum.IsNull()) {
    return WGPUTextureFormat_Undefined;
  }

  // Normal 8 bit formats
  if (webgpu_enum == "r8unorm") {
    return WGPUTextureFormat_R8Unorm;
  }
  if (webgpu_enum == "r8snorm") {
    return WGPUTextureFormat_R8Snorm;
  }
  if (webgpu_enum == "r8uint") {
    return WGPUTextureFormat_R8Uint;
  }
  if (webgpu_enum == "r8sint") {
    return WGPUTextureFormat_R8Sint;
  }

  // Normal 16 bit formats
  if (webgpu_enum == "r16uint") {
    return WGPUTextureFormat_R16Uint;
  }
  if (webgpu_enum == "r16sint") {
    return WGPUTextureFormat_R16Sint;
  }
  if (webgpu_enum == "r16float") {
    return WGPUTextureFormat_R16Float;
  }
  if (webgpu_enum == "rg8unorm") {
    return WGPUTextureFormat_RG8Unorm;
  }
  if (webgpu_enum == "rg8snorm") {
    return WGPUTextureFormat_RG8Snorm;
  }
  if (webgpu_enum == "rg8uint") {
    return WGPUTextureFormat_RG8Uint;
  }
  if (webgpu_enum == "rg8sint") {
    return WGPUTextureFormat_RG8Sint;
  }

  // Normal 32 bit formats
  if (webgpu_enum == "r32uint") {
    return WGPUTextureFormat_R32Uint;
  }
  if (webgpu_enum == "r32sint") {
    return WGPUTextureFormat_R32Sint;
  }
  if (webgpu_enum == "r32float") {
    return WGPUTextureFormat_R32Float;
  }
  if (webgpu_enum == "rg16uint") {
    return WGPUTextureFormat_RG16Uint;
  }
  if (webgpu_enum == "rg16sint") {
    return WGPUTextureFormat_RG16Sint;
  }
  if (webgpu_enum == "rg16float") {
    return WGPUTextureFormat_RG16Float;
  }
  if (webgpu_enum == "rgba8unorm") {
    return WGPUTextureFormat_RGBA8Unorm;
  }
  if (webgpu_enum == "rgba8unorm-srgb") {
    return WGPUTextureFormat_RGBA8UnormSrgb;
  }
  if (webgpu_enum == "rgba8snorm") {
    return WGPUTextureFormat_RGBA8Snorm;
  }
  if (webgpu_enum == "rgba8uint") {
    return WGPUTextureFormat_RGBA8Uint;
  }
  if (webgpu_enum == "rgba8sint") {
    return WGPUTextureFormat_RGBA8Sint;
  }
  if (webgpu_enum == "bgra8unorm") {
    return WGPUTextureFormat_BGRA8Unorm;
  }
  if (webgpu_enum == "bgra8unorm-srgb") {
    return WGPUTextureFormat_BGRA8UnormSrgb;
  }

  // Packed 32 bit formats
  if (webgpu_enum == "rgb9e5ufloat") {
    return WGPUTextureFormat_RGB9E5Ufloat;
  }
  if (webgpu_enum == "rgb10a2unorm") {
    return WGPUTextureFormat_RGB10A2Unorm;
  }
  if (webgpu_enum == "rg11b10ufloat") {
    return WGPUTextureFormat_RG11B10Ufloat;
  }

  // Normal 64 bit formats
  if (webgpu_enum == "rg32uint") {
    return WGPUTextureFormat_RG32Uint;
  }
  if (webgpu_enum == "rg32sint") {
    return WGPUTextureFormat_RG32Sint;
  }
  if (webgpu_enum == "rg32float") {
    return WGPUTextureFormat_RG32Float;
  }
  if (webgpu_enum == "rgba16uint") {
    return WGPUTextureFormat_RGBA16Uint;
  }
  if (webgpu_enum == "rgba16sint") {
    return WGPUTextureFormat_RGBA16Sint;
  }
  if (webgpu_enum == "rgba16float") {
    return WGPUTextureFormat_RGBA16Float;
  }

  // Normal 128 bit formats
  if (webgpu_enum == "rgba32uint") {
    return WGPUTextureFormat_RGBA32Uint;
  }
  if (webgpu_enum == "rgba32sint") {
    return WGPUTextureFormat_RGBA32Sint;
  }
  if (webgpu_enum == "rgba32float") {
    return WGPUTextureFormat_RGBA32Float;
  }

  // Depth / Stencil formats
  if (webgpu_enum == "depth32float") {
    return WGPUTextureFormat_Depth32Float;
  }
  if (webgpu_enum == "depth32float-stencil8") {
    return WGPUTextureFormat_Depth32FloatStencil8;
  }
  if (webgpu_enum == "depth24plus") {
    return WGPUTextureFormat_Depth24Plus;
  }
  if (webgpu_enum == "depth24plus-stencil8") {
    return WGPUTextureFormat_Depth24PlusStencil8;
  }
  if (webgpu_enum == "depth24unorm-stencil8") {
    return WGPUTextureFormat_Depth24UnormStencil8;
  }
  if (webgpu_enum == "depth16unorm") {
    return WGPUTextureFormat_Depth16Unorm;
  }

  // Block Compression (BC) formats
  if (webgpu_enum == "bc1-rgba-unorm") {
    return WGPUTextureFormat_BC1RGBAUnorm;
  }
  if (webgpu_enum == "bc1-rgba-unorm-srgb") {
    return WGPUTextureFormat_BC1RGBAUnormSrgb;
  }
  if (webgpu_enum == "bc2-rgba-unorm") {
    return WGPUTextureFormat_BC2RGBAUnorm;
  }
  if (webgpu_enum == "bc2-rgba-unorm-srgb") {
    return WGPUTextureFormat_BC2RGBAUnormSrgb;
  }
  if (webgpu_enum == "bc3-rgba-unorm") {
    return WGPUTextureFormat_BC3RGBAUnorm;
  }
  if (webgpu_enum == "bc3-rgba-unorm-srgb") {
    return WGPUTextureFormat_BC3RGBAUnormSrgb;
  }
  if (webgpu_enum == "bc4-r-unorm") {
    return WGPUTextureFormat_BC4RUnorm;
  }
  if (webgpu_enum == "bc4-r-snorm") {
    return WGPUTextureFormat_BC4RSnorm;
  }
  if (webgpu_enum == "bc5-rg-unorm") {
    return WGPUTextureFormat_BC5RGUnorm;
  }
  if (webgpu_enum == "bc5-rg-snorm") {
    return WGPUTextureFormat_BC5RGSnorm;
  }
  if (webgpu_enum == "bc6h-rgb-ufloat") {
    return WGPUTextureFormat_BC6HRGBUfloat;
  }
  if (webgpu_enum == "bc6h-rgb-float") {
    return WGPUTextureFormat_BC6HRGBFloat;
  }
  if (webgpu_enum == "bc7-rgba-unorm") {
    return WGPUTextureFormat_BC7RGBAUnorm;
  }
  if (webgpu_enum == "bc7-rgba-unorm-srgb") {
    return WGPUTextureFormat_BC7RGBAUnormSrgb;
  }

  // Ericsson Compression (ETC2) formats
  if (webgpu_enum == "etc2-rgb8unorm") {
    return WGPUTextureFormat_ETC2RGB8Unorm;
  }
  if (webgpu_enum == "etc2-rgb8unorm-srgb") {
    return WGPUTextureFormat_ETC2RGB8UnormSrgb;
  }
  if (webgpu_enum == "etc2-rgb8a1unorm") {
    return WGPUTextureFormat_ETC2RGB8A1Unorm;
  }
  if (webgpu_enum == "etc2-rgb8a1unorm-srgb") {
    return WGPUTextureFormat_ETC2RGB8A1UnormSrgb;
  }
  if (webgpu_enum == "etc2-rgba8unorm") {
    return WGPUTextureFormat_ETC2RGBA8Unorm;
  }
  if (webgpu_enum == "etc2-rgba8unorm-srgb") {
    return WGPUTextureFormat_ETC2RGBA8UnormSrgb;
  }
  if (webgpu_enum == "eac-r11unorm") {
    return WGPUTextureFormat_EACR11Unorm;
  }
  if (webgpu_enum == "eac-r11snorm") {
    return WGPUTextureFormat_EACR11Snorm;
  }
  if (webgpu_enum == "eac-rg11unorm") {
    return WGPUTextureFormat_EACRG11Unorm;
  }
  if (webgpu_enum == "eac-rg11snorm") {
    return WGPUTextureFormat_EACRG11Snorm;
  }

  // Adaptable Scalable Compression (ASTC) formats
  if (webgpu_enum == "astc-4x4-unorm") {
    return WGPUTextureFormat_ASTC4x4Unorm;
  }
  if (webgpu_enum == "astc-4x4-unorm-srgb") {
    return WGPUTextureFormat_ASTC4x4UnormSrgb;
  }
  if (webgpu_enum == "astc-5x4-unorm") {
    return WGPUTextureFormat_ASTC5x4Unorm;
  }
  if (webgpu_enum == "astc-5x4-unorm-srgb") {
    return WGPUTextureFormat_ASTC5x4UnormSrgb;
  }
  if (webgpu_enum == "astc-5x5-unorm") {
    return WGPUTextureFormat_ASTC5x5Unorm;
  }
  if (webgpu_enum == "astc-5x5-unorm-srgb") {
    return WGPUTextureFormat_ASTC5x5UnormSrgb;
  }
  if (webgpu_enum == "astc-6x5-unorm") {
    return WGPUTextureFormat_ASTC6x5Unorm;
  }
  if (webgpu_enum == "astc-6x5-unorm-srgb") {
    return WGPUTextureFormat_ASTC6x5UnormSrgb;
  }
  if (webgpu_enum == "astc-6x6-unorm") {
    return WGPUTextureFormat_ASTC6x6Unorm;
  }
  if (webgpu_enum == "astc-6x6-unorm-srgb") {
    return WGPUTextureFormat_ASTC6x6UnormSrgb;
  }
  if (webgpu_enum == "astc-8x5-unorm") {
    return WGPUTextureFormat_ASTC8x5Unorm;
  }
  if (webgpu_enum == "astc-8x5-unorm-srgb") {
    return WGPUTextureFormat_ASTC8x5UnormSrgb;
  }
  if (webgpu_enum == "astc-8x6-unorm") {
    return WGPUTextureFormat_ASTC8x6Unorm;
  }
  if (webgpu_enum == "astc-8x6-unorm-srgb") {
    return WGPUTextureFormat_ASTC8x6UnormSrgb;
  }
  if (webgpu_enum == "astc-8x8-unorm") {
    return WGPUTextureFormat_ASTC8x8Unorm;
  }
  if (webgpu_enum == "astc-8x8-unorm-srgb") {
    return WGPUTextureFormat_ASTC8x8UnormSrgb;
  }
  if (webgpu_enum == "astc-10x5-unorm") {
    return WGPUTextureFormat_ASTC10x5Unorm;
  }
  if (webgpu_enum == "astc-10x5-unorm-srgb") {
    return WGPUTextureFormat_ASTC10x5UnormSrgb;
  }
  if (webgpu_enum == "astc-10x6-unorm") {
    return WGPUTextureFormat_ASTC10x6Unorm;
  }
  if (webgpu_enum == "astc-10x6-unorm-srgb") {
    return WGPUTextureFormat_ASTC10x6UnormSrgb;
  }
  if (webgpu_enum == "astc-10x8-unorm") {
    return WGPUTextureFormat_ASTC10x8Unorm;
  }
  if (webgpu_enum == "astc-10x8-unorm-srgb") {
    return WGPUTextureFormat_ASTC10x8UnormSrgb;
  }
  if (webgpu_enum == "astc-10x10-unorm") {
    return WGPUTextureFormat_ASTC10x10Unorm;
  }
  if (webgpu_enum == "astc-10x10-unorm-srgb") {
    return WGPUTextureFormat_ASTC10x10UnormSrgb;
  }
  if (webgpu_enum == "astc-12x10-unorm") {
    return WGPUTextureFormat_ASTC12x10Unorm;
  }
  if (webgpu_enum == "astc-12x10-unorm-srgb") {
    return WGPUTextureFormat_ASTC12x10UnormSrgb;
  }
  if (webgpu_enum == "astc-12x12-unorm") {
    return WGPUTextureFormat_ASTC12x12Unorm;
  }
  if (webgpu_enum == "astc-12x12-unorm-srgb") {
    return WGPUTextureFormat_ASTC12x12UnormSrgb;
  }

  return WGPUTextureFormat_Force32;
}

template <>
WGPUTextureDimension AsDawnEnum<WGPUTextureDimension>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "1d") {
    return WGPUTextureDimension_1D;
  }
  if (webgpu_enum == "2d") {
    return WGPUTextureDimension_2D;
  }
  if (webgpu_enum == "3d") {
    return WGPUTextureDimension_3D;
  }
  NOTREACHED();
  return WGPUTextureDimension_Force32;
}

template <>
WGPUTextureViewDimension AsDawnEnum<WGPUTextureViewDimension>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum.IsNull()) {
    return WGPUTextureViewDimension_Undefined;
  }
  if (webgpu_enum == "1d") {
    return WGPUTextureViewDimension_1D;
  }
  if (webgpu_enum == "2d") {
    return WGPUTextureViewDimension_2D;
  }
  if (webgpu_enum == "2d-array") {
    return WGPUTextureViewDimension_2DArray;
  }
  if (webgpu_enum == "cube") {
    return WGPUTextureViewDimension_Cube;
  }
  if (webgpu_enum == "cube-array") {
    return WGPUTextureViewDimension_CubeArray;
  }
  if (webgpu_enum == "3d") {
    return WGPUTextureViewDimension_3D;
  }
  NOTREACHED();
  return WGPUTextureViewDimension_Force32;
}

template <>
WGPUStencilOperation AsDawnEnum<WGPUStencilOperation>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "keep") {
    return WGPUStencilOperation_Keep;
  }
  if (webgpu_enum == "zero") {
    return WGPUStencilOperation_Zero;
  }
  if (webgpu_enum == "replace") {
    return WGPUStencilOperation_Replace;
  }
  if (webgpu_enum == "invert") {
    return WGPUStencilOperation_Invert;
  }
  if (webgpu_enum == "increment-clamp") {
    return WGPUStencilOperation_IncrementClamp;
  }
  if (webgpu_enum == "decrement-clamp") {
    return WGPUStencilOperation_DecrementClamp;
  }
  if (webgpu_enum == "increment-wrap") {
    return WGPUStencilOperation_IncrementWrap;
  }
  if (webgpu_enum == "decrement-wrap") {
    return WGPUStencilOperation_DecrementWrap;
  }
  NOTREACHED();
  return WGPUStencilOperation_Force32;
}

template <>
WGPUStoreOp AsDawnEnum<WGPUStoreOp>(const WTF::String& webgpu_enum) {
  if (webgpu_enum.IsNull()) {
    return WGPUStoreOp_Undefined;
  }
  if (webgpu_enum == "store") {
    return WGPUStoreOp_Store;
  }
  if (webgpu_enum == "discard") {
    return WGPUStoreOp_Discard;
  }
  NOTREACHED();
  return WGPUStoreOp_Force32;
}

template <>
WGPULoadOp AsDawnEnum<WGPULoadOp>(const WTF::String& webgpu_enum) {
  if (webgpu_enum.IsNull()) {
    return WGPULoadOp_Undefined;
  }
  if (webgpu_enum == "load") {
    return WGPULoadOp_Load;
  }
  if (webgpu_enum == "clear") {
    return WGPULoadOp_Clear;
  }
  NOTREACHED();
  return WGPULoadOp_Force32;
}

template <>
WGPUIndexFormat AsDawnEnum<WGPUIndexFormat>(const WTF::String& webgpu_enum) {
  if (webgpu_enum.IsNull()) {
    return WGPUIndexFormat_Undefined;
  }
  if (webgpu_enum == "uint16") {
    return WGPUIndexFormat_Uint16;
  }
  if (webgpu_enum == "uint32") {
    return WGPUIndexFormat_Uint32;
  }
  NOTREACHED();
  return WGPUIndexFormat_Force32;
}

WGPUIndexFormat AsDawnEnum(const V8GPUIndexFormat& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUIndexFormat::Enum::kUint16:
      return WGPUIndexFormat_Uint16;
    case V8GPUIndexFormat::Enum::kUint32:
      return WGPUIndexFormat_Uint32;
  }
}

WGPUPredefinedColorSpace AsDawnEnum(
    const V8GPUPredefinedColorSpace& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUPredefinedColorSpace::Enum::kSRGB:
      return WGPUPredefinedColorSpace_Srgb;
  }
}

template <>
WGPUPrimitiveTopology AsDawnEnum<WGPUPrimitiveTopology>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "point-list") {
    return WGPUPrimitiveTopology_PointList;
  }
  if (webgpu_enum == "line-list") {
    return WGPUPrimitiveTopology_LineList;
  }
  if (webgpu_enum == "line-strip") {
    return WGPUPrimitiveTopology_LineStrip;
  }
  if (webgpu_enum == "triangle-list") {
    return WGPUPrimitiveTopology_TriangleList;
  }
  if (webgpu_enum == "triangle-strip") {
    return WGPUPrimitiveTopology_TriangleStrip;
  }
  NOTREACHED();
  return WGPUPrimitiveTopology_Force32;
}

template <>
WGPUBlendFactor AsDawnEnum<WGPUBlendFactor>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "zero") {
    return WGPUBlendFactor_Zero;
  }
  if (webgpu_enum == "one") {
    return WGPUBlendFactor_One;
  }
  if (webgpu_enum == "src") {
    return WGPUBlendFactor_Src;
  }
  if (webgpu_enum == "one-minus-src") {
    return WGPUBlendFactor_OneMinusSrc;
  }
  if (webgpu_enum == "src-alpha") {
    return WGPUBlendFactor_SrcAlpha;
  }
  if (webgpu_enum == "one-minus-src-alpha") {
    return WGPUBlendFactor_OneMinusSrcAlpha;
  }
  if (webgpu_enum == "dst") {
    return WGPUBlendFactor_Dst;
  }
  if (webgpu_enum == "one-minus-dst") {
    return WGPUBlendFactor_OneMinusDst;
  }
  if (webgpu_enum == "dst-alpha") {
    return WGPUBlendFactor_DstAlpha;
  }
  if (webgpu_enum == "one-minus-dst-alpha") {
    return WGPUBlendFactor_OneMinusDstAlpha;
  }
  if (webgpu_enum == "src-alpha-saturated") {
    return WGPUBlendFactor_SrcAlphaSaturated;
  }
  if (webgpu_enum == "constant") {
    return WGPUBlendFactor_Constant;
  }
  if (webgpu_enum == "one-minus-constant") {
    return WGPUBlendFactor_OneMinusConstant;
  }
  NOTREACHED();
  return WGPUBlendFactor_Force32;
}

template <>
WGPUBlendOperation AsDawnEnum<WGPUBlendOperation>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "add") {
    return WGPUBlendOperation_Add;
  }
  if (webgpu_enum == "subtract") {
    return WGPUBlendOperation_Subtract;
  }
  if (webgpu_enum == "reverse-subtract") {
    return WGPUBlendOperation_ReverseSubtract;
  }
  if (webgpu_enum == "min") {
    return WGPUBlendOperation_Min;
  }
  if (webgpu_enum == "max") {
    return WGPUBlendOperation_Max;
  }
  NOTREACHED();
  return WGPUBlendOperation_Force32;
}

template <>
WGPUVertexStepMode AsDawnEnum<WGPUVertexStepMode>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "vertex") {
    return WGPUVertexStepMode_Vertex;
  }
  if (webgpu_enum == "instance") {
    return WGPUVertexStepMode_Instance;
  }
  NOTREACHED();
  return WGPUVertexStepMode_Force32;
}

template <>
WGPUVertexFormat AsDawnEnum<WGPUVertexFormat>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "uint8x2") {
    return WGPUVertexFormat_Uint8x2;
  }
  if (webgpu_enum == "uint8x4") {
    return WGPUVertexFormat_Uint8x4;
  }
  if (webgpu_enum == "sint8x2") {
    return WGPUVertexFormat_Sint8x2;
  }
  if (webgpu_enum == "sint8x4") {
    return WGPUVertexFormat_Sint8x4;
  }
  if (webgpu_enum == "unorm8x2") {
    return WGPUVertexFormat_Unorm8x2;
  }
  if (webgpu_enum == "unorm8x4") {
    return WGPUVertexFormat_Unorm8x4;
  }
  if (webgpu_enum == "snorm8x2") {
    return WGPUVertexFormat_Snorm8x2;
  }
  if (webgpu_enum == "snorm8x4") {
    return WGPUVertexFormat_Snorm8x4;
  }
  if (webgpu_enum == "uint16x2") {
    return WGPUVertexFormat_Uint16x2;
  }
  if (webgpu_enum == "uint16x4") {
    return WGPUVertexFormat_Uint16x4;
  }
  if (webgpu_enum == "sint16x2") {
    return WGPUVertexFormat_Sint16x2;
  }
  if (webgpu_enum == "sint16x4") {
    return WGPUVertexFormat_Sint16x4;
  }
  if (webgpu_enum == "unorm16x2") {
    return WGPUVertexFormat_Unorm16x2;
  }
  if (webgpu_enum == "unorm16x4") {
    return WGPUVertexFormat_Unorm16x4;
  }
  if (webgpu_enum == "snorm16x2") {
    return WGPUVertexFormat_Snorm16x2;
  }
  if (webgpu_enum == "snorm16x4") {
    return WGPUVertexFormat_Snorm16x4;
  }
  if (webgpu_enum == "float16x2") {
    return WGPUVertexFormat_Float16x2;
  }
  if (webgpu_enum == "float16x4") {
    return WGPUVertexFormat_Float16x4;
  }
  if (webgpu_enum == "float32") {
    return WGPUVertexFormat_Float32;
  }
  if (webgpu_enum == "float32x2") {
    return WGPUVertexFormat_Float32x2;
  }
  if (webgpu_enum == "float32x3") {
    return WGPUVertexFormat_Float32x3;
  }
  if (webgpu_enum == "float32x4") {
    return WGPUVertexFormat_Float32x4;
  }
  if (webgpu_enum == "uint32") {
    return WGPUVertexFormat_Uint32;
  }
  if (webgpu_enum == "uint32x2") {
    return WGPUVertexFormat_Uint32x2;
  }
  if (webgpu_enum == "uint32x3") {
    return WGPUVertexFormat_Uint32x3;
  }
  if (webgpu_enum == "uint32x4") {
    return WGPUVertexFormat_Uint32x4;
  }
  if (webgpu_enum == "sint32") {
    return WGPUVertexFormat_Sint32;
  }
  if (webgpu_enum == "sint32x2") {
    return WGPUVertexFormat_Sint32x2;
  }
  if (webgpu_enum == "sint32x3") {
    return WGPUVertexFormat_Sint32x3;
  }
  if (webgpu_enum == "sint32x4") {
    return WGPUVertexFormat_Sint32x4;
  }
  NOTREACHED();
  return WGPUVertexFormat_Force32;
}

template <>
WGPUAddressMode AsDawnEnum<WGPUAddressMode>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "clamp-to-edge") {
    return WGPUAddressMode_ClampToEdge;
  }
  if (webgpu_enum == "repeat") {
    return WGPUAddressMode_Repeat;
  }
  if (webgpu_enum == "mirror-repeat") {
    return WGPUAddressMode_MirrorRepeat;
  }
  NOTREACHED();
  return WGPUAddressMode_Force32;
}

template <>
WGPUFilterMode AsDawnEnum<WGPUFilterMode>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "nearest") {
    return WGPUFilterMode_Nearest;
  }
  if (webgpu_enum == "linear") {
    return WGPUFilterMode_Linear;
  }
  NOTREACHED();
  return WGPUFilterMode_Force32;
}

template <>
WGPUCullMode AsDawnEnum<WGPUCullMode>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "none") {
    return WGPUCullMode_None;
  }
  if (webgpu_enum == "front") {
    return WGPUCullMode_Front;
  }
  if (webgpu_enum == "back") {
    return WGPUCullMode_Back;
  }
  NOTREACHED();
  return WGPUCullMode_Force32;
}

template <>
WGPUFrontFace AsDawnEnum<WGPUFrontFace>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "ccw") {
    return WGPUFrontFace_CCW;
  }
  if (webgpu_enum == "cw") {
    return WGPUFrontFace_CW;
  }
  NOTREACHED();
  return WGPUFrontFace_Force32;
}

template <>
WGPUTextureAspect AsDawnEnum<WGPUTextureAspect>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "all") {
    return WGPUTextureAspect_All;
  }
  if (webgpu_enum == "stencil-only") {
    return WGPUTextureAspect_StencilOnly;
  }
  if (webgpu_enum == "depth-only") {
    return WGPUTextureAspect_DepthOnly;
  }
  NOTREACHED();
  return WGPUTextureAspect_Force32;
}

template <>
WGPUErrorFilter AsDawnEnum<WGPUErrorFilter>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "out-of-memory") {
    return WGPUErrorFilter_OutOfMemory;
  }
  if (webgpu_enum == "validation") {
    return WGPUErrorFilter_Validation;
  }
  NOTREACHED();
  return WGPUErrorFilter_Force32;
}

}  // namespace blink
