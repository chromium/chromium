// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_pass_encoder.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_index_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_doublesequence_gpucolordict.h"
#include "third_party/blink/renderer/core/typed_arrays/typed_flexible_array_buffer_view.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_query_set.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"

namespace blink {

GPURenderPassEncoder::GPURenderPassEncoder(
    GPUDevice* device,
    WGPURenderPassEncoder render_pass_encoder)
    : DawnObject<WGPURenderPassEncoder>(device, render_pass_encoder) {}

void GPURenderPassEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bindGroup,
    const Vector<uint32_t>& dynamicOffsets) {
  GetProcs().renderPassEncoderSetBindGroup(
      GetHandle(), index, bindGroup->GetHandle(), dynamicOffsets.size(),
      dynamicOffsets.data());
}

void GPURenderPassEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bind_group,
    const FlexibleUint32Array& dynamic_offsets_data,
    uint64_t dynamic_offsets_data_start,
    uint32_t dynamic_offsets_data_length,
    ExceptionState& exception_state) {
  if (!ValidateSetBindGroupDynamicOffsets(
          dynamic_offsets_data, dynamic_offsets_data_start,
          dynamic_offsets_data_length, exception_state)) {
    return;
  }

  const uint32_t* data =
      dynamic_offsets_data.DataMaybeOnStack() + dynamic_offsets_data_start;

  GetProcs().renderPassEncoderSetBindGroup(GetHandle(), index,
                                           bind_group->GetHandle(),
                                           dynamic_offsets_data_length, data);
}

void GPURenderPassEncoder::setBlendConstant(const V8GPUColor* color,
                                            ExceptionState& exception_state) {
  if (color->IsDoubleSequence() && color->GetAsDoubleSequence().size() != 4) {
    exception_state.ThrowRangeError("color size must be 4");
    return;
  }

  WGPUColor dawn_color = AsDawnType(color);
  GetProcs().renderPassEncoderSetBlendConstant(GetHandle(), &dawn_color);
}

void GPURenderPassEncoder::executeBundles(
    const HeapVector<Member<GPURenderBundle>>& bundles) {
  std::unique_ptr<WGPURenderBundle[]> dawn_bundles = AsDawnType(bundles);

  GetProcs().renderPassEncoderExecuteBundles(GetHandle(), bundles.size(),
                                             dawn_bundles.get());
}

void GPURenderPassEncoder::writeTimestamp(
    const DawnObject<WGPUQuerySet>* querySet,
    uint32_t queryIndex,
    ExceptionState& exception_state) {
  // TODO(crbug.com/1379384): Avoid using string comparisons for checking
  // features because of inefficiency, maybe we can use V8GPUFeatureName instead
  // of string.
  const char* requiredFeature = "timestamp-query-inside-passes";
  if (!device_->features()->has(requiredFeature)) {
    exception_state.ThrowTypeError(String::Format(
        "Use of the writeTimestamp() method on render pass requires the '%s' "
        "feature to be enabled on %s.",
        requiredFeature, device_->formattedLabel().c_str()));
    return;
  }
  GetProcs().renderPassEncoderWriteTimestamp(GetHandle(), querySet->GetHandle(),
                                             queryIndex);
}

}  // namespace blink
