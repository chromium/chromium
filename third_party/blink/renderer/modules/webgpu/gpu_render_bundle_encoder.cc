// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle_encoder.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_bundle_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_bundle_encoder_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
GPURenderBundleEncoder* GPURenderBundleEncoder::Create(
    GPUDevice* device,
    const GPURenderBundleEncoderDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  size_t color_formats_count = webgpu_desc->colorFormats().size();

  for (const auto& color_format : webgpu_desc->colorFormats()) {
    if (color_format.has_value() &&
        !device->ValidateTextureFormatUsage(color_format.value(),
                                            exception_state)) {
      return nullptr;
    }
  }

  std::unique_ptr<WGPUTextureFormat[]> color_formats =
      AsDawnEnum<WGPUTextureFormat>(webgpu_desc->colorFormats());

  WGPUTextureFormat depth_stencil_format = WGPUTextureFormat_Undefined;
  if (webgpu_desc->hasDepthStencilFormat()) {
    if (!device->ValidateTextureFormatUsage(webgpu_desc->depthStencilFormat(),
                                            exception_state)) {
      return nullptr;
    }

    depth_stencil_format = AsDawnEnum(webgpu_desc->depthStencilFormat());
  }

  std::string label;
  WGPURenderBundleEncoderDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
#ifdef WGPU_BREAKING_CHANGE_COUNT_RENAME
  dawn_desc.colorFormatCount = color_formats_count;
#else
  dawn_desc.colorFormatsCount = color_formats_count;
#endif
  dawn_desc.colorFormats = color_formats.get();
  dawn_desc.depthStencilFormat = depth_stencil_format;
  dawn_desc.sampleCount = webgpu_desc->sampleCount();
  dawn_desc.depthReadOnly = webgpu_desc->depthReadOnly();
  dawn_desc.stencilReadOnly = webgpu_desc->stencilReadOnly();
  if (webgpu_desc->hasLabel()) {
    label = webgpu_desc->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  GPURenderBundleEncoder* encoder =
      MakeGarbageCollected<GPURenderBundleEncoder>(
          device, device->GetProcs().deviceCreateRenderBundleEncoder(
                      device->GetHandle(), &dawn_desc));
  if (webgpu_desc->hasLabel())
    encoder->setLabel(webgpu_desc->label());
  return encoder;
}

GPURenderBundleEncoder::GPURenderBundleEncoder(
    GPUDevice* device,
    WGPURenderBundleEncoder render_bundle_encoder)
    : DawnObject<WGPURenderBundleEncoder>(device, render_bundle_encoder) {}

void GPURenderBundleEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bindGroup,
    const Vector<uint32_t>& dynamicOffsets) {
  WGPUBindGroupImpl* bgImpl = bindGroup ? bindGroup->GetHandle() : nullptr;
  GetProcs().renderBundleEncoderSetBindGroup(
      GetHandle(), index, bgImpl, dynamicOffsets.size(), dynamicOffsets.data());
}

void GPURenderBundleEncoder::setBindGroup(
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

  WGPUBindGroupImpl* bgImpl = bind_group ? bind_group->GetHandle() : nullptr;
  GetProcs().renderBundleEncoderSetBindGroup(GetHandle(), index, bgImpl,
                                             dynamic_offsets_data_length, data);
}

GPURenderBundle* GPURenderBundleEncoder::finish(
    const GPURenderBundleDescriptor* webgpu_desc) {
  std::string label;
  WGPURenderBundleDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  if (webgpu_desc->hasLabel()) {
    label = webgpu_desc->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  WGPURenderBundle render_bundle =
      GetProcs().renderBundleEncoderFinish(GetHandle(), &dawn_desc);
  return MakeGarbageCollected<GPURenderBundle>(device_, render_bundle);
}

}  // namespace blink
