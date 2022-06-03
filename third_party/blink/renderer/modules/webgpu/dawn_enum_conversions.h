#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_ENUM_CONVERSIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_ENUM_CONVERSIONS_H_

#include <dawn/webgpu.h>

namespace WTF {
class String;
}

namespace blink {

class V8GPUIndexFormat;

// Convert WebGPU bitfield values to Dawn enums. These have the same value.
template <typename DawnEnum>
DawnEnum AsDawnEnum(uint32_t webgpu_enum) {
  return static_cast<DawnEnum>(webgpu_enum);
}

// Convert WebGPU string enums to Dawn enums.
template <typename DawnEnum>
DawnEnum AsDawnEnum(const WTF::String& webgpu_enum);
WGPUIndexFormat AsDawnEnum(const V8GPUIndexFormat& webgpu_enum);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_ENUM_CONVERSIONS_H_
