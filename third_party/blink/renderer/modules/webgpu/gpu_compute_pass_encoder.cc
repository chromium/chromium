// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pass_encoder.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pipeline.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_query_set.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"

namespace blink {

GPUComputePassEncoder::GPUComputePassEncoder(
    GPUDevice* device,
    wgpu::ComputePassEncoder compute_pass_encoder,
    const String& label)
    : DawnObject<wgpu::ComputePassEncoder>(device,
                                           compute_pass_encoder,
                                           label) {}

void GPUComputePassEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bindGroup,
    const Vector<uint32_t>& dynamicOffsets) {
  GetHandle().SetBindGroup(
      index, bindGroup ? bindGroup->GetHandle() : wgpu::BindGroup(nullptr),
      dynamicOffsets.size(), dynamicOffsets.data());
}

void GPUComputePassEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bind_group,
    base::span<const uint32_t> dynamic_offsets_data,
    uint64_t dynamic_offsets_data_start,
    uint32_t dynamic_offsets_data_length,
    ExceptionState& exception_state) {
  if (!ValidateSetBindGroupDynamicOffsets(
          dynamic_offsets_data, dynamic_offsets_data_start,
          dynamic_offsets_data_length, exception_state)) {
    return;
  }

  const uint32_t* data =
      dynamic_offsets_data.data() + dynamic_offsets_data_start;

  GetHandle().SetBindGroup(
      index, bind_group ? bind_group->GetHandle() : wgpu::BindGroup(nullptr),
      dynamic_offsets_data_length, data);
}

void GPUComputePassEncoder::writeTimestamp(
    const DawnObject<wgpu::QuerySet>* querySet,
    uint32_t queryIndex,
    ExceptionState& exception_state) {
  V8GPUFeatureName::Enum requiredFeatureEnum =
      V8GPUFeatureName::Enum::kChromiumExperimentalTimestampQueryInsidePasses;
  if (!device_->features()->has(requiredFeatureEnum)) {
    exception_state.ThrowTypeError(String::Format(
        "Use of the writeTimestamp() method on compute pass requires the '%s' "
        "feature to be enabled on %s.",
        V8GPUFeatureName(requiredFeatureEnum).AsCStr(),
        device_->formattedLabel().c_str()));
    return;
  }
  GetHandle().WriteTimestamp(querySet->GetHandle(), queryIndex);
}

}  // namespace blink
