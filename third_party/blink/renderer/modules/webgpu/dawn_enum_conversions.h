#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_ENUM_CONVERSIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_ENUM_CONVERSIONS_H_

#include <dawn/webgpu.h>

namespace blink {

class V8GPUBufferBindingType;
class V8GPUSamplerBindingType;
class V8GPUTextureSampleType;
class V8GPUStorageTextureAccess;
class V8GPUCompareFunction;
class V8GPUQueryType;
class V8GPUTextureFormat;
class V8GPUTextureDimension;
class V8GPUTextureViewDimension;
class V8GPUStencilOperation;
class V8GPUStoreOp;
class V8GPULoadOp;
class V8GPUFeatureName;
class V8GPUIndexFormat;
class V8GPUPrimitiveTopology;
class V8GPUBlendFactor;
class V8GPUBlendOperation;
class V8GPUVertexStepMode;
class V8GPUVertexFormat;
class V8GPUAddressMode;
class V8GPUMipmapFilterMode;
class V8GPUFilterMode;
class V8GPUCullMode;
class V8GPUFrontFace;
class V8GPUTextureAspect;
class V8GPUErrorFilter;
enum class PredefinedColorSpace;

// Convert WebGPU bitfield values to Dawn enums. These have the same value.
template <typename DawnFlags>
DawnFlags AsDawnFlags(uint32_t webgpu_enum) {
  return static_cast<DawnFlags>(webgpu_enum);
}

// Convert WebGPU IDL enums to Dawn enums.
WGPUBufferBindingType AsDawnEnum(const V8GPUBufferBindingType& webgpu_enum);
WGPUSamplerBindingType AsDawnEnum(const V8GPUSamplerBindingType& webgpu_enum);
WGPUTextureSampleType AsDawnEnum(const V8GPUTextureSampleType& webgpu_enum);
WGPUStorageTextureAccess AsDawnEnum(
    const V8GPUStorageTextureAccess& webgpu_enum);
WGPUCompareFunction AsDawnEnum(const V8GPUCompareFunction& webgpu_enum);
WGPUQueryType AsDawnEnum(const V8GPUQueryType& webgpu_enum);
WGPUTextureFormat AsDawnEnum(const V8GPUTextureFormat& webgpu_enum);
WGPUTextureDimension AsDawnEnum(const V8GPUTextureDimension& webgpu_enum);
WGPUTextureViewDimension AsDawnEnum(
    const V8GPUTextureViewDimension& webgpu_enum);
WGPUStencilOperation AsDawnEnum(const V8GPUStencilOperation& webgpu_enum);
WGPUStoreOp AsDawnEnum(const V8GPUStoreOp& webgpu_enum);
WGPULoadOp AsDawnEnum(const V8GPULoadOp& webgpu_enum);
WGPUIndexFormat AsDawnEnum(const V8GPUIndexFormat& webgpu_enum);
WGPUFeatureName AsDawnEnum(const V8GPUFeatureName& webgpu_enum);
WGPUPrimitiveTopology AsDawnEnum(const V8GPUPrimitiveTopology& webgpu_enum);
WGPUBlendFactor AsDawnEnum(const V8GPUBlendFactor& webgpu_enum);
WGPUBlendOperation AsDawnEnum(const V8GPUBlendOperation& webgpu_enum);
WGPUVertexStepMode AsDawnEnum(const V8GPUVertexStepMode& webgpu_enum);
WGPUVertexFormat AsDawnEnum(const V8GPUVertexFormat& webgpu_enum);
WGPUAddressMode AsDawnEnum(const V8GPUAddressMode& webgpu_enum);
WGPUFilterMode AsDawnEnum(const V8GPUFilterMode& webgpu_enum);
WGPUMipmapFilterMode AsDawnEnum(const V8GPUMipmapFilterMode& webgpu_enum);
WGPUCullMode AsDawnEnum(const V8GPUCullMode& webgpu_enum);
WGPUFrontFace AsDawnEnum(const V8GPUFrontFace& webgpu_enum);
WGPUTextureAspect AsDawnEnum(const V8GPUTextureAspect& webgpu_enum);
WGPUErrorFilter AsDawnEnum(const V8GPUErrorFilter& webgpu_enum);

// Convert Dawn enums to WebGPU IDL enums.
const char* FromDawnEnum(WGPUQueryType dawn_enum);
const char* FromDawnEnum(WGPUTextureDimension dawn_enum);
const char* FromDawnEnum(WGPUTextureFormat dawn_enum);
const char* FromDawnEnum(WGPUBufferMapState dawn_enum);
const char* FromDawnEnum(WGPUBackendType dawn_enum);
const char* FromDawnEnum(WGPUAdapterType dawn_enum);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_ENUM_CONVERSIONS_H_
