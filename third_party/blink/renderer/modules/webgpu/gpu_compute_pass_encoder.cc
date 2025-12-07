// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  const base::span<const uint32_t> data_span = dynamic_offsets_data.subspan(
      base::checked_cast<size_t>(dynamic_offsets_data_start),
      dynamic_offsets_data_length);

  GetHandle().SetBindGroup(
      index, bind_group ? bind_group->GetHandle() : wgpu::BindGroup(nullptr),
      data_span.size(), data_span.data());
}

void GPUComputePassEncoder::setImmediates(uint32_t range_offset,
                                          const DOMArrayBufferBase* data,
                                          uint64_t data_offset,
                                          ExceptionState& exception_state) {
  base::span<const uint8_t> data_span;
  if (!ValidateSetImmediatesAndSubSpan(
          exception_state, &data_span, range_offset,
          data->ByteSpanMaybeShared(), 1, data_offset)) {
    return;
  }

  GetHandle().SetImmediates(range_offset, data_span.data(), data_span.size());
}

void GPUComputePassEncoder::setImmediates(uint32_t range_offset,
                                          const DOMArrayBufferBase* data,
                                          uint64_t data_offset,
                                          uint64_t size,
                                          ExceptionState& exception_state) {
  base::span<const uint8_t> data_span;
  if (!ValidateSetImmediatesAndSubSpan(
          exception_state, &data_span, range_offset,
          data->ByteSpanMaybeShared(), 1, data_offset, size)) {
    return;
  }

  GetHandle().SetImmediates(range_offset, data_span.data(), data_span.size());
}

void GPUComputePassEncoder::setImmediates(
    uint32_t range_offset,
    const MaybeShared<DOMArrayBufferView>& data,
    uint64_t data_offset,
    ExceptionState& exception_state) {
  base::span<const uint8_t> data_span;
  if (!ValidateSetImmediatesAndSubSpan(
          exception_state, &data_span, range_offset,
          data->ByteSpanMaybeShared(), data->TypeSize(), data_offset)) {
    return;
  }

  GetHandle().SetImmediates(range_offset, data_span.data(), data_span.size());
}

void GPUComputePassEncoder::setImmediates(
    uint32_t range_offset,
    const MaybeShared<DOMArrayBufferView>& data,
    uint64_t data_offset,
    uint64_t size,
    ExceptionState& exception_state) {
  base::span<const uint8_t> data_span;
  if (!ValidateSetImmediatesAndSubSpan(
          exception_state, &data_span, range_offset,
          data->ByteSpanMaybeShared(), data->TypeSize(), data_offset, size)) {
    return;
  }

  GetHandle().SetImmediates(range_offset, data_span.data(), data_span.size());
}

void GPUComputePassEncoder::writeTimestamp(
    const DawnObject<wgpu::QuerySet>* querySet,
    uint32_t queryIndex,
    ExceptionState& exception_state) {
  V8GPUFeatureName::Enum requiredFeatureEnum =
      V8GPUFeatureName::Enum::kChromiumExperimentalTimestampQueryInsidePasses;
  if (!device_->features()->Has(requiredFeatureEnum)) {
    exception_state.ThrowTypeError(String::Format(
        "Use of the writeTimestamp() method on compute pass requires the '%s' "
        "feature to be enabled on %s.",
        V8GPUFeatureName(requiredFeatureEnum).AsCStr(),
        device_->GetFormattedLabel().c_str()));
    return;
  }
  GetHandle().WriteTimestamp(querySet->GetHandle(), queryIndex);
}

}  // namespace blink
