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
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_cull_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_error_filter.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_feature_name.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_filter_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_front_face.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_index_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_load_op.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_mipmap_filter_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_primitive_topology.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_query_type.h"
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
#include "third_party/blink/renderer/bindings/modules/v8/v8_wgsl_feature_name.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"

namespace blink {

wgpu::BufferBindingType AsDawnEnum(const V8GPUBufferBindingType& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUBufferBindingType::Enum::kUniform:
      return wgpu::BufferBindingType::Uniform;
    case V8GPUBufferBindingType::Enum::kStorage:
      return wgpu::BufferBindingType::Storage;
    case V8GPUBufferBindingType::Enum::kReadOnlyStorage:
      return wgpu::BufferBindingType::ReadOnlyStorage;
  }
}

wgpu::SamplerBindingType AsDawnEnum(
    const V8GPUSamplerBindingType& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUSamplerBindingType::Enum::kFiltering:
      return wgpu::SamplerBindingType::Filtering;
    case V8GPUSamplerBindingType::Enum::kNonFiltering:
      return wgpu::SamplerBindingType::NonFiltering;
    case V8GPUSamplerBindingType::Enum::kComparison:
      return wgpu::SamplerBindingType::Comparison;
  }
}

wgpu::TextureSampleType AsDawnEnum(const V8GPUTextureSampleType& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUTextureSampleType::Enum::kFloat:
      return wgpu::TextureSampleType::Float;
    case V8GPUTextureSampleType::Enum::kUnfilterableFloat:
      return wgpu::TextureSampleType::UnfilterableFloat;
    case V8GPUTextureSampleType::Enum::kDepth:
      return wgpu::TextureSampleType::Depth;
    case V8GPUTextureSampleType::Enum::kSint:
      return wgpu::TextureSampleType::Sint;
    case V8GPUTextureSampleType::Enum::kUint:
      return wgpu::TextureSampleType::Uint;
  }
}

wgpu::StorageTextureAccess AsDawnEnum(
    const V8GPUStorageTextureAccess& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUStorageTextureAccess::Enum::kWriteOnly:
      return wgpu::StorageTextureAccess::WriteOnly;
    case V8GPUStorageTextureAccess::Enum::kReadOnly:
      return wgpu::StorageTextureAccess::ReadOnly;
    case V8GPUStorageTextureAccess::Enum::kReadWrite:
      return wgpu::StorageTextureAccess::ReadWrite;
  }
}

wgpu::CompareFunction AsDawnEnum(const V8GPUCompareFunction& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUCompareFunction::Enum::kNever:
      return wgpu::CompareFunction::Never;
    case V8GPUCompareFunction::Enum::kLess:
      return wgpu::CompareFunction::Less;
    case V8GPUCompareFunction::Enum::kEqual:
      return wgpu::CompareFunction::Equal;
    case V8GPUCompareFunction::Enum::kLessEqual:
      return wgpu::CompareFunction::LessEqual;
    case V8GPUCompareFunction::Enum::kGreater:
      return wgpu::CompareFunction::Greater;
    case V8GPUCompareFunction::Enum::kNotEqual:
      return wgpu::CompareFunction::NotEqual;
    case V8GPUCompareFunction::Enum::kGreaterEqual:
      return wgpu::CompareFunction::GreaterEqual;
    case V8GPUCompareFunction::Enum::kAlways:
      return wgpu::CompareFunction::Always;
  }
}

wgpu::QueryType AsDawnEnum(const V8GPUQueryType& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUQueryType::Enum::kOcclusion:
      return wgpu::QueryType::Occlusion;
    case V8GPUQueryType::Enum::kTimestamp:
      return wgpu::QueryType::Timestamp;
  }
}

const char* FromDawnEnum(wgpu::QueryType dawn_enum) {
  switch (dawn_enum) {
    case wgpu::QueryType::Occlusion:
      return "occlusion";
    case wgpu::QueryType::Timestamp:
      return "timestamp";
  }
  NOTREACHED();
}

wgpu::TextureFormat AsDawnEnum(const V8GPUTextureFormat& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
      // Normal 8 bit formats
    case V8GPUTextureFormat::Enum::kR8Unorm:
      return wgpu::TextureFormat::R8Unorm;
    case V8GPUTextureFormat::Enum::kR8Snorm:
      return wgpu::TextureFormat::R8Snorm;
    case V8GPUTextureFormat::Enum::kR8Uint:
      return wgpu::TextureFormat::R8Uint;
    case V8GPUTextureFormat::Enum::kR8Sint:
      return wgpu::TextureFormat::R8Sint;

      // Normal 16 bit formats
    case V8GPUTextureFormat::Enum::kR16Uint:
      return wgpu::TextureFormat::R16Uint;
    case V8GPUTextureFormat::Enum::kR16Sint:
      return wgpu::TextureFormat::R16Sint;
    case V8GPUTextureFormat::Enum::kR16Float:
      return wgpu::TextureFormat::R16Float;
    case V8GPUTextureFormat::Enum::kRg8Unorm:
      return wgpu::TextureFormat::RG8Unorm;
    case V8GPUTextureFormat::Enum::kRg8Snorm:
      return wgpu::TextureFormat::RG8Snorm;
    case V8GPUTextureFormat::Enum::kRg8Uint:
      return wgpu::TextureFormat::RG8Uint;
    case V8GPUTextureFormat::Enum::kRg8Sint:
      return wgpu::TextureFormat::RG8Sint;

      // Normal 32 bit formats
    case V8GPUTextureFormat::Enum::kR32Uint:
      return wgpu::TextureFormat::R32Uint;
    case V8GPUTextureFormat::Enum::kR32Sint:
      return wgpu::TextureFormat::R32Sint;
    case V8GPUTextureFormat::Enum::kR32Float:
      return wgpu::TextureFormat::R32Float;
    case V8GPUTextureFormat::Enum::kRg16Uint:
      return wgpu::TextureFormat::RG16Uint;
    case V8GPUTextureFormat::Enum::kRg16Sint:
      return wgpu::TextureFormat::RG16Sint;
    case V8GPUTextureFormat::Enum::kRg16Float:
      return wgpu::TextureFormat::RG16Float;
    case V8GPUTextureFormat::Enum::kRgba8Unorm:
      return wgpu::TextureFormat::RGBA8Unorm;
    case V8GPUTextureFormat::Enum::kRgba8UnormSrgb:
      return wgpu::TextureFormat::RGBA8UnormSrgb;
    case V8GPUTextureFormat::Enum::kRgba8Snorm:
      return wgpu::TextureFormat::RGBA8Snorm;
    case V8GPUTextureFormat::Enum::kRgba8Uint:
      return wgpu::TextureFormat::RGBA8Uint;
    case V8GPUTextureFormat::Enum::kRgba8Sint:
      return wgpu::TextureFormat::RGBA8Sint;
    case V8GPUTextureFormat::Enum::kBgra8Unorm:
      return wgpu::TextureFormat::BGRA8Unorm;
    case V8GPUTextureFormat::Enum::kBgra8UnormSrgb:
      return wgpu::TextureFormat::BGRA8UnormSrgb;

      // Packed 32 bit formats
    case V8GPUTextureFormat::Enum::kRgb9E5Ufloat:
      return wgpu::TextureFormat::RGB9E5Ufloat;
    case V8GPUTextureFormat::Enum::kRgb10A2Uint:
      return wgpu::TextureFormat::RGB10A2Uint;
    case V8GPUTextureFormat::Enum::kRgb10A2Unorm:
      return wgpu::TextureFormat::RGB10A2Unorm;
    case V8GPUTextureFormat::Enum::kRg11B10Ufloat:
      return wgpu::TextureFormat::RG11B10Ufloat;

      // Normal 64 bit formats
    case V8GPUTextureFormat::Enum::kRg32Uint:
      return wgpu::TextureFormat::RG32Uint;
    case V8GPUTextureFormat::Enum::kRg32Sint:
      return wgpu::TextureFormat::RG32Sint;
    case V8GPUTextureFormat::Enum::kRg32Float:
      return wgpu::TextureFormat::RG32Float;
    case V8GPUTextureFormat::Enum::kRgba16Uint:
      return wgpu::TextureFormat::RGBA16Uint;
    case V8GPUTextureFormat::Enum::kRgba16Sint:
      return wgpu::TextureFormat::RGBA16Sint;
    case V8GPUTextureFormat::Enum::kRgba16Float:
      return wgpu::TextureFormat::RGBA16Float;

      // Normal 128 bit formats
    case V8GPUTextureFormat::Enum::kRgba32Uint:
      return wgpu::TextureFormat::RGBA32Uint;
    case V8GPUTextureFormat::Enum::kRgba32Sint:
      return wgpu::TextureFormat::RGBA32Sint;
    case V8GPUTextureFormat::Enum::kRgba32Float:
      return wgpu::TextureFormat::RGBA32Float;

      // Depth / Stencil formats
    case V8GPUTextureFormat::Enum::kDepth32Float:
      return wgpu::TextureFormat::Depth32Float;
    case V8GPUTextureFormat::Enum::kDepth32FloatStencil8:
      return wgpu::TextureFormat::Depth32FloatStencil8;
    case V8GPUTextureFormat::Enum::kDepth24Plus:
      return wgpu::TextureFormat::Depth24Plus;
    case V8GPUTextureFormat::Enum::kDepth24PlusStencil8:
      return wgpu::TextureFormat::Depth24PlusStencil8;
    case V8GPUTextureFormat::Enum::kDepth16Unorm:
      return wgpu::TextureFormat::Depth16Unorm;
    case V8GPUTextureFormat::Enum::kStencil8:
      return wgpu::TextureFormat::Stencil8;

      // Block Compression (BC) formats
    case V8GPUTextureFormat::Enum::kBc1RgbaUnorm:
      return wgpu::TextureFormat::BC1RGBAUnorm;
    case V8GPUTextureFormat::Enum::kBc1RgbaUnormSrgb:
      return wgpu::TextureFormat::BC1RGBAUnormSrgb;
    case V8GPUTextureFormat::Enum::kBc2RgbaUnorm:
      return wgpu::TextureFormat::BC2RGBAUnorm;
    case V8GPUTextureFormat::Enum::kBc2RgbaUnormSrgb:
      return wgpu::TextureFormat::BC2RGBAUnormSrgb;
    case V8GPUTextureFormat::Enum::kBc3RgbaUnorm:
      return wgpu::TextureFormat::BC3RGBAUnorm;
    case V8GPUTextureFormat::Enum::kBc3RgbaUnormSrgb:
      return wgpu::TextureFormat::BC3RGBAUnormSrgb;
    case V8GPUTextureFormat::Enum::kBc4RUnorm:
      return wgpu::TextureFormat::BC4RUnorm;
    case V8GPUTextureFormat::Enum::kBc4RSnorm:
      return wgpu::TextureFormat::BC4RSnorm;
    case V8GPUTextureFormat::Enum::kBc5RgUnorm:
      return wgpu::TextureFormat::BC5RGUnorm;
    case V8GPUTextureFormat::Enum::kBc5RgSnorm:
      return wgpu::TextureFormat::BC5RGSnorm;
    case V8GPUTextureFormat::Enum::kBc6HRgbUfloat:
      return wgpu::TextureFormat::BC6HRGBUfloat;
    case V8GPUTextureFormat::Enum::kBc6HRgbFloat:
      return wgpu::TextureFormat::BC6HRGBFloat;
    case V8GPUTextureFormat::Enum::kBc7RgbaUnorm:
      return wgpu::TextureFormat::BC7RGBAUnorm;
    case V8GPUTextureFormat::Enum::kBc7RgbaUnormSrgb:
      return wgpu::TextureFormat::BC7RGBAUnormSrgb;

      // Ericsson Compression (ETC2) formats
    case V8GPUTextureFormat::Enum::kEtc2Rgb8Unorm:
      return wgpu::TextureFormat::ETC2RGB8Unorm;
    case V8GPUTextureFormat::Enum::kEtc2Rgb8UnormSrgb:
      return wgpu::TextureFormat::ETC2RGB8UnormSrgb;
    case V8GPUTextureFormat::Enum::kEtc2Rgb8A1Unorm:
      return wgpu::TextureFormat::ETC2RGB8A1Unorm;
    case V8GPUTextureFormat::Enum::kEtc2Rgb8A1UnormSrgb:
      return wgpu::TextureFormat::ETC2RGB8A1UnormSrgb;
    case V8GPUTextureFormat::Enum::kEtc2Rgba8Unorm:
      return wgpu::TextureFormat::ETC2RGBA8Unorm;
    case V8GPUTextureFormat::Enum::kEtc2Rgba8UnormSrgb:
      return wgpu::TextureFormat::ETC2RGBA8UnormSrgb;
    case V8GPUTextureFormat::Enum::kEacR11Unorm:
      return wgpu::TextureFormat::EACR11Unorm;
    case V8GPUTextureFormat::Enum::kEacR11Snorm:
      return wgpu::TextureFormat::EACR11Snorm;
    case V8GPUTextureFormat::Enum::kEacRg11Unorm:
      return wgpu::TextureFormat::EACRG11Unorm;
    case V8GPUTextureFormat::Enum::kEacRg11Snorm:
      return wgpu::TextureFormat::EACRG11Snorm;

      // Adaptable Scalable Compression (ASTC) formats
    case V8GPUTextureFormat::Enum::kAstc4X4Unorm:
      return wgpu::TextureFormat::ASTC4x4Unorm;
    case V8GPUTextureFormat::Enum::kAstc4X4UnormSrgb:
      return wgpu::TextureFormat::ASTC4x4UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc5X4Unorm:
      return wgpu::TextureFormat::ASTC5x4Unorm;
    case V8GPUTextureFormat::Enum::kAstc5X4UnormSrgb:
      return wgpu::TextureFormat::ASTC5x4UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc5X5Unorm:
      return wgpu::TextureFormat::ASTC5x5Unorm;
    case V8GPUTextureFormat::Enum::kAstc5X5UnormSrgb:
      return wgpu::TextureFormat::ASTC5x5UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc6X5Unorm:
      return wgpu::TextureFormat::ASTC6x5Unorm;
    case V8GPUTextureFormat::Enum::kAstc6X5UnormSrgb:
      return wgpu::TextureFormat::ASTC6x5UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc6X6Unorm:
      return wgpu::TextureFormat::ASTC6x6Unorm;
    case V8GPUTextureFormat::Enum::kAstc6X6UnormSrgb:
      return wgpu::TextureFormat::ASTC6x6UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc8X5Unorm:
      return wgpu::TextureFormat::ASTC8x5Unorm;
    case V8GPUTextureFormat::Enum::kAstc8X5UnormSrgb:
      return wgpu::TextureFormat::ASTC8x5UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc8X6Unorm:
      return wgpu::TextureFormat::ASTC8x6Unorm;
    case V8GPUTextureFormat::Enum::kAstc8X6UnormSrgb:
      return wgpu::TextureFormat::ASTC8x6UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc8X8Unorm:
      return wgpu::TextureFormat::ASTC8x8Unorm;
    case V8GPUTextureFormat::Enum::kAstc8X8UnormSrgb:
      return wgpu::TextureFormat::ASTC8x8UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc10X5Unorm:
      return wgpu::TextureFormat::ASTC10x5Unorm;
    case V8GPUTextureFormat::Enum::kAstc10X5UnormSrgb:
      return wgpu::TextureFormat::ASTC10x5UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc10X6Unorm:
      return wgpu::TextureFormat::ASTC10x6Unorm;
    case V8GPUTextureFormat::Enum::kAstc10X6UnormSrgb:
      return wgpu::TextureFormat::ASTC10x6UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc10X8Unorm:
      return wgpu::TextureFormat::ASTC10x8Unorm;
    case V8GPUTextureFormat::Enum::kAstc10X8UnormSrgb:
      return wgpu::TextureFormat::ASTC10x8UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc10X10Unorm:
      return wgpu::TextureFormat::ASTC10x10Unorm;
    case V8GPUTextureFormat::Enum::kAstc10X10UnormSrgb:
      return wgpu::TextureFormat::ASTC10x10UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc12X10Unorm:
      return wgpu::TextureFormat::ASTC12x10Unorm;
    case V8GPUTextureFormat::Enum::kAstc12X10UnormSrgb:
      return wgpu::TextureFormat::ASTC12x10UnormSrgb;
    case V8GPUTextureFormat::Enum::kAstc12X12Unorm:
      return wgpu::TextureFormat::ASTC12x12Unorm;
    case V8GPUTextureFormat::Enum::kAstc12X12UnormSrgb:
      return wgpu::TextureFormat::ASTC12x12UnormSrgb;
  }
}

const char* FromDawnEnum(wgpu::TextureFormat dawn_enum) {
  switch (dawn_enum) {
    // Normal 8 bit formats
    case wgpu::TextureFormat::R8Unorm:
      return "r8unorm";
    case wgpu::TextureFormat::R8Snorm:
      return "r8snorm";
    case wgpu::TextureFormat::R8Uint:
      return "r8uint";
    case wgpu::TextureFormat::R8Sint:
      return "r8sint";

    // Normal 16 bit formats
    case wgpu::TextureFormat::R16Uint:
      return "r16uint";
    case wgpu::TextureFormat::R16Sint:
      return "r16sint";
    case wgpu::TextureFormat::R16Float:
      return "r16float";
    case wgpu::TextureFormat::RG8Unorm:
      return "rg8unorm";
    case wgpu::TextureFormat::RG8Snorm:
      return "rg8snorm";
    case wgpu::TextureFormat::RG8Uint:
      return "rg8uint";
    case wgpu::TextureFormat::RG8Sint:
      return "rg8sint";

    // Normal 32 bit formats
    case wgpu::TextureFormat::R32Uint:
      return "r32uint";
    case wgpu::TextureFormat::R32Sint:
      return "r32sint";
    case wgpu::TextureFormat::R32Float:
      return "r32float";
    case wgpu::TextureFormat::RG16Uint:
      return "rg16uint";
    case wgpu::TextureFormat::RG16Sint:
      return "rg16sint";
    case wgpu::TextureFormat::RG16Float:
      return "rg16float";
    case wgpu::TextureFormat::RGBA8Unorm:
      return "rgba8unorm";
    case wgpu::TextureFormat::RGBA8UnormSrgb:
      return "rgba8unorm-srgb";
    case wgpu::TextureFormat::RGBA8Snorm:
      return "rgba8snorm";
    case wgpu::TextureFormat::RGBA8Uint:
      return "rgba8uint";
    case wgpu::TextureFormat::RGBA8Sint:
      return "rgba8sint";
    case wgpu::TextureFormat::BGRA8Unorm:
      return "bgra8unorm";
    case wgpu::TextureFormat::BGRA8UnormSrgb:
      return "bgra8unorm-srgb";

    // Packed 32 bit formats
    case wgpu::TextureFormat::RGB9E5Ufloat:
      return "rgb9e5ufloat";
    case wgpu::TextureFormat::RGB10A2Uint:
      return "rgb10a2uint";
    case wgpu::TextureFormat::RGB10A2Unorm:
      return "rgb10a2unorm";
    case wgpu::TextureFormat::RG11B10Ufloat:
      return "rg11b10ufloat";

    // Normal 64 bit formats
    case wgpu::TextureFormat::RG32Uint:
      return "rg32uint";
    case wgpu::TextureFormat::RG32Sint:
      return "rg32sint";
    case wgpu::TextureFormat::RG32Float:
      return "rg32float";
    case wgpu::TextureFormat::RGBA16Uint:
      return "rgba16uint";
    case wgpu::TextureFormat::RGBA16Sint:
      return "rgba16sint";
    case wgpu::TextureFormat::RGBA16Float:
      return "rgba16float";

    // Normal 128 bit formats
    case wgpu::TextureFormat::RGBA32Uint:
      return "rgba32uint";
    case wgpu::TextureFormat::RGBA32Sint:
      return "rgba32sint";
    case wgpu::TextureFormat::RGBA32Float:
      return "rgba32float";

    // Depth / Stencil formats
    case wgpu::TextureFormat::Depth32Float:
      return "depth32float";
    case wgpu::TextureFormat::Depth32FloatStencil8:
      return "depth32float-stencil8";
    case wgpu::TextureFormat::Depth24Plus:
      return "depth24plus";
    case wgpu::TextureFormat::Depth24PlusStencil8:
      return "depth24plus-stencil8";
    case wgpu::TextureFormat::Depth16Unorm:
      return "depth16unorm";
    case wgpu::TextureFormat::Stencil8:
      return "stencil8";

    // Block Compression (BC) formats
    case wgpu::TextureFormat::BC1RGBAUnorm:
      return "bc1-rgba-unorm";
    case wgpu::TextureFormat::BC1RGBAUnormSrgb:
      return "bc1-rgba-unorm-srgb";
    case wgpu::TextureFormat::BC2RGBAUnorm:
      return "bc2-rgba-unorm";
    case wgpu::TextureFormat::BC2RGBAUnormSrgb:
      return "bc2-rgba-unorm-srgb";
    case wgpu::TextureFormat::BC3RGBAUnorm:
      return "bc3-rgba-unorm";
    case wgpu::TextureFormat::BC3RGBAUnormSrgb:
      return "bc3-rgba-unorm-srgb";
    case wgpu::TextureFormat::BC4RUnorm:
      return "bc4-r-unorm";
    case wgpu::TextureFormat::BC4RSnorm:
      return "bc4-r-snorm";
    case wgpu::TextureFormat::BC5RGUnorm:
      return "bc5-rg-unorm";
    case wgpu::TextureFormat::BC5RGSnorm:
      return "bc5-rg-snorm";
    case wgpu::TextureFormat::BC6HRGBUfloat:
      return "bc6h-rgb-ufloat";
    case wgpu::TextureFormat::BC6HRGBFloat:
      return "bc6h-rgb-float";
    case wgpu::TextureFormat::BC7RGBAUnorm:
      return "bc7-rgba-unorm";
    case wgpu::TextureFormat::BC7RGBAUnormSrgb:
      return "bc7-rgba-unorm-srgb";

    // Ericsson Compression (ETC2) formats
    case wgpu::TextureFormat::ETC2RGB8Unorm:
      return "etc2-rgb8unorm";
    case wgpu::TextureFormat::ETC2RGB8UnormSrgb:
      return "etc2-rgb8unorm-srgb";
    case wgpu::TextureFormat::ETC2RGB8A1Unorm:
      return "etc2-rgb8a1unorm";
    case wgpu::TextureFormat::ETC2RGB8A1UnormSrgb:
      return "etc2-rgb8a1unorm-srgb";
    case wgpu::TextureFormat::ETC2RGBA8Unorm:
      return "etc2-rgba8unorm";
    case wgpu::TextureFormat::ETC2RGBA8UnormSrgb:
      return "etc2-rgba8unorm-srgb";
    case wgpu::TextureFormat::EACR11Unorm:
      return "eac-r11unorm";
    case wgpu::TextureFormat::EACR11Snorm:
      return "eac-r11snorm";
    case wgpu::TextureFormat::EACRG11Unorm:
      return "eac-rg11unorm";
    case wgpu::TextureFormat::EACRG11Snorm:
      return "eac-rg11snorm";

    // Adaptable Scalable Compression (ASTC) formats
    case wgpu::TextureFormat::ASTC4x4Unorm:
      return "astc-4x4-unorm";
    case wgpu::TextureFormat::ASTC4x4UnormSrgb:
      return "astc-4x4-unorm-srgb";
    case wgpu::TextureFormat::ASTC5x4Unorm:
      return "astc-5x4-unorm";
    case wgpu::TextureFormat::ASTC5x4UnormSrgb:
      return "astc-5x4-unorm-srgb";
    case wgpu::TextureFormat::ASTC5x5Unorm:
      return "astc-5x5-unorm";
    case wgpu::TextureFormat::ASTC5x5UnormSrgb:
      return "astc-5x5-unorm-srgb";
    case wgpu::TextureFormat::ASTC6x5Unorm:
      return "astc-6x5-unorm";
    case wgpu::TextureFormat::ASTC6x5UnormSrgb:
      return "astc-6x5-unorm-srgb";
    case wgpu::TextureFormat::ASTC6x6Unorm:
      return "astc-6x6-unorm";
    case wgpu::TextureFormat::ASTC6x6UnormSrgb:
      return "astc-6x6-unorm-srgb";
    case wgpu::TextureFormat::ASTC8x5Unorm:
      return "astc-8x5-unorm";
    case wgpu::TextureFormat::ASTC8x5UnormSrgb:
      return "astc-8x5-unorm-srgb";
    case wgpu::TextureFormat::ASTC8x6Unorm:
      return "astc-8x6-unorm";
    case wgpu::TextureFormat::ASTC8x6UnormSrgb:
      return "astc-8x6-unorm-srgb";
    case wgpu::TextureFormat::ASTC8x8Unorm:
      return "astc-8x8-unorm";
    case wgpu::TextureFormat::ASTC8x8UnormSrgb:
      return "astc-8x8-unorm-srgb";
    case wgpu::TextureFormat::ASTC10x5Unorm:
      return "astc-10x5-unorm";
    case wgpu::TextureFormat::ASTC10x5UnormSrgb:
      return "astc-10x5-unorm-srgb";
    case wgpu::TextureFormat::ASTC10x6Unorm:
      return "astc-10x6-unorm";
    case wgpu::TextureFormat::ASTC10x6UnormSrgb:
      return "astc-10x6-unorm-srgb";
    case wgpu::TextureFormat::ASTC10x8Unorm:
      return "astc-10x8-unorm";
    case wgpu::TextureFormat::ASTC10x8UnormSrgb:
      return "astc-10x8-unorm-srgb";
    case wgpu::TextureFormat::ASTC10x10Unorm:
      return "astc-10x10-unorm";
    case wgpu::TextureFormat::ASTC10x10UnormSrgb:
      return "astc-10x10-unorm-srgb";
    case wgpu::TextureFormat::ASTC12x10Unorm:
      return "astc-12x10-unorm";
    case wgpu::TextureFormat::ASTC12x10UnormSrgb:
      return "astc-12x10-unorm-srgb";
    case wgpu::TextureFormat::ASTC12x12Unorm:
      return "astc-12x12-unorm";
    case wgpu::TextureFormat::ASTC12x12UnormSrgb:
      return "astc-12x12-unorm-srgb";

    default:
      NOTREACHED_IN_MIGRATION();
  }
  return "";
}

wgpu::TextureDimension AsDawnEnum(const V8GPUTextureDimension& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUTextureDimension::Enum::k1d:
      return wgpu::TextureDimension::e1D;
    case V8GPUTextureDimension::Enum::k2D:
      return wgpu::TextureDimension::e2D;
    case V8GPUTextureDimension::Enum::k3d:
      return wgpu::TextureDimension::e3D;
  }
}

const char* FromDawnEnum(wgpu::TextureDimension dawn_enum) {
  switch (dawn_enum) {
    case wgpu::TextureDimension::e1D:
      return "1d";
    case wgpu::TextureDimension::e2D:
      return "2d";
    case wgpu::TextureDimension::e3D:
      return "3d";
    case wgpu::TextureDimension::Undefined:
      break;
  }
  NOTREACHED();
}

wgpu::TextureViewDimension AsDawnEnum(
    const V8GPUTextureViewDimension& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUTextureViewDimension::Enum::k1d:
      return wgpu::TextureViewDimension::e1D;
    case V8GPUTextureViewDimension::Enum::k2D:
      return wgpu::TextureViewDimension::e2D;
    case V8GPUTextureViewDimension::Enum::k2DArray:
      return wgpu::TextureViewDimension::e2DArray;
    case V8GPUTextureViewDimension::Enum::kCube:
      return wgpu::TextureViewDimension::Cube;
    case V8GPUTextureViewDimension::Enum::kCubeArray:
      return wgpu::TextureViewDimension::CubeArray;
    case V8GPUTextureViewDimension::Enum::k3d:
      return wgpu::TextureViewDimension::e3D;
  }
}

wgpu::StencilOperation AsDawnEnum(const V8GPUStencilOperation& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUStencilOperation::Enum::kKeep:
      return wgpu::StencilOperation::Keep;
    case V8GPUStencilOperation::Enum::kZero:
      return wgpu::StencilOperation::Zero;
    case V8GPUStencilOperation::Enum::kReplace:
      return wgpu::StencilOperation::Replace;
    case V8GPUStencilOperation::Enum::kInvert:
      return wgpu::StencilOperation::Invert;
    case V8GPUStencilOperation::Enum::kIncrementClamp:
      return wgpu::StencilOperation::IncrementClamp;
    case V8GPUStencilOperation::Enum::kDecrementClamp:
      return wgpu::StencilOperation::DecrementClamp;
    case V8GPUStencilOperation::Enum::kIncrementWrap:
      return wgpu::StencilOperation::IncrementWrap;
    case V8GPUStencilOperation::Enum::kDecrementWrap:
      return wgpu::StencilOperation::DecrementWrap;
  }
}

wgpu::StoreOp AsDawnEnum(const V8GPUStoreOp& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUStoreOp::Enum::kStore:
      return wgpu::StoreOp::Store;
    case V8GPUStoreOp::Enum::kDiscard:
      return wgpu::StoreOp::Discard;
  }
}

wgpu::LoadOp AsDawnEnum(const V8GPULoadOp& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPULoadOp::Enum::kLoad:
      return wgpu::LoadOp::Load;
    case V8GPULoadOp::Enum::kClear:
      return wgpu::LoadOp::Clear;
  }
}

wgpu::IndexFormat AsDawnEnum(const V8GPUIndexFormat& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUIndexFormat::Enum::kUint16:
      return wgpu::IndexFormat::Uint16;
    case V8GPUIndexFormat::Enum::kUint32:
      return wgpu::IndexFormat::Uint32;
  }
}

wgpu::FeatureName AsDawnEnum(const V8GPUFeatureName& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUFeatureName::Enum::kTextureCompressionBc:
      return wgpu::FeatureName::TextureCompressionBC;
    case V8GPUFeatureName::Enum::kTextureCompressionEtc2:
      return wgpu::FeatureName::TextureCompressionETC2;
    case V8GPUFeatureName::Enum::kTextureCompressionAstc:
      return wgpu::FeatureName::TextureCompressionASTC;
    case V8GPUFeatureName::Enum::kTimestampQuery:
      return wgpu::FeatureName::TimestampQuery;
    case V8GPUFeatureName::Enum::
        kChromiumExperimentalTimestampQueryInsidePasses:
      return wgpu::FeatureName::ChromiumExperimentalTimestampQueryInsidePasses;
    case V8GPUFeatureName::Enum::kDepthClipControl:
      return wgpu::FeatureName::DepthClipControl;
    case V8GPUFeatureName::Enum::kDepth32FloatStencil8:
      return wgpu::FeatureName::Depth32FloatStencil8;
    case V8GPUFeatureName::Enum::kIndirectFirstInstance:
      return wgpu::FeatureName::IndirectFirstInstance;
    case V8GPUFeatureName::Enum::kChromiumExperimentalSubgroups:
      return wgpu::FeatureName::ChromiumExperimentalSubgroups;
    case V8GPUFeatureName::Enum::
        kChromiumExperimentalSubgroupUniformControlFlow:
      return wgpu::FeatureName::ChromiumExperimentalSubgroupUniformControlFlow;
    case V8GPUFeatureName::Enum::kRg11B10UfloatRenderable:
      return wgpu::FeatureName::RG11B10UfloatRenderable;
    case V8GPUFeatureName::Enum::kBgra8UnormStorage:
      return wgpu::FeatureName::BGRA8UnormStorage;
    case V8GPUFeatureName::Enum::kShaderF16:
      return wgpu::FeatureName::ShaderF16;
    case V8GPUFeatureName::Enum::kFloat32Filterable:
      return wgpu::FeatureName::Float32Filterable;
    case V8GPUFeatureName::Enum::kDualSourceBlending:
      return wgpu::FeatureName::DualSourceBlending;
    case V8GPUFeatureName::Enum::kSubgroups:
      return wgpu::FeatureName::Subgroups;
    case V8GPUFeatureName::Enum::kSubgroupsF16:
      return wgpu::FeatureName::SubgroupsF16;
    case V8GPUFeatureName::Enum::kClipDistances:
      return wgpu::FeatureName::ClipDistances;
    case V8GPUFeatureName::Enum::kChromiumExperimentalMultiDrawIndirect:
      return wgpu::FeatureName::MultiDrawIndirect;
  }
}

wgpu::PrimitiveTopology AsDawnEnum(const V8GPUPrimitiveTopology& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUPrimitiveTopology::Enum::kPointList:
      return wgpu::PrimitiveTopology::PointList;
    case V8GPUPrimitiveTopology::Enum::kLineList:
      return wgpu::PrimitiveTopology::LineList;
    case V8GPUPrimitiveTopology::Enum::kLineStrip:
      return wgpu::PrimitiveTopology::LineStrip;
    case V8GPUPrimitiveTopology::Enum::kTriangleList:
      return wgpu::PrimitiveTopology::TriangleList;
    case V8GPUPrimitiveTopology::Enum::kTriangleStrip:
      return wgpu::PrimitiveTopology::TriangleStrip;
  }
}

wgpu::BlendFactor AsDawnEnum(const V8GPUBlendFactor& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUBlendFactor::Enum::kZero:
      return wgpu::BlendFactor::Zero;
    case V8GPUBlendFactor::Enum::kOne:
      return wgpu::BlendFactor::One;
    case V8GPUBlendFactor::Enum::kSrc:
      return wgpu::BlendFactor::Src;
    case V8GPUBlendFactor::Enum::kOneMinusSrc:
      return wgpu::BlendFactor::OneMinusSrc;
    case V8GPUBlendFactor::Enum::kSrcAlpha:
      return wgpu::BlendFactor::SrcAlpha;
    case V8GPUBlendFactor::Enum::kOneMinusSrcAlpha:
      return wgpu::BlendFactor::OneMinusSrcAlpha;
    case V8GPUBlendFactor::Enum::kDst:
      return wgpu::BlendFactor::Dst;
    case V8GPUBlendFactor::Enum::kOneMinusDst:
      return wgpu::BlendFactor::OneMinusDst;
    case V8GPUBlendFactor::Enum::kDstAlpha:
      return wgpu::BlendFactor::DstAlpha;
    case V8GPUBlendFactor::Enum::kOneMinusDstAlpha:
      return wgpu::BlendFactor::OneMinusDstAlpha;
    case V8GPUBlendFactor::Enum::kSrcAlphaSaturated:
      return wgpu::BlendFactor::SrcAlphaSaturated;
    case V8GPUBlendFactor::Enum::kConstant:
      return wgpu::BlendFactor::Constant;
    case V8GPUBlendFactor::Enum::kOneMinusConstant:
      return wgpu::BlendFactor::OneMinusConstant;
    case V8GPUBlendFactor::Enum::kSrc1:
      return wgpu::BlendFactor::Src1;
    case V8GPUBlendFactor::Enum::kOneMinusSrc1:
      return wgpu::BlendFactor::OneMinusSrc1;
    case V8GPUBlendFactor::Enum::kSrc1Alpha:
      return wgpu::BlendFactor::Src1Alpha;
    case V8GPUBlendFactor::Enum::kOneMinusSrc1Alpha:
      return wgpu::BlendFactor::OneMinusSrc1Alpha;
  }
}

wgpu::BlendOperation AsDawnEnum(const V8GPUBlendOperation& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUBlendOperation::Enum::kAdd:
      return wgpu::BlendOperation::Add;
    case V8GPUBlendOperation::Enum::kSubtract:
      return wgpu::BlendOperation::Subtract;
    case V8GPUBlendOperation::Enum::kReverseSubtract:
      return wgpu::BlendOperation::ReverseSubtract;
    case V8GPUBlendOperation::Enum::kMin:
      return wgpu::BlendOperation::Min;
    case V8GPUBlendOperation::Enum::kMax:
      return wgpu::BlendOperation::Max;
  }
}

wgpu::VertexStepMode AsDawnEnum(const V8GPUVertexStepMode& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUVertexStepMode::Enum::kVertex:
      return wgpu::VertexStepMode::Vertex;
    case V8GPUVertexStepMode::Enum::kInstance:
      return wgpu::VertexStepMode::Instance;
  }
}

wgpu::VertexFormat AsDawnEnum(const V8GPUVertexFormat& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUVertexFormat::Enum::kUint8X2:
      return wgpu::VertexFormat::Uint8x2;
    case V8GPUVertexFormat::Enum::kUint8X4:
      return wgpu::VertexFormat::Uint8x4;
    case V8GPUVertexFormat::Enum::kSint8X2:
      return wgpu::VertexFormat::Sint8x2;
    case V8GPUVertexFormat::Enum::kSint8X4:
      return wgpu::VertexFormat::Sint8x4;
    case V8GPUVertexFormat::Enum::kUnorm8X2:
      return wgpu::VertexFormat::Unorm8x2;
    case V8GPUVertexFormat::Enum::kUnorm8X4:
      return wgpu::VertexFormat::Unorm8x4;
    case V8GPUVertexFormat::Enum::kSnorm8X2:
      return wgpu::VertexFormat::Snorm8x2;
    case V8GPUVertexFormat::Enum::kSnorm8X4:
      return wgpu::VertexFormat::Snorm8x4;
    case V8GPUVertexFormat::Enum::kUint16X2:
      return wgpu::VertexFormat::Uint16x2;
    case V8GPUVertexFormat::Enum::kUint16X4:
      return wgpu::VertexFormat::Uint16x4;
    case V8GPUVertexFormat::Enum::kSint16X2:
      return wgpu::VertexFormat::Sint16x2;
    case V8GPUVertexFormat::Enum::kSint16X4:
      return wgpu::VertexFormat::Sint16x4;
    case V8GPUVertexFormat::Enum::kUnorm16X2:
      return wgpu::VertexFormat::Unorm16x2;
    case V8GPUVertexFormat::Enum::kUnorm16X4:
      return wgpu::VertexFormat::Unorm16x4;
    case V8GPUVertexFormat::Enum::kSnorm16X2:
      return wgpu::VertexFormat::Snorm16x2;
    case V8GPUVertexFormat::Enum::kSnorm16X4:
      return wgpu::VertexFormat::Snorm16x4;
    case V8GPUVertexFormat::Enum::kFloat16X2:
      return wgpu::VertexFormat::Float16x2;
    case V8GPUVertexFormat::Enum::kFloat16X4:
      return wgpu::VertexFormat::Float16x4;
    case V8GPUVertexFormat::Enum::kFloat32:
      return wgpu::VertexFormat::Float32;
    case V8GPUVertexFormat::Enum::kFloat32X2:
      return wgpu::VertexFormat::Float32x2;
    case V8GPUVertexFormat::Enum::kFloat32X3:
      return wgpu::VertexFormat::Float32x3;
    case V8GPUVertexFormat::Enum::kFloat32X4:
      return wgpu::VertexFormat::Float32x4;
    case V8GPUVertexFormat::Enum::kUint32:
      return wgpu::VertexFormat::Uint32;
    case V8GPUVertexFormat::Enum::kUint32X2:
      return wgpu::VertexFormat::Uint32x2;
    case V8GPUVertexFormat::Enum::kUint32X3:
      return wgpu::VertexFormat::Uint32x3;
    case V8GPUVertexFormat::Enum::kUint32X4:
      return wgpu::VertexFormat::Uint32x4;
    case V8GPUVertexFormat::Enum::kSint32:
      return wgpu::VertexFormat::Sint32;
    case V8GPUVertexFormat::Enum::kSint32X2:
      return wgpu::VertexFormat::Sint32x2;
    case V8GPUVertexFormat::Enum::kSint32X3:
      return wgpu::VertexFormat::Sint32x3;
    case V8GPUVertexFormat::Enum::kSint32X4:
      return wgpu::VertexFormat::Sint32x4;
    case V8GPUVertexFormat::Enum::kUnorm1010102:
      return wgpu::VertexFormat::Unorm10_10_10_2;
  }
}

wgpu::AddressMode AsDawnEnum(const V8GPUAddressMode& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUAddressMode::Enum::kClampToEdge:
      return wgpu::AddressMode::ClampToEdge;
    case V8GPUAddressMode::Enum::kRepeat:
      return wgpu::AddressMode::Repeat;
    case V8GPUAddressMode::Enum::kMirrorRepeat:
      return wgpu::AddressMode::MirrorRepeat;
  }
}

wgpu::FilterMode AsDawnEnum(const V8GPUFilterMode& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUFilterMode::Enum::kNearest:
      return wgpu::FilterMode::Nearest;
    case V8GPUFilterMode::Enum::kLinear:
      return wgpu::FilterMode::Linear;
  }
}

wgpu::MipmapFilterMode AsDawnEnum(const V8GPUMipmapFilterMode& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUMipmapFilterMode::Enum::kNearest:
      return wgpu::MipmapFilterMode::Nearest;
    case V8GPUMipmapFilterMode::Enum::kLinear:
      return wgpu::MipmapFilterMode::Linear;
  }
}

wgpu::CullMode AsDawnEnum(const V8GPUCullMode& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUCullMode::Enum::kNone:
      return wgpu::CullMode::None;
    case V8GPUCullMode::Enum::kFront:
      return wgpu::CullMode::Front;
    case V8GPUCullMode::Enum::kBack:
      return wgpu::CullMode::Back;
  }
}

wgpu::FrontFace AsDawnEnum(const V8GPUFrontFace& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUFrontFace::Enum::kCcw:
      return wgpu::FrontFace::CCW;
    case V8GPUFrontFace::Enum::kCw:
      return wgpu::FrontFace::CW;
  }
}

wgpu::TextureAspect AsDawnEnum(const V8GPUTextureAspect& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUTextureAspect::Enum::kAll:
      return wgpu::TextureAspect::All;
    case V8GPUTextureAspect::Enum::kStencilOnly:
      return wgpu::TextureAspect::StencilOnly;
    case V8GPUTextureAspect::Enum::kDepthOnly:
      return wgpu::TextureAspect::DepthOnly;
  }
}

wgpu::ErrorFilter AsDawnEnum(const V8GPUErrorFilter& webgpu_enum) {
  switch (webgpu_enum.AsEnum()) {
    case V8GPUErrorFilter::Enum::kOutOfMemory:
      return wgpu::ErrorFilter::OutOfMemory;
    case V8GPUErrorFilter::Enum::kValidation:
      return wgpu::ErrorFilter::Validation;
    case V8GPUErrorFilter::Enum::kInternal:
      return wgpu::ErrorFilter::Internal;
  }
}

const char* FromDawnEnum(wgpu::BufferMapState dawn_enum) {
  switch (dawn_enum) {
    case wgpu::BufferMapState::Unmapped:
      return "unmapped";
    case wgpu::BufferMapState::Pending:
      return "pending";
    case wgpu::BufferMapState::Mapped:
      return "mapped";
  }
  NOTREACHED();
}

const char* FromDawnEnum(wgpu::BackendType dawn_enum) {
  switch (dawn_enum) {
    case wgpu::BackendType::Undefined:
      return "";
    case wgpu::BackendType::Null:
      return "null";
    case wgpu::BackendType::WebGPU:
      return "WebGPU";
    case wgpu::BackendType::D3D11:
      return "D3D11";
    case wgpu::BackendType::D3D12:
      return "D3D12";
    case wgpu::BackendType::Metal:
      return "metal";
    case wgpu::BackendType::Vulkan:
      return "vulkan";
    case wgpu::BackendType::OpenGL:
      return "openGL";
    case wgpu::BackendType::OpenGLES:
      return "openGLES";
  }
  NOTREACHED();
}

const char* FromDawnEnum(wgpu::AdapterType dawn_enum) {
  switch (dawn_enum) {
    case wgpu::AdapterType::DiscreteGPU:
      return "discrete GPU";
    case wgpu::AdapterType::IntegratedGPU:
      return "integrated GPU";
    case wgpu::AdapterType::CPU:
      return "CPU";
    case wgpu::AdapterType::Unknown:
      return "unknown";
  }
  NOTREACHED();
}

bool FromDawnEnum(wgpu::WGSLFeatureName dawn_enum, V8WGSLFeatureName* result) {
  switch (dawn_enum) {
    case wgpu::WGSLFeatureName::ReadonlyAndReadwriteStorageTextures:
      *result = V8WGSLFeatureName(
          V8WGSLFeatureName::Enum::kReadonlyAndReadwriteStorageTextures);
      return true;
    case wgpu::WGSLFeatureName::Packed4x8IntegerDotProduct:
      *result = V8WGSLFeatureName(
          V8WGSLFeatureName::Enum::kPacked4X8IntegerDotProduct);
      return true;
    case wgpu::WGSLFeatureName::UnrestrictedPointerParameters:
      *result = V8WGSLFeatureName(
          V8WGSLFeatureName::Enum::kUnrestrictedPointerParameters);
      return true;
    case wgpu::WGSLFeatureName::PointerCompositeAccess:
      *result =
          V8WGSLFeatureName(V8WGSLFeatureName::Enum::kPointerCompositeAccess);
      return true;

    case wgpu::WGSLFeatureName::ChromiumTestingUnimplemented:
      *result = V8WGSLFeatureName(
          V8WGSLFeatureName::Enum::kChromiumTestingUnimplemented);
      return true;
    case wgpu::WGSLFeatureName::ChromiumTestingUnsafeExperimental:
      *result = V8WGSLFeatureName(
          V8WGSLFeatureName::Enum::kChromiumTestingUnsafeExperimental);
      return true;
    case wgpu::WGSLFeatureName::ChromiumTestingExperimental:
      *result = V8WGSLFeatureName(
          V8WGSLFeatureName::Enum::kChromiumTestingExperimental);
      return true;
    case wgpu::WGSLFeatureName::ChromiumTestingShippedWithKillswitch:
      *result = V8WGSLFeatureName(
          V8WGSLFeatureName::Enum::kChromiumTestingShippedWithKillswitch);
      return true;
    case wgpu::WGSLFeatureName::ChromiumTestingShipped:
      *result =
          V8WGSLFeatureName(V8WGSLFeatureName::Enum::kChromiumTestingShipped);
      return true;

    default:
      return false;
  }
}

}  // namespace blink
