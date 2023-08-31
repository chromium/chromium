// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_address_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_blend_factor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_blend_operation.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_buffer_binding_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compare_function.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pass_timestamp_location.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_cull_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_error_filter.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_feature_name.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_filter_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_front_face.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_index_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_load_op.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_mipmap_filter_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_pipeline_statistic_name.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_primitive_topology.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_query_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_timestamp_location.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_sampler_binding_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_stencil_operation.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_storage_texture_access.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_store_op.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_aspect.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_dimension.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_sample_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_view_dimension.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_vertex_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_vertex_step_mode.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"

namespace blink {

WGPUBufferBindingType AsDawnEnum(const V8GPUBufferBindingType& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUBufferBindingType::Enum::kUniform:
      return WGPUBufferBindingType_Uniform;
    case V8GPUBufferBindingType::Enum::kStorage:
      return WGPUBufferBindingType_Storage;
    case V8GPUBufferBindingType::Enum::kReadOnlyStorage:
      return WGPUBufferBindingType_ReadOnlyStorage;
  }
}

WGPUSamplerBindingType AsDawnEnum(const V8GPUSamplerBindingType& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUSamplerBindingType::Enum::kFiltering:
      return WGPUSamplerBindingType_Filtering;
    case V8GPUSamplerBindingType::Enum::kNonFiltering:
      return WGPUSamplerBindingType_NonFiltering;
    case V8GPUSamplerBindingType::Enum::kComparison:
      return WGPUSamplerBindingType_Comparison;
  }
}

WGPUTextureSampleType AsDawnEnum(const V8GPUTextureSampleType& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUTextureSampleType::Enum::kFloat:
      return WGPUTextureSampleType_Float;
    case V8GPUTextureSampleType::Enum::kUnfilterableFloat:
      return WGPUTextureSampleType_UnfilterableFloat;
    case V8GPUTextureSampleType::Enum::kDepth:
      return WGPUTextureSampleType_Depth;
    case V8GPUTextureSampleType::Enum::kSint:
      return WGPUTextureSampleType_Sint;
    case V8GPUTextureSampleType::Enum::kUint:
      return WGPUTextureSampleType_Uint;
  }
}

WGPUStorageTextureAccess AsDawnEnum(
    const V8GPUStorageTextureAccess& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUStorageTextureAccess::Enum::kWriteOnly:
      return WGPUStorageTextureAccess_WriteOnly;
    case V8GPUStorageTextureAccess::Enum::kReadOnly:
      return WGPUStorageTextureAccess_ReadOnly;
    case V8GPUStorageTextureAccess::Enum::kReadWrite:
      return WGPUStorageTextureAccess_ReadWrite;
  }
}

WGPUCompareFunction AsDawnEnum(const V8GPUCompareFunction& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUCompareFunction::Enum::kNever:
      return WGPUCompareFunction_Never;
    case V8GPUCompareFunction::Enum::kLess:
      return WGPUCompareFunction_Less;
    case V8GPUCompareFunction::Enum::kEqual:
      return WGPUCompareFunction_Equal;
    case V8GPUCompareFunction::Enum::kLessEqual:
      return WGPUCompareFunction_LessEqual;
    case V8GPUCompareFunction::Enum::kGreater:
      return WGPUCompareFunction_Greater;
    case V8GPUCompareFunction::Enum::kNotEqual:
      return WGPUCompareFunction_NotEqual;
    case V8GPUCompareFunction::Enum::kGreaterEqual:
      return WGPUCompareFunction_GreaterEqual;
    case V8GPUCompareFunction::Enum::kAlways:
      return WGPUCompareFunction_Always;
  }
}

WGPUQueryType AsDawnEnum(const V8GPUQueryType& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUQueryType::Enum::kOcclusion:
      return WGPUQueryType_Occlusion;
    case V8GPUQueryType::Enum::kPipelineStatistics:
      return WGPUQueryType_PipelineStatistics;
    case V8GPUQueryType::Enum::kTimestamp:
      return WGPUQueryType_Timestamp;
  }
}

const char* FromDawnEnum(WGPUQueryType dawn_enum) {
  switch (dawn_enum) {
    case WGPUQueryType_Occlusion:
      return "occlusion";
    case WGPUQueryType_Timestamp:
      return "timestamp";
    case WGPUQueryType_PipelineStatistics:
      return "pipeline-statistics";
    case WGPUQueryType_Force32:
      NOTREACHED();
  }
  return "";
}

WGPUPipelineStatisticName AsDawnEnum(
    const V8GPUPipelineStatisticName& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUPipelineStatisticName::Enum::kVertexShaderInvocations:
      return WGPUPipelineStatisticName_VertexShaderInvocations;
    case V8GPUPipelineStatisticName::Enum::kClipperInvocations:
      return WGPUPipelineStatisticName_ClipperInvocations;
    case V8GPUPipelineStatisticName::Enum::kClipperPrimitivesOut:
      return WGPUPipelineStatisticName_ClipperPrimitivesOut;
    case V8GPUPipelineStatisticName::Enum::kFragmentShaderInvocations:
      return WGPUPipelineStatisticName_FragmentShaderInvocations;
    case V8GPUPipelineStatisticName::Enum::kComputeShaderInvocations:
      return WGPUPipelineStatisticName_ComputeShaderInvocations;
  }
}

WGPUTextureFormat AsDawnEnum(const V8GPUTextureFormat& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
      // Normal 8 bit formats
    case V8GPUTextureFormat::Enum::kR8Unorm:
      return WGPUTextureFormat_R8Unorm;
    case V8GPUTextureFormat::Enum::kR8Snorm:
      return WGPUTextureFormat_R8Snorm;
    case V8GPUTextureFormat::Enum::kR8Uint:
      return WGPUTextureFormat_R8Uint;
    case V8GPUTextureFormat::Enum::kR8Sint:
      return WGPUTextureFormat_R8Sint;

      // Normal 16 bit formats
    case V8GPUTextureFormat::Enum::kR16Uint:
      return WGPUTextureFormat_R16Uint;
    case V8GPUTextureFormat::Enum::kR16Sint:
      return WGPUTextureFormat_R16Sint;
    case V8GPUTextureFormat::Enum::kR16Float:
      return WGPUTextureFormat_R16Float;
    case V8GPUTextureFormat::Enum::kRg8Unorm:
      return WGPUTextureFormat_RG8Unorm;
    case V8GPUTextureFormat::Enum::kRg8Snorm:
      return WGPUTextureFormat_RG8Snorm;
    case V8GPUTextureFormat::Enum::kRg8Uint:
      return WGPUTextureFormat_RG8Uint;
    case V8GPUTextureFormat::Enum::kRg8Sint:
      return WGPUTextureFormat_RG8Sint;

      // Normal 32 bit formats
    case V8GPUTextureFormat::Enum::kR32Uint:
      return WGPUTextureFormat_R32Uint;
    case V8GPUTextureFormat::Enum::kR32Sint:
      return WGPUTextureFormat_R32Sint;
    case V8GPUTextureFormat::Enum::kR32Float:
      return WGPUTextureFormat_R32Float;
    case V8GPUTextureFormat::Enum::kRg16Uint:
      return WGPUTextureFormat_RG16Uint;
    case V8GPUTextureFormat::Enum::kRg16Sint:
      return WGPUTextureFormat_RG16Sint;
    case V8GPUTextureFormat::Enum::kRg16Float:
      return WGPUTextureFormat_RG16Float;
    case V8GPUTextureFormat::Enum::kRgba8Unorm:
      return WGPUTextureFormat_RGBA8Unorm;
    case V8GPUTextureFormat::Enum::kRgba8UnormSrgb:
      return WGPUTextureFormat_RGBA8UnormSrgb;
    case V8GPUTextureFormat::Enum::kRgba8Snorm:
      return WGPUTextureFormat_RGBA8Snorm;
    case V8GPUTextureFormat::Enum::kRgba8Uint:
      return WGPUTextureFormat_RGBA8Uint;
    case V8GPUTextureFormat::Enum::kRgba8Sint:
      return WGPUTextureFormat_RGBA8Sint;
    case V8GPUTextureFormat::Enum::kBgra8Unorm:
      return WGPUTextureFormat_BGRA8Unorm;
    case V8GPUTextureFormat::Enum::kBgra8UnormSrgb:
      return WGPUTextureFormat_BGRA8UnormSrgb;

      // Packed 32 bit formats
    case V8GPUTextureFormat::Enum::kRgb9E5Ufloat:
      return WGPUTextureFormat_RGB9E5Ufloat;
    case V8GPUTextureFormat::Enum::kRgb10A2Unorm:
      return WGPUTextureFormat_RGB10A2Unorm;
    case V8GPUTextureFormat::Enum::kRg11B10Ufloat:
      return WGPUTextureFormat_RG11B10Ufloat;

      // Normal 64 bit formats
    case V8GPUTextureFormat::Enum::kRg32Uint:
      return WGPUTextureFormat_RG32Uint;
    case V8GPUTextureFormat::Enum::kRg32Sint:
      return WGPUTextureFormat_RG32Sint;
    case V8GPUTextureFormat::Enum::kRg32Float:
      return WGPUTextureFormat_RG32Float;
    case V8GPUTextureFormat::Enum::kRgba16Uint:
      return WGPUTextureFormat_RGBA16Uint;
    case V8GPUTextureFormat::Enum::kRgba16Sint:
      return WGPUTextureFormat_RGBA16Sint;
    case V8GPUTextureFormat::Enum::kRgba16Float:
      return WGPUTextureFormat_RGBA16Float;

      // Normal 128 bit formats
    case V8GPUTextureFormat::Enum::kRgba32Uint:
      return WGPUTextureFormat_RGBA32Uint;
    case V8GPUTextureFormat::Enum::kRgba32Sint:
      return WGPUTextureFormat_RGBA32Sint;
    case V8GPUTextureFormat::Enum::kRgba32Float:
      return WGPUTextureFormat_RGBA32Float;

      // Depth / Stencil formats
    case V8GPUTextureFormat::Enum::kDepth32Float:
      return WGPUTextureFormat_Depth32Float;
    case V8GPUTextureFormat::Enum::kDepth32FloatStencil8:
      return WGPUTextureFormat_Depth32FloatStencil8;
    case V8GPUTextureFormat::Enum::kDepth24Plus:
      return WGPUTextureFormat_Depth24Plus;
    case V8GPUTextureFormat::Enum::kDepth24PlusStencil8:
      return WGPUTextureFormat_Depth24PlusStencil8;
    case V8GPUTextureFormat::Enum::kDepth16Unorm:
      return WGPUTextureFormat_Depth16Unorm;
    case V8GPUTextureFormat::Enum::kStencil8:
      return WGPUTextureFormat_Stencil8;

      // Block Compression (BC) formats
    case V8GPUTextureFormat::Enum::kBc1RgbaUnorm:
      return WGPUTextureFormat_BC1RGBAUnorm;
    case V8GPUTextureFormat::Enum::kBc1RgbaUnormSrgb:
      return WGPUTextureFormat_BC1RGBAUnormSrgb;
    case V8GPUTextureFormat::Enum::kBc2RgbaUnorm:
      return WGPUTextureFormat_BC2RGBAUnorm;
    case V8GPUTextureFormat::Enum::kBc2RgbaUnormSrgb:
      return WGPUTextureFormat_BC2RGBAUnormSrgb;
    case V8GPUTextureFormat::Enum::kBc3RgbaUnorm:
      return WGPUTextureFormat_BC3RGBAUnorm;
    case V8GPUTextureFormat::Enum::kBc3RgbaUnormSrgb:
      return WGPUTextureFormat_BC3RGBAUnormSrgb;
    case V8GPUTextureFormat::Enum::kBc4RUnorm:
      return WGPUTextureFormat_BC4RUnorm;
    case V8GPUTextureFormat::Enum::kBc4RSnorm:
      return WGPUTextureFormat_BC4RSnorm;
    case V8GPUTextureFormat::Enum::kBc5RgUnorm:
      return WGPUTextureFormat_BC5RGUnorm;
    case V8GPUTextureFormat::Enum::kBc5RgSnorm:
      return WGPUTextureFormat_BC5RGSnorm;
    case V8GPUTextureFormat::Enum::kBc6HRgbUfloat:
      return WGPUTextureFormat_BC6HRGBUfloat;
    case V8GPUTextureFormat::Enum::kBc6HRgbFloat:
      return WGPUTextureFormat_BC6HRGBFloat;
    case V8GPUTextureFormat::Enum::kBc7RgbaUnorm:
      return WGPUTextureFormat_BC7RGBAUnorm;
    case V8GPUTextureFormat::Enum::kBc7RgbaUnormSrgb:
      return WGPUTextureFormat_BC7RGBAUnormSrgb;

      // Ericsson Compression (ETC2) formats
    case V8GPUTextureFormat::Enum::kEtc2Rgb8Unorm:
      return WGPUTextureFormat_ETC2RGB8Unorm;
    case V8GPUTextureFormat::Enum::kEtc2Rgb8UnormSrgb:
      return WGPUTextureFormat_ETC2RGB8UnormSrgb;
    case V8GPUTextureFormat::Enum::kEtc2Rgb8A1Unorm:
      return WGPUTextureFormat_ETC2RGB8A1Unorm;
    case V8GPUTextureFormat::Enum::kEtc2Rgb8A1UnormSrgb:
      return WGPUTextureFormat_ETC2RGB8A1UnormSrgb;
    case V8GPUTextureFormat::Enum::kEtc2Rgba8Unorm:
      return WGPUTextureFormat_ETC2RGBA8Unorm;
    case V8GPUTextureFormat::Enum::kEtc2Rgba8UnormSrgb:
      return WGPUTextureFormat_ETC2RGBA8UnormSrgb;
    case V8GPUTextureFormat::Enum::kEacR11Unorm:
      return WGPUTextureFormat_EACR11Unorm;
    case V8GPUTextureFormat::Enum::kEacR11Snorm:
      return WGPUTextureFormat_EACR11Snorm;
    case V8GPUTextureFormat::Enum::kEacRg11Unorm:
      return WGPUTextureFormat_EACRG11Unorm;
    case V8GPUTextureFormat::Enum::kEacRg11Snorm:
      return WGPUTextureFormat_EACRG11Snorm;

      // Adaptable Scalable Compression (ASTC) formats
    case V8GPUTextureFormat::Enum::kAstc4X4Unorm:
      return WGPUTextureFormat_ASTC4x4Unorm;
    case V8GPUTextureFormat::Enum::kAstc4X4UnormSrgb:
      return WGPUTextureFormat_ASTC4x4UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc5X4Unorm:
      return WGPUTextureFormat_ASTC5x4Unorm;
    case V8GPUTextureFormat::Enum::kAstc5X4UnormSrgb:
      return WGPUTextureFormat_ASTC5x4UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc5X5Unorm:
      return WGPUTextureFormat_ASTC5x5Unorm;
    case V8GPUTextureFormat::Enum::kAstc5X5UnormSrgb:
      return WGPUTextureFormat_ASTC5x5UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc6X5Unorm:
      return WGPUTextureFormat_ASTC6x5Unorm;
    case V8GPUTextureFormat::Enum::kAstc6X5UnormSrgb:
      return WGPUTextureFormat_ASTC6x5UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc6X6Unorm:
      return WGPUTextureFormat_ASTC6x6Unorm;
    case V8GPUTextureFormat::Enum::kAstc6X6UnormSrgb:
      return WGPUTextureFormat_ASTC6x6UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc8X5Unorm:
      return WGPUTextureFormat_ASTC8x5Unorm;
    case V8GPUTextureFormat::Enum::kAstc8X5UnormSrgb:
      return WGPUTextureFormat_ASTC8x5UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc8X6Unorm:
      return WGPUTextureFormat_ASTC8x6Unorm;
    case V8GPUTextureFormat::Enum::kAstc8X6UnormSrgb:
      return WGPUTextureFormat_ASTC8x6UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc8X8Unorm:
      return WGPUTextureFormat_ASTC8x8Unorm;
    case V8GPUTextureFormat::Enum::kAstc8X8UnormSrgb:
      return WGPUTextureFormat_ASTC8x8UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc10X5Unorm:
      return WGPUTextureFormat_ASTC10x5Unorm;
    case V8GPUTextureFormat::Enum::kAstc10X5UnormSrgb:
      return WGPUTextureFormat_ASTC10x5UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc10X6Unorm:
      return WGPUTextureFormat_ASTC10x6Unorm;
    case V8GPUTextureFormat::Enum::kAstc10X6UnormSrgb:
      return WGPUTextureFormat_ASTC10x6UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc10X8Unorm:
      return WGPUTextureFormat_ASTC10x8Unorm;
    case V8GPUTextureFormat::Enum::kAstc10X8UnormSrgb:
      return WGPUTextureFormat_ASTC10x8UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc10X10Unorm:
      return WGPUTextureFormat_ASTC10x10Unorm;
    case V8GPUTextureFormat::Enum::kAstc10X10UnormSrgb:
      return WGPUTextureFormat_ASTC10x10UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc12X10Unorm:
      return WGPUTextureFormat_ASTC12x10Unorm;
    case V8GPUTextureFormat::Enum::kAstc12X10UnormSrgb:
      return WGPUTextureFormat_ASTC12x10UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc12X12Unorm:
      return WGPUTextureFormat_ASTC12x12Unorm;
    case V8GPUTextureFormat::Enum::kAstc12X12UnormSrgb:
      return WGPUTextureFormat_ASTC12x12UnormSrgb;
  }
}

const char* FromDawnEnum(WGPUTextureFormat dawn_enum) {
  switch (dawn_enum) {
    // Normal 8 bit formats
    case WGPUTextureFormat_R8Unorm:
      return "r8unorm";
    case WGPUTextureFormat_R8Snorm:
      return "r8snorm";
    case WGPUTextureFormat_R8Uint:
      return "r8uint";
    case WGPUTextureFormat_R8Sint:
      return "r8sint";

    // Normal 16 bit formats
    case WGPUTextureFormat_R16Uint:
      return "r16uint";
    case WGPUTextureFormat_R16Sint:
      return "r16sint";
    case WGPUTextureFormat_R16Float:
      return "r16float";
    case WGPUTextureFormat_RG8Unorm:
      return "rg8unorm";
    case WGPUTextureFormat_RG8Snorm:
      return "rg8snorm";
    case WGPUTextureFormat_RG8Uint:
      return "rg8uint";
    case WGPUTextureFormat_RG8Sint:
      return "rg8sint";

    // Normal 32 bit formats
    case WGPUTextureFormat_R32Uint:
      return "r32uint";
    case WGPUTextureFormat_R32Sint:
      return "r32sint";
    case WGPUTextureFormat_R32Float:
      return "r32float";
    case WGPUTextureFormat_RG16Uint:
      return "rg16uint";
    case WGPUTextureFormat_RG16Sint:
      return "rg16sint";
    case WGPUTextureFormat_RG16Float:
      return "rg16float";
    case WGPUTextureFormat_RGBA8Unorm:
      return "rgba8unorm";
    case WGPUTextureFormat_RGBA8UnormSrgb:
      return "rgba8unorm-srgb";
    case WGPUTextureFormat_RGBA8Snorm:
      return "rgba8snorm";
    case WGPUTextureFormat_RGBA8Uint:
      return "rgba8uint";
    case WGPUTextureFormat_RGBA8Sint:
      return "rgba8sint";
    case WGPUTextureFormat_BGRA8Unorm:
      return "bgra8unorm";
    case WGPUTextureFormat_BGRA8UnormSrgb:
      return "bgra8unorm-srgb";

    // Packed 32 bit formats
    case WGPUTextureFormat_RGB9E5Ufloat:
      return "rgb9e5ufloat";
    case WGPUTextureFormat_RGB10A2Unorm:
      return "rgb10a2unorm";
    case WGPUTextureFormat_RG11B10Ufloat:
      return "rg11b10ufloat";

    // Normal 64 bit formats
    case WGPUTextureFormat_RG32Uint:
      return "rg32uint";
    case WGPUTextureFormat_RG32Sint:
      return "rg32sint";
    case WGPUTextureFormat_RG32Float:
      return "rg32float";
    case WGPUTextureFormat_RGBA16Uint:
      return "rgba16uint";
    case WGPUTextureFormat_RGBA16Sint:
      return "rgba16sint";
    case WGPUTextureFormat_RGBA16Float:
      return "rgba16float";

    // Normal 128 bit formats
    case WGPUTextureFormat_RGBA32Uint:
      return "rgba32uint";
    case WGPUTextureFormat_RGBA32Sint:
      return "rgba32sint";
    case WGPUTextureFormat_RGBA32Float:
      return "rgba32float";

    // Depth / Stencil formats
    case WGPUTextureFormat_Depth32Float:
      return "depth32float";
    case WGPUTextureFormat_Depth32FloatStencil8:
      return "depth32float-stencil8";
    case WGPUTextureFormat_Depth24Plus:
      return "depth24plus";
    case WGPUTextureFormat_Depth24PlusStencil8:
      return "depth24plus-stencil8";
    case WGPUTextureFormat_Depth16Unorm:
      return "depth16unorm";
    case WGPUTextureFormat_Stencil8:
      return "stencil8";

    // Block Compression (BC) formats
    case WGPUTextureFormat_BC1RGBAUnorm:
      return "bc1-rgba-unorm";
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
      return "bc1-rgba-unorm-srgb";
    case WGPUTextureFormat_BC2RGBAUnorm:
      return "bc2-rgba-unorm";
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
      return "bc2-rgba-unorm-srgb";
    case WGPUTextureFormat_BC3RGBAUnorm:
      return "bc3-rgba-unorm";
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
      return "bc3-rgba-unorm-srgb";
    case WGPUTextureFormat_BC4RUnorm:
      return "bc4-r-unorm";
    case WGPUTextureFormat_BC4RSnorm:
      return "bc4-r-snorm";
    case WGPUTextureFormat_BC5RGUnorm:
      return "bc5-rg-unorm";
    case WGPUTextureFormat_BC5RGSnorm:
      return "bc5-rg-snorm";
    case WGPUTextureFormat_BC6HRGBUfloat:
      return "bc6h-rgb-ufloat";
    case WGPUTextureFormat_BC6HRGBFloat:
      return "bc6h-rgb-float";
    case WGPUTextureFormat_BC7RGBAUnorm:
      return "bc7-rgba-unorm";
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
      return "bc7-rgba-unorm-srgb";

    // Ericsson Compression (ETC2) formats
    case WGPUTextureFormat_ETC2RGB8Unorm:
      return "etc2-rgb8unorm";
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
      return "etc2-rgb8unorm-srgb";
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
      return "etc2-rgb8a1unorm";
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
      return "etc2-rgb8a1unorm-srgb";
    case WGPUTextureFormat_ETC2RGBA8Unorm:
      return "etc2-rgba8unorm";
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
      return "etc2-rgba8unorm-srgb";
    case WGPUTextureFormat_EACR11Unorm:
      return "eac-r11unorm";
    case WGPUTextureFormat_EACR11Snorm:
      return "eac-r11snorm";
    case WGPUTextureFormat_EACRG11Unorm:
      return "eac-rg11unorm";
    case WGPUTextureFormat_EACRG11Snorm:
      return "eac-rg11snorm";

    // Adaptable Scalable Compression (ASTC) formats
    case WGPUTextureFormat_ASTC4x4Unorm:
      return "astc-4x4-unorm";
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
      return "astc-4x4-unorm-srgb";
    case WGPUTextureFormat_ASTC5x4Unorm:
      return "astc-5x4-unorm";
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
      return "astc-5x4-unorm-srgb";
    case WGPUTextureFormat_ASTC5x5Unorm:
      return "astc-5x5-unorm";
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
      return "astc-5x5-unorm-srgb";
    case WGPUTextureFormat_ASTC6x5Unorm:
      return "astc-6x5-unorm";
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
      return "astc-6x5-unorm-srgb";
    case WGPUTextureFormat_ASTC6x6Unorm:
      return "astc-6x6-unorm";
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
      return "astc-6x6-unorm-srgb";
    case WGPUTextureFormat_ASTC8x5Unorm:
      return "astc-8x5-unorm";
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
      return "astc-8x5-unorm-srgb";
    case WGPUTextureFormat_ASTC8x6Unorm:
      return "astc-8x6-unorm";
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
      return "astc-8x6-unorm-srgb";
    case WGPUTextureFormat_ASTC8x8Unorm:
      return "astc-8x8-unorm";
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
      return "astc-8x8-unorm-srgb";
    case WGPUTextureFormat_ASTC10x5Unorm:
      return "astc-10x5-unorm";
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
      return "astc-10x5-unorm-srgb";
    case WGPUTextureFormat_ASTC10x6Unorm:
      return "astc-10x6-unorm";
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
      return "astc-10x6-unorm-srgb";
    case WGPUTextureFormat_ASTC10x8Unorm:
      return "astc-10x8-unorm";
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
      return "astc-10x8-unorm-srgb";
    case WGPUTextureFormat_ASTC10x10Unorm:
      return "astc-10x10-unorm";
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
      return "astc-10x10-unorm-srgb";
    case WGPUTextureFormat_ASTC12x10Unorm:
      return "astc-12x10-unorm";
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
      return "astc-12x10-unorm-srgb";
    case WGPUTextureFormat_ASTC12x12Unorm:
      return "astc-12x12-unorm";
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
      return "astc-12x12-unorm-srgb";

    default:
      NOTREACHED();
  }
  return "";
}

WGPUTextureDimension AsDawnEnum(const V8GPUTextureDimension& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUTextureDimension::Enum::k1d:
      return WGPUTextureDimension_1D;
    case V8GPUTextureDimension::Enum::k2D:
      return WGPUTextureDimension_2D;
    case V8GPUTextureDimension::Enum::k3d:
      return WGPUTextureDimension_3D;
  }
}

const char* FromDawnEnum(WGPUTextureDimension dawn_enum) {
  switch (dawn_enum) {
    case WGPUTextureDimension_1D:
      return "1d";
    case WGPUTextureDimension_2D:
      return "2d";
    case WGPUTextureDimension_3D:
      return "3d";
    case WGPUTextureDimension_Force32:
      NOTREACHED();
  }
  return "";
}

WGPUTextureViewDimension AsDawnEnum(
    const V8GPUTextureViewDimension& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUTextureViewDimension::Enum::k1d:
      return WGPUTextureViewDimension_1D;
    case V8GPUTextureViewDimension::Enum::k2D:
      return WGPUTextureViewDimension_2D;
    case V8GPUTextureViewDimension::Enum::k2DArray:
      return WGPUTextureViewDimension_2DArray;
    case V8GPUTextureViewDimension::Enum::kCube:
      return WGPUTextureViewDimension_Cube;
    case V8GPUTextureViewDimension::Enum::kCubeArray:
      return WGPUTextureViewDimension_CubeArray;
    case V8GPUTextureViewDimension::Enum::k3d:
      return WGPUTextureViewDimension_3D;
  }
}

WGPUStencilOperation AsDawnEnum(const V8GPUStencilOperation& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUStencilOperation::Enum::kKeep:
      return WGPUStencilOperation_Keep;
    case V8GPUStencilOperation::Enum::kZero:
      return WGPUStencilOperation_Zero;
    case V8GPUStencilOperation::Enum::kReplace:
      return WGPUStencilOperation_Replace;
    case V8GPUStencilOperation::Enum::kInvert:
      return WGPUStencilOperation_Invert;
    case V8GPUStencilOperation::Enum::kIncrementClamp:
      return WGPUStencilOperation_IncrementClamp;
    case V8GPUStencilOperation::Enum::kDecrementClamp:
      return WGPUStencilOperation_DecrementClamp;
    case V8GPUStencilOperation::Enum::kIncrementWrap:
      return WGPUStencilOperation_IncrementWrap;
    case V8GPUStencilOperation::Enum::kDecrementWrap:
      return WGPUStencilOperation_DecrementWrap;
  }
}

WGPUStoreOp AsDawnEnum(const V8GPUStoreOp& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUStoreOp::Enum::kStore:
      return WGPUStoreOp_Store;
    case V8GPUStoreOp::Enum::kDiscard:
      return WGPUStoreOp_Discard;
  }
}

WGPULoadOp AsDawnEnum(const V8GPULoadOp& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPULoadOp::Enum::kLoad:
      return WGPULoadOp_Load;
    case V8GPULoadOp::Enum::kClear:
      return WGPULoadOp_Clear;
  }
}

WGPUIndexFormat AsDawnEnum(const V8GPUIndexFormat& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUIndexFormat::Enum::kUint16:
      return WGPUIndexFormat_Uint16;
    case V8GPUIndexFormat::Enum::kUint32:
      return WGPUIndexFormat_Uint32;
  }
}

WGPUFeatureName AsDawnEnum(const V8GPUFeatureName& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUFeatureName::Enum::kPipelineStatisticsQuery:
      return WGPUFeatureName_PipelineStatisticsQuery;
    case V8GPUFeatureName::Enum::kTextureCompressionBc:
      return WGPUFeatureName_TextureCompressionBC;
    case V8GPUFeatureName::Enum::kTextureCompressionEtc2:
      return WGPUFeatureName_TextureCompressionETC2;
    case V8GPUFeatureName::Enum::kTextureCompressionAstc:
      return WGPUFeatureName_TextureCompressionASTC;
    case V8GPUFeatureName::Enum::kTimestampQuery:
      return WGPUFeatureName_TimestampQuery;
    case V8GPUFeatureName::Enum::kTimestampQueryInsidePasses:
      return WGPUFeatureName_TimestampQueryInsidePasses;
    case V8GPUFeatureName::Enum::kDepthClipControl:
      return WGPUFeatureName_DepthClipControl;
    case V8GPUFeatureName::Enum::kDepth32FloatStencil8:
      return WGPUFeatureName_Depth32FloatStencil8;
    case V8GPUFeatureName::Enum::kIndirectFirstInstance:
      return WGPUFeatureName_IndirectFirstInstance;
    case V8GPUFeatureName::Enum::kChromiumExperimentalDp4A:
      return WGPUFeatureName_ChromiumExperimentalDp4a;
    case V8GPUFeatureName::Enum::kChromiumExperimentalReadWriteStorageTexture:
      return WGPUFeatureName_ChromiumExperimentalReadWriteStorageTexture;
    case V8GPUFeatureName::Enum::kChromiumExperimentalSubgroups:
      return WGPUFeatureName_ChromiumExperimentalSubgroups;
    case V8GPUFeatureName::Enum::
        kChromiumExperimentalSubgroupUniformControlFlow:
      return WGPUFeatureName_ChromiumExperimentalSubgroupUniformControlFlow;
    case V8GPUFeatureName::Enum::kRg11B10UfloatRenderable:
      return WGPUFeatureName_RG11B10UfloatRenderable;
    case V8GPUFeatureName::Enum::kBgra8UnormStorage:
      return WGPUFeatureName_BGRA8UnormStorage;
    case V8GPUFeatureName::Enum::kShaderF16:
      return WGPUFeatureName_ShaderF16;
    case V8GPUFeatureName::Enum::kFloat32Filterable:
      return WGPUFeatureName_Float32Filterable;
  }
}

WGPUPrimitiveTopology AsDawnEnum(const V8GPUPrimitiveTopology& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUPrimitiveTopology::Enum::kPointList:
      return WGPUPrimitiveTopology_PointList;
    case V8GPUPrimitiveTopology::Enum::kLineList:
      return WGPUPrimitiveTopology_LineList;
    case V8GPUPrimitiveTopology::Enum::kLineStrip:
      return WGPUPrimitiveTopology_LineStrip;
    case V8GPUPrimitiveTopology::Enum::kTriangleList:
      return WGPUPrimitiveTopology_TriangleList;
    case V8GPUPrimitiveTopology::Enum::kTriangleStrip:
      return WGPUPrimitiveTopology_TriangleStrip;
  }
}

WGPUBlendFactor AsDawnEnum(const V8GPUBlendFactor& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUBlendFactor::Enum::kZero:
      return WGPUBlendFactor_Zero;
    case V8GPUBlendFactor::Enum::kOne:
      return WGPUBlendFactor_One;
    case V8GPUBlendFactor::Enum::kSrc:
      return WGPUBlendFactor_Src;
    case V8GPUBlendFactor::Enum::kOneMinusSrc:
      return WGPUBlendFactor_OneMinusSrc;
    case V8GPUBlendFactor::Enum::kSrcAlpha:
      return WGPUBlendFactor_SrcAlpha;
    case V8GPUBlendFactor::Enum::kOneMinusSrcAlpha:
      return WGPUBlendFactor_OneMinusSrcAlpha;
    case V8GPUBlendFactor::Enum::kDst:
      return WGPUBlendFactor_Dst;
    case V8GPUBlendFactor::Enum::kOneMinusDst:
      return WGPUBlendFactor_OneMinusDst;
    case V8GPUBlendFactor::Enum::kDstAlpha:
      return WGPUBlendFactor_DstAlpha;
    case V8GPUBlendFactor::Enum::kOneMinusDstAlpha:
      return WGPUBlendFactor_OneMinusDstAlpha;
    case V8GPUBlendFactor::Enum::kSrcAlphaSaturated:
      return WGPUBlendFactor_SrcAlphaSaturated;
    case V8GPUBlendFactor::Enum::kConstant:
      return WGPUBlendFactor_Constant;
    case V8GPUBlendFactor::Enum::kOneMinusConstant:
      return WGPUBlendFactor_OneMinusConstant;
  }
}

WGPUBlendOperation AsDawnEnum(const V8GPUBlendOperation& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUBlendOperation::Enum::kAdd:
      return WGPUBlendOperation_Add;
    case V8GPUBlendOperation::Enum::kSubtract:
      return WGPUBlendOperation_Subtract;
    case V8GPUBlendOperation::Enum::kReverseSubtract:
      return WGPUBlendOperation_ReverseSubtract;
    case V8GPUBlendOperation::Enum::kMin:
      return WGPUBlendOperation_Min;
    case V8GPUBlendOperation::Enum::kMax:
      return WGPUBlendOperation_Max;
  }
}

WGPUVertexStepMode AsDawnEnum(const V8GPUVertexStepMode& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUVertexStepMode::Enum::kVertex:
      return WGPUVertexStepMode_Vertex;
    case V8GPUVertexStepMode::Enum::kInstance:
      return WGPUVertexStepMode_Instance;
  }
}

WGPUVertexFormat AsDawnEnum(const V8GPUVertexFormat& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUVertexFormat::Enum::kUint8X2:
      return WGPUVertexFormat_Uint8x2;
    case V8GPUVertexFormat::Enum::kUint8X4:
      return WGPUVertexFormat_Uint8x4;
    case V8GPUVertexFormat::Enum::kSint8X2:
      return WGPUVertexFormat_Sint8x2;
    case V8GPUVertexFormat::Enum::kSint8X4:
      return WGPUVertexFormat_Sint8x4;
    case V8GPUVertexFormat::Enum::kUnorm8X2:
      return WGPUVertexFormat_Unorm8x2;
    case V8GPUVertexFormat::Enum::kUnorm8X4:
      return WGPUVertexFormat_Unorm8x4;
    case V8GPUVertexFormat::Enum::kSnorm8X2:
      return WGPUVertexFormat_Snorm8x2;
    case V8GPUVertexFormat::Enum::kSnorm8X4:
      return WGPUVertexFormat_Snorm8x4;
    case V8GPUVertexFormat::Enum::kUint16X2:
      return WGPUVertexFormat_Uint16x2;
    case V8GPUVertexFormat::Enum::kUint16X4:
      return WGPUVertexFormat_Uint16x4;
    case V8GPUVertexFormat::Enum::kSint16X2:
      return WGPUVertexFormat_Sint16x2;
    case V8GPUVertexFormat::Enum::kSint16X4:
      return WGPUVertexFormat_Sint16x4;
    case V8GPUVertexFormat::Enum::kUnorm16X2:
      return WGPUVertexFormat_Unorm16x2;
    case V8GPUVertexFormat::Enum::kUnorm16X4:
      return WGPUVertexFormat_Unorm16x4;
    case V8GPUVertexFormat::Enum::kSnorm16X2:
      return WGPUVertexFormat_Snorm16x2;
    case V8GPUVertexFormat::Enum::kSnorm16X4:
      return WGPUVertexFormat_Snorm16x4;
    case V8GPUVertexFormat::Enum::kFloat16X2:
      return WGPUVertexFormat_Float16x2;
    case V8GPUVertexFormat::Enum::kFloat16X4:
      return WGPUVertexFormat_Float16x4;
    case V8GPUVertexFormat::Enum::kFloat32:
      return WGPUVertexFormat_Float32;
    case V8GPUVertexFormat::Enum::kFloat32X2:
      return WGPUVertexFormat_Float32x2;
    case V8GPUVertexFormat::Enum::kFloat32X3:
      return WGPUVertexFormat_Float32x3;
    case V8GPUVertexFormat::Enum::kFloat32X4:
      return WGPUVertexFormat_Float32x4;
    case V8GPUVertexFormat::Enum::kUint32:
      return WGPUVertexFormat_Uint32;
    case V8GPUVertexFormat::Enum::kUint32X2:
      return WGPUVertexFormat_Uint32x2;
    case V8GPUVertexFormat::Enum::kUint32X3:
      return WGPUVertexFormat_Uint32x3;
    case V8GPUVertexFormat::Enum::kUint32X4:
      return WGPUVertexFormat_Uint32x4;
    case V8GPUVertexFormat::Enum::kSint32:
      return WGPUVertexFormat_Sint32;
    case V8GPUVertexFormat::Enum::kSint32X2:
      return WGPUVertexFormat_Sint32x2;
    case V8GPUVertexFormat::Enum::kSint32X3:
      return WGPUVertexFormat_Sint32x3;
    case V8GPUVertexFormat::Enum::kSint32X4:
      return WGPUVertexFormat_Sint32x4;
  }
}

WGPUAddressMode AsDawnEnum(const V8GPUAddressMode& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUAddressMode::Enum::kClampToEdge:
      return WGPUAddressMode_ClampToEdge;
    case V8GPUAddressMode::Enum::kRepeat:
      return WGPUAddressMode_Repeat;
    case V8GPUAddressMode::Enum::kMirrorRepeat:
      return WGPUAddressMode_MirrorRepeat;
  }
}

WGPUFilterMode AsDawnEnum(const V8GPUFilterMode& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUFilterMode::Enum::kNearest:
      return WGPUFilterMode_Nearest;
    case V8GPUFilterMode::Enum::kLinear:
      return WGPUFilterMode_Linear;
  }
}

WGPUMipmapFilterMode AsDawnEnum(const V8GPUMipmapFilterMode& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUMipmapFilterMode::Enum::kNearest:
      return WGPUMipmapFilterMode_Nearest;
    case V8GPUMipmapFilterMode::Enum::kLinear:
      return WGPUMipmapFilterMode_Linear;
  }
}

WGPUCullMode AsDawnEnum(const V8GPUCullMode& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUCullMode::Enum::kNone:
      return WGPUCullMode_None;
    case V8GPUCullMode::Enum::kFront:
      return WGPUCullMode_Front;
    case V8GPUCullMode::Enum::kBack:
      return WGPUCullMode_Back;
  }
}

WGPUFrontFace AsDawnEnum(const V8GPUFrontFace& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUFrontFace::Enum::kCcw:
      return WGPUFrontFace_CCW;
    case V8GPUFrontFace::Enum::kCw:
      return WGPUFrontFace_CW;
  }
}

WGPUTextureAspect AsDawnEnum(const V8GPUTextureAspect& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUTextureAspect::Enum::kAll:
      return WGPUTextureAspect_All;
    case V8GPUTextureAspect::Enum::kStencilOnly:
      return WGPUTextureAspect_StencilOnly;
    case V8GPUTextureAspect::Enum::kDepthOnly:
      return WGPUTextureAspect_DepthOnly;
  }
}

WGPUErrorFilter AsDawnEnum(const V8GPUErrorFilter& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUErrorFilter::Enum::kOutOfMemory:
      return WGPUErrorFilter_OutOfMemory;
    case V8GPUErrorFilter::Enum::kValidation:
      return WGPUErrorFilter_Validation;
    case V8GPUErrorFilter::Enum::kInternal:
      return WGPUErrorFilter_Internal;
  }
}

WGPUComputePassTimestampLocation AsDawnEnum(
    const V8GPUComputePassTimestampLocation& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUComputePassTimestampLocation::Enum::kBeginning:
      return WGPUComputePassTimestampLocation_Beginning;
    case V8GPUComputePassTimestampLocation::Enum::kEnd:
      return WGPUComputePassTimestampLocation_End;
  }
}

WGPURenderPassTimestampLocation AsDawnEnum(
    const V8GPURenderPassTimestampLocation& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPURenderPassTimestampLocation::Enum::kBeginning:
      return WGPURenderPassTimestampLocation_Beginning;
    case V8GPURenderPassTimestampLocation::Enum::kEnd:
      return WGPURenderPassTimestampLocation_End;
  }
}

const char* FromDawnEnum(WGPUBufferMapState dawn_enum) {
  switch (dawn_enum) {
    case WGPUBufferMapState_Unmapped:
      return "unmapped";
    case WGPUBufferMapState_Pending:
      return "pending";
    case WGPUBufferMapState_Mapped:
      return "mapped";
    case WGPUBufferMapState_Force32:
      NOTREACHED();
  }
  return "";
}

}  // namespace blink
