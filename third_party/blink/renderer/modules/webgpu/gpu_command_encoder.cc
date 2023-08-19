// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_command_encoder.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_command_buffer_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_command_encoder_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pass_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pass_timestamp_write.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pass_timestamp_writes.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_buffer.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_texture.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_color_attachment.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_depth_stencil_attachment.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_timestamp_write.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_timestamp_writes.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpucomputepasstimestampwritesequence_gpucomputepasstimestampwrites.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpurenderpasstimestampwritesequence_gpurenderpasstimestampwrites.h"
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

namespace {

// TODO(dawn:1800): Remove after a deprecation period;
using ComputeTimestampWrites =
    V8UnionGPUComputePassTimestampWriteSequenceOrGPUComputePassTimestampWrites;
using RenderTimestampWrites =
    V8UnionGPURenderPassTimestampWriteSequenceOrGPURenderPassTimestampWrites;

// TODO(dawn:1800): Remove after a deprecation period;
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

// TODO(dawn:1800): Remove after a deprecation period;
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

  std::vector<WGPURenderPassTimestampWrite> dawn_timestamp_writes;
  if (descriptor->hasTimestampWrites()) {
    V8GPUFeatureName::Enum requiredFeatureEnum =
        V8GPUFeatureName::Enum::kTimestampQuery;

    if (descriptor->timestampWrites()->GetContentType() ==
        RenderTimestampWrites::ContentType::
            kGPURenderPassTimestampWriteSequence) {
      // TODO(dawn:1800): Remove this branch after a deprecation period;
      device_->AddSingletonWarning(GPUSingletonWarning::kTimestampArray);

      auto timestamp_sequence =
          descriptor->timestampWrites()
              ->GetAsGPURenderPassTimestampWriteSequence();

      uint32_t timestamp_writes_count = timestamp_sequence.size();
      for (uint32_t i = 0; i < timestamp_writes_count; ++i) {
        dawn_timestamp_writes.push_back(
            AsDawnType(timestamp_sequence[i].Get()));
      }

      if (timestamp_writes_count > 0 &&
          !device_->features()->has(requiredFeatureEnum)) {
        exception_state.ThrowTypeError(
            String::Format("Use of the timestampWrites member in compute pass "
                           "descriptor requires the '%s' "
                           "feature to be enabled on %s.",
                           V8GPUFeatureName(requiredFeatureEnum).AsCStr(),
                           device_->formattedLabel().c_str()));
        return nullptr;
      }
    } else {
      if (!device_->features()->has(requiredFeatureEnum)) {
        exception_state.ThrowTypeError(
            String::Format("Use of the timestampWrites member in compute pass "
                           "descriptor requires the '%s' "
                           "feature to be enabled on %s.",
                           V8GPUFeatureName(requiredFeatureEnum).AsCStr(),
                           device_->formattedLabel().c_str()));
        return nullptr;
      }

      GPURenderPassTimestampWrites* timestamp_writes =
          descriptor->timestampWrites()->GetAsGPURenderPassTimestampWrites();

      if (timestamp_writes->hasBeginningOfPassWriteIndex()) {
        WGPURenderPassTimestampWrite begin_write = {};
        begin_write.querySet = timestamp_writes->querySet()->GetHandle();
        begin_write.queryIndex = timestamp_writes->beginningOfPassWriteIndex();
        begin_write.location = WGPURenderPassTimestampLocation_Beginning;
        dawn_timestamp_writes.push_back(begin_write);
      }

      if (timestamp_writes->hasEndOfPassWriteIndex()) {
        WGPURenderPassTimestampWrite end_write = {};
        end_write.querySet = timestamp_writes->querySet()->GetHandle();
        end_write.queryIndex = timestamp_writes->endOfPassWriteIndex();
        end_write.location = WGPURenderPassTimestampLocation_End;
        dawn_timestamp_writes.push_back(end_write);
      }

      if (dawn_timestamp_writes.size() == 0) {
        GetProcs().commandEncoderInjectValidationError(
            GetHandle(),
            "If timestampWrites is specified at least one of "
            "beginningOfPassWriteIndex or endOfPassWriteIndex must be given.");
      }
    }

    dawn_desc.timestampWrites = dawn_timestamp_writes.data();
    dawn_desc.timestampWriteCount = dawn_timestamp_writes.size();
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

  std::vector<WGPUComputePassTimestampWrite> dawn_timestamp_writes;
  if (descriptor->hasTimestampWrites()) {
    V8GPUFeatureName::Enum requiredFeatureEnum =
        V8GPUFeatureName::Enum::kTimestampQuery;

    if (descriptor->timestampWrites()->GetContentType() ==
        ComputeTimestampWrites::ContentType::
            kGPUComputePassTimestampWriteSequence) {
      // TODO(dawn:1800): Remove this branch after a deprecation period;
      device_->AddSingletonWarning(GPUSingletonWarning::kTimestampArray);

      auto timestamp_sequence =
          descriptor->timestampWrites()
              ->GetAsGPUComputePassTimestampWriteSequence();

      uint32_t timestamp_writes_count = timestamp_sequence.size();
      for (uint32_t i = 0; i < timestamp_writes_count; ++i) {
        dawn_timestamp_writes.push_back(
            AsDawnType(timestamp_sequence[i].Get()));
      }

      if (timestamp_writes_count > 0 &&
          !device_->features()->has(requiredFeatureEnum)) {
        exception_state.ThrowTypeError(
            String::Format("Use of the timestampWrites member in compute pass "
                           "descriptor requires the '%s' "
                           "feature to be enabled on %s.",
                           V8GPUFeatureName(requiredFeatureEnum).AsCStr(),
                           device_->formattedLabel().c_str()));
        return nullptr;
      }
    } else {
      if (!device_->features()->has(requiredFeatureEnum)) {
        exception_state.ThrowTypeError(
            String::Format("Use of the timestampWrites member in compute pass "
                           "descriptor requires the '%s' "
                           "feature to be enabled on %s.",
                           V8GPUFeatureName(requiredFeatureEnum).AsCStr(),
                           device_->formattedLabel().c_str()));
        return nullptr;
      }

      GPUComputePassTimestampWrites* timestamp_writes =
          descriptor->timestampWrites()->GetAsGPUComputePassTimestampWrites();

      if (timestamp_writes->hasBeginningOfPassWriteIndex()) {
        WGPUComputePassTimestampWrite begin_write = {};
        begin_write.querySet = timestamp_writes->querySet()->GetHandle();
        begin_write.queryIndex = timestamp_writes->beginningOfPassWriteIndex();
        begin_write.location = WGPUComputePassTimestampLocation_Beginning;
        dawn_timestamp_writes.push_back(begin_write);
      }

      if (timestamp_writes->hasEndOfPassWriteIndex()) {
        WGPUComputePassTimestampWrite end_write = {};
        end_write.querySet = timestamp_writes->querySet()->GetHandle();
        end_write.queryIndex = timestamp_writes->endOfPassWriteIndex();
        end_write.location = WGPUComputePassTimestampLocation_End;
        dawn_timestamp_writes.push_back(end_write);
      }

      if (dawn_timestamp_writes.size() == 0) {
        GetProcs().commandEncoderInjectValidationError(
            GetHandle(),
            "If timestampWrites is specified at least one of "
            "beginningOfPassWriteIndex or endOfPassWriteIndex must be given.");
      }
    }

    dawn_desc.timestampWrites = dawn_timestamp_writes.data();
    dawn_desc.timestampWriteCount = dawn_timestamp_writes.size();
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
  if (!ConvertToDawn(copy_size, &dawn_copy_size, device_, exception_state) ||
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
  if (!ConvertToDawn(copy_size, &dawn_copy_size, device_, exception_state) ||
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
  if (!ConvertToDawn(copy_size, &dawn_copy_size, device_, exception_state) ||
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
