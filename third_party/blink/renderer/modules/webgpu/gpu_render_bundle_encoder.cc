// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle_encoder.h"

#include "base/containers/heap_array.h"
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

  base::HeapArray<wgpu::TextureFormat> color_formats =
      AsDawnEnum<wgpu::TextureFormat>(webgpu_desc->colorFormats());

  wgpu::TextureFormat depth_stencil_format = wgpu::TextureFormat::Undefined;
  if (webgpu_desc->hasDepthStencilFormat()) {
    if (!device->ValidateTextureFormatUsage(webgpu_desc->depthStencilFormat(),
                                            exception_state)) {
      return nullptr;
    }

    depth_stencil_format = AsDawnEnum(webgpu_desc->depthStencilFormat());
  }

  wgpu::RenderBundleEncoderDescriptor dawn_desc = {
      .colorFormatCount = color_formats_count,
      .colorFormats = color_formats.data(),
      .depthStencilFormat = depth_stencil_format,
      .sampleCount = webgpu_desc->sampleCount(),
      .depthReadOnly = webgpu_desc->depthReadOnly(),
      .stencilReadOnly = webgpu_desc->stencilReadOnly(),
  };
  std::string label = webgpu_desc->label().Utf8();
  if (!label.empty()) {
    dawn_desc.label = label.c_str();
  }

  GPURenderBundleEncoder* encoder =
      MakeGarbageCollected<GPURenderBundleEncoder>(
          device, device->GetHandle().CreateRenderBundleEncoder(&dawn_desc),
          webgpu_desc->label());
  return encoder;
}

GPURenderBundleEncoder::GPURenderBundleEncoder(
    GPUDevice* device,
    wgpu::RenderBundleEncoder render_bundle_encoder,
    const String& label)
    : DawnObject<wgpu::RenderBundleEncoder>(device,
                                            render_bundle_encoder,
                                            label) {}

void GPURenderBundleEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bindGroup,
    const Vector<uint32_t>& dynamicOffsets) {
  GetHandle().SetBindGroup(
      index, bindGroup ? bindGroup->GetHandle() : wgpu::BindGroup(nullptr),
      dynamicOffsets.size(), dynamicOffsets.data());
}

void GPURenderBundleEncoder::setBindGroup(
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

GPURenderBundle* GPURenderBundleEncoder::finish(
    const GPURenderBundleDescriptor* webgpu_desc) {
  wgpu::RenderBundleDescriptor dawn_desc = {};
  std::string label = webgpu_desc->label().Utf8();
  if (!label.empty()) {
    dawn_desc.label = label.c_str();
  }

  return MakeGarbageCollected<GPURenderBundle>(
      device_, GetHandle().Finish(&dawn_desc), webgpu_desc->label());
}

}  // namespace blink
