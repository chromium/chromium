// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"

#include <dawn/webgpu.h>

#include "third_party/blink/renderer/bindings/modules/v8/double_sequence_or_gpu_color_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_sequence_or_gpu_extent_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_sequence_or_gpu_origin_3d_dict.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_programmable_stage_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

template <>
WGPUBindingType AsDawnEnum<WGPUBindingType>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "uniform-buffer") {
    return WGPUBindingType_UniformBuffer;
  }
  if (webgpu_enum == "storage-buffer") {
    return WGPUBindingType_StorageBuffer;
  }
  if (webgpu_enum == "sampler") {
    return WGPUBindingType_Sampler;
  }
  if (webgpu_enum == "sampled-texture") {
    return WGPUBindingType_SampledTexture;
  }
  if (webgpu_enum == "storage-texture") {
    return WGPUBindingType_StorageTexture;
  }
  NOTREACHED();
  return WGPUBindingType_Force32;
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
  if (webgpu_enum == "rgb10a2unorm") {
    return WGPUTextureFormat_RGB10A2Unorm;
  }
  if (webgpu_enum == "rg11b10float") {
    return WGPUTextureFormat_RG11B10Float;
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
  if (webgpu_enum == "depth24plus") {
    return WGPUTextureFormat_Depth24Plus;
  }
  if (webgpu_enum == "depth24plus-stencil8") {
    return WGPUTextureFormat_Depth24PlusStencil8;
  }

  return WGPUTextureFormat_Force32;
}

template <>
WGPUTextureDimension AsDawnEnum<WGPUTextureDimension>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "2d") {
    return WGPUTextureDimension_2D;
  }
  // TODO(crbug.com/dawn/129): Implement "1d" and "3d".
  NOTREACHED();
  return WGPUTextureDimension_Force32;
}

template <>
WGPUTextureViewDimension AsDawnEnum<WGPUTextureViewDimension>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum.IsNull()) {
    return WGPUTextureViewDimension_Undefined;
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
  // TODO(crbug.com/dawn/129): Implement "1d" and "3d".
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
  if (webgpu_enum == "store") {
    return WGPUStoreOp_Store;
  }
  if (webgpu_enum == "clear") {
    return WGPUStoreOp_Clear;
  }
  NOTREACHED();
  return WGPUStoreOp_Force32;
}

template <>
WGPULoadOp AsDawnEnum<WGPULoadOp>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "load") {
    return WGPULoadOp_Load;
  }
  NOTREACHED();
  return WGPULoadOp_Force32;
}

template <>
WGPUIndexFormat AsDawnEnum<WGPUIndexFormat>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "uint16") {
    return WGPUIndexFormat_Uint16;
  }
  if (webgpu_enum == "uint32") {
    return WGPUIndexFormat_Uint32;
  }
  NOTREACHED();
  return WGPUIndexFormat_Force32;
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
  if (webgpu_enum == "src-color") {
    return WGPUBlendFactor_SrcColor;
  }
  if (webgpu_enum == "one-minus-src-color") {
    return WGPUBlendFactor_OneMinusSrcColor;
  }
  if (webgpu_enum == "src-alpha") {
    return WGPUBlendFactor_SrcAlpha;
  }
  if (webgpu_enum == "one-minus-src-alpha") {
    return WGPUBlendFactor_OneMinusSrcAlpha;
  }
  if (webgpu_enum == "dst-color") {
    return WGPUBlendFactor_DstColor;
  }
  if (webgpu_enum == "one-minus-dst-color") {
    return WGPUBlendFactor_OneMinusDstColor;
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
  if (webgpu_enum == "blend-color") {
    return WGPUBlendFactor_BlendColor;
  }
  if (webgpu_enum == "one-minus-blend-color") {
    return WGPUBlendFactor_OneMinusBlendColor;
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
WGPUInputStepMode AsDawnEnum<WGPUInputStepMode>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "vertex") {
    return WGPUInputStepMode_Vertex;
  }
  if (webgpu_enum == "instance") {
    return WGPUInputStepMode_Instance;
  }
  NOTREACHED();
  return WGPUInputStepMode_Force32;
}

template <>
WGPUVertexFormat AsDawnEnum<WGPUVertexFormat>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "uchar2") {
    return WGPUVertexFormat_UChar2;
  }
  if (webgpu_enum == "uchar4") {
    return WGPUVertexFormat_UChar4;
  }
  if (webgpu_enum == "char2") {
    return WGPUVertexFormat_Char2;
  }
  if (webgpu_enum == "char4") {
    return WGPUVertexFormat_Char4;
  }
  if (webgpu_enum == "uchar2norm") {
    return WGPUVertexFormat_UChar2Norm;
  }
  if (webgpu_enum == "uchar4norm") {
    return WGPUVertexFormat_UChar4Norm;
  }
  if (webgpu_enum == "char2norm") {
    return WGPUVertexFormat_Char2Norm;
  }
  if (webgpu_enum == "char4norm") {
    return WGPUVertexFormat_Char4Norm;
  }
  if (webgpu_enum == "ushort2") {
    return WGPUVertexFormat_UShort2;
  }
  if (webgpu_enum == "ushort4") {
    return WGPUVertexFormat_UShort4;
  }
  if (webgpu_enum == "short2") {
    return WGPUVertexFormat_Short2;
  }
  if (webgpu_enum == "short4") {
    return WGPUVertexFormat_Short4;
  }
  if (webgpu_enum == "ushort2norm") {
    return WGPUVertexFormat_UShort2Norm;
  }
  if (webgpu_enum == "ushort4norm") {
    return WGPUVertexFormat_UShort4Norm;
  }
  if (webgpu_enum == "short2norm") {
    return WGPUVertexFormat_Short2Norm;
  }
  if (webgpu_enum == "short4norm") {
    return WGPUVertexFormat_Short4Norm;
  }
  if (webgpu_enum == "half2") {
    return WGPUVertexFormat_Half2;
  }
  if (webgpu_enum == "half4") {
    return WGPUVertexFormat_Half4;
  }
  if (webgpu_enum == "float") {
    return WGPUVertexFormat_Float;
  }
  if (webgpu_enum == "float2") {
    return WGPUVertexFormat_Float2;
  }
  if (webgpu_enum == "float3") {
    return WGPUVertexFormat_Float3;
  }
  if (webgpu_enum == "float4") {
    return WGPUVertexFormat_Float4;
  }
  if (webgpu_enum == "uint") {
    return WGPUVertexFormat_UInt;
  }
  if (webgpu_enum == "uint2") {
    return WGPUVertexFormat_UInt2;
  }
  if (webgpu_enum == "uint3") {
    return WGPUVertexFormat_UInt3;
  }
  if (webgpu_enum == "uint4") {
    return WGPUVertexFormat_UInt4;
  }
  if (webgpu_enum == "int") {
    return WGPUVertexFormat_Int;
  }
  if (webgpu_enum == "int2") {
    return WGPUVertexFormat_Int2;
  }
  if (webgpu_enum == "int3") {
    return WGPUVertexFormat_Int3;
  }
  if (webgpu_enum == "int4") {
    return WGPUVertexFormat_Int4;
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
  if (webgpu_enum == "none") {
    return WGPUErrorFilter_None;
  }
  if (webgpu_enum == "out-of-memory") {
    return WGPUErrorFilter_OutOfMemory;
  }
  if (webgpu_enum == "validation") {
    return WGPUErrorFilter_Validation;
  }
  NOTREACHED();
  return WGPUErrorFilter_Force32;
}

WGPUColor AsDawnColor(const Vector<double>& webgpu_color) {
  DCHECK_EQ(webgpu_color.size(), 4UL);

  WGPUColor dawn_color = {};
  dawn_color.r = webgpu_color[0];
  dawn_color.g = webgpu_color[1];
  dawn_color.b = webgpu_color[2];
  dawn_color.a = webgpu_color[3];

  return dawn_color;
}

WGPUColor AsDawnType(const GPUColorDict* webgpu_color) {
  DCHECK(webgpu_color);

  WGPUColor dawn_color = {};
  dawn_color.r = webgpu_color->r();
  dawn_color.g = webgpu_color->g();
  dawn_color.b = webgpu_color->b();
  dawn_color.a = webgpu_color->a();

  return dawn_color;
}

WGPUColor AsDawnType(const DoubleSequenceOrGPUColorDict* webgpu_color) {
  DCHECK(webgpu_color);

  if (webgpu_color->IsDoubleSequence()) {
    return AsDawnColor(webgpu_color->GetAsDoubleSequence());
  } else if (webgpu_color->IsGPUColorDict()) {
    return AsDawnType(webgpu_color->GetAsGPUColorDict());
  }
  NOTREACHED();
  WGPUColor dawn_color = {};
  return dawn_color;
}

WGPUExtent3D AsDawnType(
    const UnsignedLongSequenceOrGPUExtent3DDict* webgpu_extent) {
  DCHECK(webgpu_extent);

  WGPUExtent3D dawn_extent = {};

  if (webgpu_extent->IsUnsignedLongSequence()) {
    const Vector<uint32_t>& webgpu_extent_sequence =
        webgpu_extent->GetAsUnsignedLongSequence();
    DCHECK_EQ(webgpu_extent_sequence.size(), 3UL);
    dawn_extent.width = webgpu_extent_sequence[0];
    dawn_extent.height = webgpu_extent_sequence[1];
    dawn_extent.depth = webgpu_extent_sequence[2];

  } else if (webgpu_extent->IsGPUExtent3DDict()) {
    const GPUExtent3DDict* webgpu_extent_3d_dict =
        webgpu_extent->GetAsGPUExtent3DDict();
    dawn_extent.width = webgpu_extent_3d_dict->width();
    dawn_extent.height = webgpu_extent_3d_dict->height();
    dawn_extent.depth = webgpu_extent_3d_dict->depth();

  } else {
    NOTREACHED();
  }

  return dawn_extent;
}

WGPUOrigin3D AsDawnType(
    const UnsignedLongSequenceOrGPUOrigin3DDict* webgpu_origin) {
  DCHECK(webgpu_origin);

  WGPUOrigin3D dawn_origin = {};

  if (webgpu_origin->IsUnsignedLongSequence()) {
    const Vector<uint32_t>& webgpu_origin_sequence =
        webgpu_origin->GetAsUnsignedLongSequence();
    DCHECK_EQ(webgpu_origin_sequence.size(), 3UL);
    dawn_origin.x = webgpu_origin_sequence[0];
    dawn_origin.y = webgpu_origin_sequence[1];
    dawn_origin.z = webgpu_origin_sequence[2];

  } else if (webgpu_origin->IsGPUOrigin3DDict()) {
    const GPUOrigin3DDict* webgpu_origin_3d_dict =
        webgpu_origin->GetAsGPUOrigin3DDict();
    dawn_origin.x = webgpu_origin_3d_dict->x();
    dawn_origin.y = webgpu_origin_3d_dict->y();
    dawn_origin.z = webgpu_origin_3d_dict->z();

  } else {
    NOTREACHED();
  }

  return dawn_origin;
}

OwnedProgrammableStageDescriptor AsDawnType(
    const GPUProgrammableStageDescriptor* webgpu_stage) {
  DCHECK(webgpu_stage);

  std::string entry_point = webgpu_stage->entryPoint().Ascii();
  // length() is in bytes (not utf-8 characters or something), so this is ok.
  size_t byte_size = entry_point.length() + 1;

  std::unique_ptr<char[]> entry_point_keepalive =
      std::make_unique<char[]>(byte_size);
  char* entry_point_ptr = entry_point_keepalive.get();
  memcpy(entry_point_ptr, entry_point.c_str(), byte_size);

  WGPUProgrammableStageDescriptor dawn_stage = {};
  dawn_stage.module = webgpu_stage->module()->GetHandle();
  dawn_stage.entryPoint = entry_point_ptr;

  return std::make_tuple(dawn_stage, std::move(entry_point_keepalive));
}

}  // namespace blink
