// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webgpu/gpu_render_pass_encoder.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_index_format.h"
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
    wgpu::RenderPassEncoder render_pass_encoder,
    const String& label)
    : DawnObject<wgpu::RenderPassEncoder>(device,
                                          std::move(render_pass_encoder),
                                          label) {}

void GPURenderPassEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bindGroup,
    const Vector<uint32_t>& dynamicOffsets) {
  GetHandle().SetBindGroup(
      index, bindGroup ? bindGroup->GetHandle() : wgpu::BindGroup(nullptr),
      dynamicOffsets.size(), dynamicOffsets.data());
}

void GPURenderPassEncoder::setBindGroup(
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

void GPURenderPassEncoder::setBlendConstant(const V8GPUColor* color,
                                            ExceptionState& exception_state) {
  wgpu::Color dawn_color;
  if (!ConvertToDawn(color, &dawn_color, exception_state)) {
    return;
  }

  GetHandle().SetBlendConstant(&dawn_color);
}

void GPURenderPassEncoder::multiDrawIndirect(
    const DawnObject<wgpu::Buffer>* indirectBuffer,
    uint64_t indirectOffset,
    uint32_t maxDrawCount,
    ExceptionState& exception_state) {
  multiDrawIndirect(indirectBuffer, indirectOffset, maxDrawCount, nullptr, 0,
                    exception_state);
}

void GPURenderPassEncoder::multiDrawIndirect(
    const DawnObject<wgpu::Buffer>* indirectBuffer,
    uint64_t indirectOffset,
    uint32_t maxDrawCount,
    DawnObject<wgpu::Buffer>* drawCountBuffer,
    ExceptionState& exception_state) {
  multiDrawIndirect(indirectBuffer, indirectOffset, maxDrawCount,
                    drawCountBuffer, 0, exception_state);
}

void GPURenderPassEncoder::multiDrawIndirect(
    const DawnObject<wgpu::Buffer>* indirectBuffer,
    uint64_t indirectOffset,
    uint32_t maxDrawCount,
    DawnObject<wgpu::Buffer>* drawCountBuffer,
    uint64_t drawCountBufferOffset,
    ExceptionState& exception_state) {
  V8GPUFeatureName::Enum requiredFeatureEnum =
      V8GPUFeatureName::Enum::kChromiumExperimentalMultiDrawIndirect;

  if (!device_->features()->has(requiredFeatureEnum)) {
    exception_state.ThrowTypeError(
        String::Format("Use of the multiDrawIndirect() method on render pass "
                       "requires the '%s' "
                       "feature to be enabled on %s.",
                       V8GPUFeatureName(requiredFeatureEnum).AsCStr(),
                       device_->formattedLabel().c_str()));
    return;
  }
  GetHandle().MultiDrawIndirect(
      indirectBuffer->GetHandle(), indirectOffset, maxDrawCount,
      drawCountBuffer ? drawCountBuffer->GetHandle() : wgpu::Buffer(nullptr),
      drawCountBufferOffset);
}

void GPURenderPassEncoder::multiDrawIndexedIndirect(
    const DawnObject<wgpu::Buffer>* indirectBuffer,
    uint64_t indirectOffset,
    uint32_t maxDrawCount,
    ExceptionState& exception_state) {
  multiDrawIndexedIndirect(indirectBuffer, indirectOffset, maxDrawCount,
                           nullptr, 0, exception_state);
}

void GPURenderPassEncoder::multiDrawIndexedIndirect(
    const DawnObject<wgpu::Buffer>* indirectBuffer,
    uint64_t indirectOffset,
    uint32_t maxDrawCount,
    DawnObject<wgpu::Buffer>* drawCountBuffer,
    ExceptionState& exception_state) {
  multiDrawIndexedIndirect(indirectBuffer, indirectOffset, maxDrawCount,
                           drawCountBuffer, 0, exception_state);
}

void GPURenderPassEncoder::multiDrawIndexedIndirect(
    const DawnObject<wgpu::Buffer>* indirectBuffer,
    uint64_t indirectOffset,
    uint32_t maxDrawCount,
    DawnObject<wgpu::Buffer>* drawCountBuffer,
    uint64_t drawCountBufferOffset,
    ExceptionState& exception_state) {
  V8GPUFeatureName::Enum requiredFeatureEnum =
      V8GPUFeatureName::Enum::kChromiumExperimentalMultiDrawIndirect;

  if (!device_->features()->has(requiredFeatureEnum)) {
    exception_state.ThrowTypeError(String::Format(
        "Use of the multiDrawIndexedIndirect() method on render pass "
        "requires the '%s' "
        "feature to be enabled on %s.",
        V8GPUFeatureName(requiredFeatureEnum).AsCStr(),
        device_->formattedLabel().c_str()));
    return;
  }
  GetHandle().MultiDrawIndexedIndirect(
      indirectBuffer->GetHandle(), indirectOffset, maxDrawCount,
      drawCountBuffer ? drawCountBuffer->GetHandle() : wgpu::Buffer(nullptr),
      drawCountBufferOffset);
}

void GPURenderPassEncoder::executeBundles(
    const HeapVector<Member<GPURenderBundle>>& bundles) {
  std::unique_ptr<wgpu::RenderBundle[]> dawn_bundles = AsDawnType(bundles);

  GetHandle().ExecuteBundles(bundles.size(), dawn_bundles.get());
}

void GPURenderPassEncoder::writeTimestamp(
    const DawnObject<wgpu::QuerySet>* querySet,
    uint32_t queryIndex,
    ExceptionState& exception_state) {
  V8GPUFeatureName::Enum requiredFeatureEnum =
      V8GPUFeatureName::Enum::kChromiumExperimentalTimestampQueryInsidePasses;

  if (!device_->features()->has(requiredFeatureEnum)) {
    exception_state.ThrowTypeError(String::Format(
        "Use of the writeTimestamp() method on render pass requires the '%s' "
        "feature to be enabled on %s.",
        V8GPUFeatureName(requiredFeatureEnum).AsCStr(),
        device_->formattedLabel().c_str()));
    return;
  }
  GetHandle().WriteTimestamp(querySet->GetHandle(), queryIndex);
}

}  // namespace blink
