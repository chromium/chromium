// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CONVERSIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CONVERSIONS_H_

#include <dawn/webgpu.h>

#include <memory>

#include "base/logging.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"

// This file provides helpers for converting WebGPU objects, descriptors,
// and enums from Blink to Dawn types.

namespace blink {

class DoubleSequenceOrGPUColorDict;
class GPUColorDict;
class GPUProgrammableStageDescriptor;
class UnsignedLongSequenceOrGPUExtent3DDict;
class UnsignedLongSequenceOrGPUOrigin3DDict;

// Convert WebGPU bitfield values to Dawn enums. These have the same value.
template <typename DawnEnum>
DawnEnum AsDawnEnum(uint32_t webgpu_enum) {
  return static_cast<DawnEnum>(webgpu_enum);
}

// Convert WebGPU string enums to Dawn enums.
template <typename DawnEnum>
DawnEnum AsDawnEnum(const WTF::String& webgpu_enum);

// These conversions are used multiple times and are declared here. Conversions
// used only once, for example for object construction, are defined
// individually.
WGPUColor AsDawnColor(const Vector<double>&);
WGPUColor AsDawnType(const GPUColorDict*);
WGPUColor AsDawnType(const DoubleSequenceOrGPUColorDict*);
WGPUExtent3D AsDawnType(const UnsignedLongSequenceOrGPUExtent3DDict*);
WGPUOrigin3D AsDawnType(const UnsignedLongSequenceOrGPUOrigin3DDict*);

using OwnedProgrammableStageDescriptor =
    std::tuple<WGPUProgrammableStageDescriptor, std::unique_ptr<char[]>>;
OwnedProgrammableStageDescriptor AsDawnType(
    const GPUProgrammableStageDescriptor*);

// WebGPU objects are converted to Dawn objects by getting the opaque handle
// which can be passed to Dawn.
template <typename Handle>
Handle AsDawnType(const DawnObject<Handle>* object) {
  DCHECK(object);
  return object->GetHandle();
}

template <typename WebGPUType>
using TypeOfDawnType = decltype(AsDawnType(std::declval<const WebGPUType*>()));

// Helper for converting a list of objects to Dawn structs or handles
template <typename WebGPUType>
std::unique_ptr<TypeOfDawnType<WebGPUType>[]> AsDawnType(
    const HeapVector<Member<WebGPUType>>& webgpu_objects) {
  using DawnType = TypeOfDawnType<WebGPUType>;

  wtf_size_t count = webgpu_objects.size();
  // TODO(enga): Pass in temporary memory or an allocator so we don't make a
  // separate memory allocation here.
  std::unique_ptr<DawnType[]> dawn_objects(new DawnType[count]);
  for (wtf_size_t i = 0; i < count; ++i) {
    dawn_objects[i] = AsDawnType(webgpu_objects[i].Get());
  }
  return dawn_objects;
}

template <typename DawnEnum, typename WebGPUEnum>
std::unique_ptr<DawnEnum[]> AsDawnEnum(const Vector<WebGPUEnum>& webgpu_enums) {
  wtf_size_t count = webgpu_enums.size();
  // TODO(enga): Pass in temporary memory or an allocator so we don't make a
  // separate memory allocation here.
  std::unique_ptr<DawnEnum[]> dawn_enums(new DawnEnum[count]);
  for (wtf_size_t i = 0; i < count; ++i) {
    dawn_enums[i] = AsDawnEnum<DawnEnum>(webgpu_enums[i]);
  }
  return dawn_enums;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CONVERSIONS_H_
