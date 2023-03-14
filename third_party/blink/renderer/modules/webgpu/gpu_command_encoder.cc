// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_command_encoder.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_command_buffer_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_command_encoder_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pass_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pass_timestamp_write.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_buffer.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_texture.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_color_attachment.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_depth_stencil_attachment.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_timestamp_write.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pass_encoder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_query_set.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pass_encoder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

bool ConvertToDawn(const GPURenderPassColorAttachment* in,
                   WGPURenderPassColorAttachment* out,
                   ExceptionState& exception_state) {
  DCHECK(in);
  DCHECK(out);

  *out = {};
  out->view = in->view()->GetHandle();
  if (in->hasResolveTarget()) {
    out->resolveTarget = in->resolveTarget()->GetHandle();
  }
  if (in->hasClearValue() &&
      !ConvertToDawn(in->clearValue(), &out->clearValue, exception_state)) {
    return false;
  }
  out->loadOp = AsDawnEnum(in->loadOp());
  out->storeOp = AsDawnEnum(in->storeOp());

  return true;
}

WGPUComputePassTimestampWrite AsDawnType(
    const GPUComputePassTimestampWrite* webgpu_desc) {
  DCHECK(webgpu_desc);
  DCHECK(webgpu_desc->querySet());

  WGPUComputePassTimestampWrite dawn_desc = {};
  dawn_desc.querySet = webgpu_desc->querySet()->GetHandle();
  dawn_desc.queryIndex = webgpu_desc->queryIndex();
  dawn_desc.location = AsDawnEnum(webgpu_desc->location());

  return dawn_desc;
}

WGPURenderPassTimestampWrite AsDawnType(
    const GPURenderPassTimestampWrite* webgpu_desc) {
  DCHECK(webgpu_desc);
  DCHECK(webgpu_desc->querySet());

  WGPURenderPassTimestampWrite dawn_desc = {};
  dawn_desc.querySet = webgpu_desc->querySet()->GetHandle();
  dawn_desc.queryIndex = webgpu_desc->queryIndex();
  dawn_desc.location = AsDawnEnum(webgpu_desc->location());

  return dawn_desc;
}

namespace {

WGPURenderPassDepthStencilAttachment AsDawnType(
    GPUDevice* device,
    const GPURenderPassDepthStencilAttachment* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPURenderPassDepthStencilAttachment dawn_desc = {};
  dawn_desc.view = webgpu_desc->view()->GetHandle();

  if (webgpu_desc->hasDepthLoadOp()) {
    dawn_desc.depthLoadOp = AsDawnEnum(webgpu_desc->depthLoadOp());
  }
  // NaN is the default value in Dawn
  dawn_desc.depthClearValue = webgpu_desc->getDepthClearValueOr(
      std::numeric_limits<float>::quiet_NaN());

  if (webgpu_desc->hasDepthStoreOp()) {
    dawn_desc.depthStoreOp = AsDawnEnum(webgpu_desc->depthStoreOp());
  }

  dawn_desc.depthReadOnly = webgpu_desc->depthReadOnly();

  if (webgpu_desc->hasStencilLoadOp()) {
    dawn_desc.stencilLoadOp = AsDawnEnum(webgpu_desc->stencilLoadOp());
    dawn_desc.stencilClearValue = webgpu_desc->stencilClearValue();
  }

  if (webgpu_desc->hasStencilStoreOp()) {
    dawn_desc.stencilStoreOp = AsDawnEnum(webgpu_desc->stencilStoreOp());
  }

  dawn_desc.stencilReadOnly = webgpu_desc->stencilReadOnly();

  return dawn_desc;
}

WGPUImageCopyBuffer ValidateAndConvertImageCopyBuffer(
    const GPUImageCopyBuffer* webgpu_view,
    const char** error) {
  DCHECK(webgpu_view);
  DCHECK(webgpu_view->buffer());

  WGPUImageCopyBuffer dawn_view = {};
  dawn_view.nextInChain = nullptr;
  dawn_view.buffer = webgpu_view->buffer()->GetHandle();

  *error = ValidateTextureDataLayout(webgpu_view, &dawn_view.layout);
  return dawn_view;
}

WGPUCommandEncoderDescriptor AsDawnType(
    const GPUCommandEncoderDescriptor* webgpu_desc,
    std::string* label) {
  DCHECK(webgpu_desc);
  DCHECK(label);

  WGPUCommandEncoderDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  if (webgpu_desc->hasLabel()) {
    *label = webgpu_desc->label().Utf8();
    dawn_desc.label = label->c_str();
  }

  return dawn_desc;
}

}  // anonymous namespace

// static
GPUCommandEncoder* GPUCommandEncoder::Create(
    GPUDevice* device,
    const GPUCommandEncoderDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  std::string label;
  WGPUCommandEncoderDescriptor dawn_desc = AsDawnType(webgpu_desc, &label);

  GPUCommandEncoder* encoder = MakeGarbageCollected<GPUCommandEncoder>(
      device, device->GetProcs().deviceCreateCommandEncoder(device->GetHandle(),
                                                            &dawn_desc));
  if (webgpu_desc->hasLabel())
    encoder->setLabel(webgpu_desc->label());
  return encoder;
}

GPUCommandEncoder::GPUCommandEncoder(GPUDevice* device,
                                     WGPUCommandEncoder command_encoder)
    : DawnObject<WGPUCommandEncoder>(device, command_encoder) {}

GPURenderPassEncoder* GPUCommandEncoder::beginRenderPass(
    const GPURenderPassDescriptor* descriptor,
    ExceptionState& exception_state) {
  DCHECK(descriptor);

  WGPURenderPassDescriptor dawn_desc = {};

  std::string label;
  if (descriptor->hasLabel()) {
    label = descriptor->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  std::unique_ptr<WGPURenderPassColorAttachment[]> color_attachments;
  dawn_desc.colorAttachmentCount = descriptor->colorAttachments().size();
  if (dawn_desc.colorAttachmentCount > 0) {
    if (!ConvertToDawn(descriptor->colorAttachments(), &color_attachments,
                       exception_state)) {
      return nullptr;
    }
    dawn_desc.colorAttachments = color_attachments.get();
  }

  WGPURenderPassDepthStencilAttachment depthStencilAttachment = {};
  if (descriptor->hasDepthStencilAttachment()) {
    const GPURenderPassDepthStencilAttachment* depth_stencil =
        descriptor->depthStencilAttachment();
    depthStencilAttachment = AsDawnType(device_, depth_stencil);
    dawn_desc.depthStencilAttachment = &depthStencilAttachment;
  }

  if (descriptor->hasOcclusionQuerySet()) {
    dawn_desc.occlusionQuerySet = AsDawnType(descriptor->occlusionQuerySet());
  }

  uint32_t timestamp_writes_count =
      static_cast<uint32_t>(descriptor->timestampWrites().size());
  dawn_desc.timestampWriteCount = timestamp_writes_count;
  std::unique_ptr<WGPURenderPassTimestampWrite[]> timestamp_writes;
  if (timestamp_writes_count > 0) {
    V8GPUFeatureName::Enum requiredFeatureEnum =
        V8GPUFeatureName::Enum::kTimestampQuery;
    if (!device_->features()->has(requiredFeatureEnum)) {
      exception_state.ThrowTypeError(
          String::Format("Use of the timestampWrites member in render pass "
                         "descriptor requires the '%s' "
                         "feature to be enabled on %s.",
                         V8GPUFeatureName(requiredFeatureEnum).AsCStr(),
                         device_->formattedLabel().c_str()));
      return nullptr;
    }

    timestamp_writes = AsDawnType(descriptor->timestampWrites());
    dawn_desc.timestampWrites = timestamp_writes.get();
  }

  WGPURenderPassDescriptorMaxDrawCount max_draw_count = {};
  if (descriptor->hasMaxDrawCount()) {
    max_draw_count.chain.sType = WGPUSType_RenderPassDescriptorMaxDrawCount;
    max_draw_count.maxDrawCount = descriptor->maxDrawCount();
    dawn_desc.nextInChain =
        reinterpret_cast<WGPUChainedStruct*>(&max_draw_count);
  }

  GPURenderPassEncoder* encoder = MakeGarbageCollected<GPURenderPassEncoder>(
      device_,
      GetProcs().commandEncoderBeginRenderPass(GetHandle(), &dawn_desc));
  if (descriptor->hasLabel())
    encoder->setLabel(descriptor->label());
  return encoder;
}

GPUComputePassEncoder* GPUCommandEncoder::beginComputePass(
    const GPUComputePassDescriptor* descriptor,
    ExceptionState& exception_state) {
  std::string label;
  WGPUComputePassDescriptor dawn_desc = {};
  if (descriptor->hasLabel()) {
    label = descriptor->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  uint32_t timestamp_writes_count =
      static_cast<uint32_t>(descriptor->timestampWrites().size());
  dawn_desc.timestampWriteCount = timestamp_writes_count;
  std::unique_ptr<WGPUComputePassTimestampWrite[]> timestamp_writes;
  if (timestamp_writes_count > 0) {
    V8GPUFeatureName::Enum requiredFeatureEnum =
        V8GPUFeatureName::Enum::kTimestampQuery;
    if (!device_->features()->has(requiredFeatureEnum)) {
      exception_state.ThrowTypeError(
          String::Format("Use of the timestampWrites member in compute pass "
                         "descriptor requires the '%s' "
                         "feature to be enabled on %s.",
                         V8GPUFeatureName(requiredFeatureEnum).AsCStr(),
                         device_->formattedLabel().c_str()));
      return nullptr;
    }

    timestamp_writes = AsDawnType(descriptor->timestampWrites());
    dawn_desc.timestampWrites = timestamp_writes.get();
  } else {
    dawn_desc.timestampWrites = nullptr;
  }

  GPUComputePassEncoder* encoder = MakeGarbageCollected<GPUComputePassEncoder>(
      device_,
      GetProcs().commandEncoderBeginComputePass(GetHandle(), &dawn_desc));
  if (descriptor->hasLabel())
    encoder->setLabel(descriptor->label());
  return encoder;
}

void GPUCommandEncoder::copyBufferToTexture(GPUImageCopyBuffer* source,
                                            GPUImageCopyTexture* destination,
                                            const V8GPUExtent3D* copy_size,
                                            ExceptionState& exception_state) {
  WGPUExtent3D dawn_copy_size;
  WGPUImageCopyTexture dawn_destination;
  if (!ConvertToDawn(copy_size, &dawn_copy_size, exception_state) ||
      !ConvertToDawn(destination, &dawn_destination, exception_state)) {
    return;
  }

  const char* error = nullptr;
  WGPUImageCopyBuffer dawn_source =
      ValidateAndConvertImageCopyBuffer(source, &error);
  if (error) {
    GetProcs().commandEncoderInjectValidationError(GetHandle(), error);
    return;
  }

  GetProcs().commandEncoderCopyBufferToTexture(
      GetHandle(), &dawn_source, &dawn_destination, &dawn_copy_size);
}

void GPUCommandEncoder::copyTextureToBuffer(GPUImageCopyTexture* source,
                                            GPUImageCopyBuffer* destination,
                                            const V8GPUExtent3D* copy_size,
                                            ExceptionState& exception_state) {
  WGPUExtent3D dawn_copy_size;
  WGPUImageCopyTexture dawn_source;
  if (!ConvertToDawn(copy_size, &dawn_copy_size, exception_state) ||
      !ConvertToDawn(source, &dawn_source, exception_state)) {
    return;
  }

  const char* error = nullptr;
  WGPUImageCopyBuffer dawn_destination =
      ValidateAndConvertImageCopyBuffer(destination, &error);
  if (error) {
    GetProcs().commandEncoderInjectValidationError(GetHandle(), error);
    return;
  }

  GetProcs().commandEncoderCopyTextureToBuffer(
      GetHandle(), &dawn_source, &dawn_destination, &dawn_copy_size);
}

void GPUCommandEncoder::copyTextureToTexture(GPUImageCopyTexture* source,
                                             GPUImageCopyTexture* destination,
                                             const V8GPUExtent3D* copy_size,
                                             ExceptionState& exception_state) {
  WGPUExtent3D dawn_copy_size;
  WGPUImageCopyTexture dawn_source;
  WGPUImageCopyTexture dawn_destination;
  if (!ConvertToDawn(copy_size, &dawn_copy_size, exception_state) ||
      !ConvertToDawn(source, &dawn_source, exception_state) ||
      !ConvertToDawn(destination, &dawn_destination, exception_state)) {
    return;
  }

  GetProcs().commandEncoderCopyTextureToTexture(
      GetHandle(), &dawn_source, &dawn_destination, &dawn_copy_size);
}

void GPUCommandEncoder::writeTimestamp(DawnObject<WGPUQuerySet>* querySet,
                                       uint32_t queryIndex,
                                       ExceptionState& exception_state) {
  V8GPUFeatureName::Enum requiredFeatureEnum =
      V8GPUFeatureName::Enum::kTimestampQuery;
  if (!device_->features()->has(requiredFeatureEnum)) {
    exception_state.ThrowTypeError(
        String::Format("Use of the writeTimestamp() method requires the '%s' "
                       "feature to be enabled on %s.",
                       V8GPUFeatureName(requiredFeatureEnum).AsCStr(),
                       device_->formattedLabel().c_str()));
    return;
  }
  GetProcs().commandEncoderWriteTimestamp(GetHandle(), querySet->GetHandle(),
                                          queryIndex);
}

GPUCommandBuffer* GPUCommandEncoder::finish(
    const GPUCommandBufferDescriptor* descriptor) {
  std::string label;
  WGPUCommandBufferDescriptor dawn_desc = {};
  if (descriptor->hasLabel()) {
    label = descriptor->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  GPUCommandBuffer* command_buffer = MakeGarbageCollected<GPUCommandBuffer>(
      device_, GetProcs().commandEncoderFinish(GetHandle(), &dawn_desc));
  if (descriptor->hasLabel()) {
    command_buffer->setLabel(descriptor->label());
  }

  return command_buffer;
}

}  // namespace blink
