// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_blend_component.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_blend_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_color_state_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_color_target_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_depth_stencil_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_fragment_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_multisample_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_primitive_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_rasterization_state_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pipeline_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_stencil_face_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_vertex_attribute.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_vertex_buffer_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_vertex_state.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

const char kGPUBlendComponentPartiallySpecifiedMessage[] =
    "fragment.targets[%u].blend.%s has a mix of explicit and defaulted "
    "members, which is unusual. Did you mean to specify other members?";

WGPUBlendComponent AsDawnType(const GPUBlendComponent* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUBlendComponent dawn_desc = {};
  dawn_desc.dstFactor = AsDawnEnum(webgpu_desc->getDstFactorOr(
      V8GPUBlendFactor(V8GPUBlendFactor::Enum::kZero)));
  dawn_desc.srcFactor = AsDawnEnum(webgpu_desc->getSrcFactorOr(
      V8GPUBlendFactor(V8GPUBlendFactor::Enum::kOne)));
  dawn_desc.operation = AsDawnEnum(webgpu_desc->getOperationOr(
      V8GPUBlendOperation(V8GPUBlendOperation::Enum::kAdd)));

  return dawn_desc;
}

WGPUBlendState AsDawnType(const GPUBlendState* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUBlendState dawn_desc = {};
  dawn_desc.color = AsDawnType(webgpu_desc->color());
  dawn_desc.alpha = AsDawnType(webgpu_desc->alpha());

  return dawn_desc;
}

}  // anonymous namespace

WGPUColorTargetState AsDawnType(const GPUColorTargetState* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUColorTargetState dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  // Blend is handled in ConvertToDawnType
  dawn_desc.blend = nullptr;
  dawn_desc.writeMask =
      AsDawnFlags<WGPUColorWriteMask>(webgpu_desc->writeMask());
  dawn_desc.format = AsDawnEnum(webgpu_desc->format());

  return dawn_desc;
}

WGPUVertexBufferLayout AsDawnType(const GPUVertexBufferLayout* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUVertexBufferLayout dawn_desc = {};
  dawn_desc.arrayStride = webgpu_desc->arrayStride();
  dawn_desc.stepMode = AsDawnEnum(webgpu_desc->stepMode());
  dawn_desc.attributeCount = webgpu_desc->attributes().size();

  // dawn_desc.attributes is handled outside separately

  return dawn_desc;
}

WGPUVertexAttribute AsDawnType(const GPUVertexAttribute* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUVertexAttribute dawn_desc = {};
  dawn_desc.shaderLocation = webgpu_desc->shaderLocation();
  dawn_desc.offset = webgpu_desc->offset();
  dawn_desc.format = AsDawnEnum(webgpu_desc->format());

  return dawn_desc;
}

namespace {

WGPUStencilFaceState AsDawnType(const GPUStencilFaceState* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUStencilFaceState dawn_desc = {};
  dawn_desc.compare = AsDawnEnum(webgpu_desc->compare());
  dawn_desc.depthFailOp = AsDawnEnum(webgpu_desc->depthFailOp());
  dawn_desc.failOp = AsDawnEnum(webgpu_desc->failOp());
  dawn_desc.passOp = AsDawnEnum(webgpu_desc->passOp());

  return dawn_desc;
}

void GPUPrimitiveStateAsWGPUPrimitiveState(
    const GPUPrimitiveState* webgpu_desc, OwnedPrimitiveState* dawn_state) {
  DCHECK(webgpu_desc);
  DCHECK(dawn_state);

  dawn_state->dawn_desc.nextInChain = nullptr;
  dawn_state->dawn_desc.topology = AsDawnEnum(webgpu_desc->topology());
  if (webgpu_desc->hasStripIndexFormat()) {
    dawn_state->dawn_desc.stripIndexFormat =
        AsDawnEnum(webgpu_desc->stripIndexFormat());
  }
  dawn_state->dawn_desc.frontFace = AsDawnEnum(webgpu_desc->frontFace());
  dawn_state->dawn_desc.cullMode = AsDawnEnum(webgpu_desc->cullMode());

  if (webgpu_desc->unclippedDepth()) {
    auto* depth_clip_control = &dawn_state->depth_clip_control;
    depth_clip_control->chain.sType = WGPUSType_PrimitiveDepthClipControl;
    depth_clip_control->unclippedDepth = webgpu_desc->unclippedDepth();
    dawn_state->dawn_desc.nextInChain =
        reinterpret_cast<WGPUChainedStruct*>(depth_clip_control);
  }
}

WGPUDepthStencilState AsDawnType(GPUDevice* device,
                                 const GPUDepthStencilState* webgpu_desc,
                                 ExceptionState& exception_state) {
  DCHECK(webgpu_desc);

  if (!device->ValidateTextureFormatUsage(webgpu_desc->format(),
                                          exception_state)) {
    return {};
  }

  WGPUDepthStencilState dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.format = AsDawnEnum(webgpu_desc->format());
  dawn_desc.depthWriteEnabled = webgpu_desc->depthWriteEnabled();
  dawn_desc.depthCompare = AsDawnEnum(webgpu_desc->depthCompare());
  dawn_desc.stencilFront = AsDawnType(webgpu_desc->stencilFront());
  dawn_desc.stencilBack = AsDawnType(webgpu_desc->stencilBack());
  dawn_desc.stencilReadMask = webgpu_desc->stencilReadMask();
  dawn_desc.stencilWriteMask = webgpu_desc->stencilWriteMask();
  dawn_desc.depthBias = webgpu_desc->depthBias();
  dawn_desc.depthBiasSlopeScale = webgpu_desc->depthBiasSlopeScale();
  dawn_desc.depthBiasClamp = webgpu_desc->depthBiasClamp();

  return dawn_desc;
}

WGPUMultisampleState AsDawnType(const GPUMultisampleState* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUMultisampleState dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.count = webgpu_desc->count();
  dawn_desc.mask = webgpu_desc->mask();
  dawn_desc.alphaToCoverageEnabled = webgpu_desc->alphaToCoverageEnabled();

  return dawn_desc;
}

void AsDawnVertexBufferLayouts(GPUDevice* device,
                               const GPUVertexState* descriptor,
                               OwnedVertexState* dawn_desc_info) {
  DCHECK(descriptor);
  DCHECK(dawn_desc_info);

  WGPUVertexState* dawn_vertex = dawn_desc_info->dawn_desc;
  dawn_vertex->bufferCount = descriptor->buffers().size();

  if (dawn_vertex->bufferCount == 0) {
    dawn_vertex->buffers = nullptr;
    return;
  }

  // TODO(cwallez@chromium.org): Should we validate the Length() first so we
  // don't risk creating HUGE vectors of WGPUVertexBufferLayout from
  // the sparse array?
  dawn_desc_info->buffers = AsDawnType(descriptor->buffers());
  dawn_vertex->buffers = dawn_desc_info->buffers.get();

  // Handle WGPUVertexBufferLayout::attributes separately to guarantee the
  // lifetime.
  dawn_desc_info->attributes =
      std::make_unique<std::unique_ptr<WGPUVertexAttribute[]>[]>(
          dawn_vertex->bufferCount);
  for (wtf_size_t i = 0; i < dawn_vertex->bufferCount; ++i) {
    const auto& maybe_buffer = descriptor->buffers()[i];
    if (!maybe_buffer) {
      // This buffer layout is empty.
      // Explicitly set VertexBufferNotUsed step mode to represent
      // this slot is empty for Dawn, and continue the loop.
      dawn_desc_info->buffers[i].stepMode =
          WGPUVertexStepMode::WGPUVertexStepMode_VertexBufferNotUsed;
      continue;
    }
    const GPUVertexBufferLayout* buffer = maybe_buffer.Get();
    dawn_desc_info->attributes.get()[i] = AsDawnType(buffer->attributes());
    WGPUVertexBufferLayout* dawn_buffer = &dawn_desc_info->buffers[i];
    dawn_buffer->attributes = dawn_desc_info->attributes.get()[i].get();
  }
}

void GPUVertexStateAsWGPUVertexState(GPUDevice* device,
                                     const GPUVertexState* descriptor,
                                     OwnedVertexState* dawn_vertex) {
  DCHECK(descriptor);
  DCHECK(dawn_vertex);

  *dawn_vertex->dawn_desc = {};
  dawn_vertex->dawn_desc->nextInChain = nullptr;
  GPUProgrammableStageAsWGPUProgrammableStage(descriptor, dawn_vertex);
  dawn_vertex->dawn_desc->constantCount = dawn_vertex->constantCount;
  dawn_vertex->dawn_desc->constants = dawn_vertex->constants.get();
  dawn_vertex->dawn_desc->module = descriptor->module()->GetHandle();
  dawn_vertex->dawn_desc->entryPoint = dawn_vertex->entry_point.c_str();

  if (descriptor->hasBuffers()) {
    AsDawnVertexBufferLayouts(device, descriptor, dawn_vertex);
  }
}

bool IsGPUBlendComponentPartiallySpecified(
    const GPUBlendComponent* webgpu_desc) {
  DCHECK(webgpu_desc);
  // GPUBlendComponent is considered partially specified when:
  // - srcFactor is missing but operation or dstFactor is provided
  // - dstFactor is missing but operation or srcFactor is provided
  return ((!webgpu_desc->hasSrcFactor() &&
           (webgpu_desc->hasDstFactor() || webgpu_desc->hasOperation())) ||
          (!webgpu_desc->hasDstFactor() &&
           (webgpu_desc->hasSrcFactor() || webgpu_desc->hasOperation())));
}

void GPUFragmentStateAsWGPUFragmentState(GPUDevice* device,
                                         const GPUFragmentState* descriptor,
                                         OwnedFragmentState* dawn_fragment,
                                         ExceptionState& exception_state) {
  DCHECK(descriptor);
  DCHECK(dawn_fragment);

  dawn_fragment->dawn_desc = {};
  dawn_fragment->dawn_desc.nextInChain = nullptr;

  GPUProgrammableStageAsWGPUProgrammableStage(descriptor, dawn_fragment);
  dawn_fragment->dawn_desc.constantCount = dawn_fragment->constantCount;
  dawn_fragment->dawn_desc.constants = dawn_fragment->constants.get();
  dawn_fragment->dawn_desc.module = descriptor->module()->GetHandle();
  dawn_fragment->dawn_desc.entryPoint = dawn_fragment->entry_point.c_str();

  dawn_fragment->dawn_desc.targets = nullptr;
  dawn_fragment->dawn_desc.targetCount = descriptor->targets().size();
  if (dawn_fragment->dawn_desc.targetCount > 0) {
    dawn_fragment->targets = AsDawnType(descriptor->targets());
    dawn_fragment->dawn_desc.targets = dawn_fragment->targets.get();
  }

  // In order to maintain proper ownership we have to process the blend states
  // for each target outside of AsDawnType().
  // ReserveCapacity beforehand to make sure our pointers within the vector
  // stay stable.
  dawn_fragment->blend_states.resize(descriptor->targets().size());
  for (wtf_size_t i = 0; i < descriptor->targets().size(); ++i) {
    const auto& maybe_color_target = descriptor->targets()[i];
    if (!maybe_color_target) {
      continue;
    }
    const GPUColorTargetState* color_target = maybe_color_target.Get();
    if (!device->ValidateTextureFormatUsage(color_target->format(),
                                            exception_state)) {
      return;
    }
    if (color_target->hasBlend()) {
      const GPUBlendState* blend_state = color_target->blend();
      if (IsGPUBlendComponentPartiallySpecified(blend_state->color())) {
        device->AddConsoleWarning(String::Format(
            kGPUBlendComponentPartiallySpecifiedMessage, i, "color"));
      }
      if (IsGPUBlendComponentPartiallySpecified(blend_state->alpha())) {
        device->AddConsoleWarning(String::Format(
            kGPUBlendComponentPartiallySpecifiedMessage, i, "alpha"));
      }
      dawn_fragment->blend_states[i] = AsDawnType(blend_state);
      dawn_fragment->targets[i].blend = &dawn_fragment->blend_states[i];
    }
  }
}

}  // anonymous namespace

void ConvertToDawnType(v8::Isolate* isolate,
                       GPUDevice* device,
                       const GPURenderPipelineDescriptor* webgpu_desc,
                       OwnedRenderPipelineDescriptor* dawn_desc_info,
                       ExceptionState& exception_state) {
  DCHECK(isolate);
  DCHECK(webgpu_desc);
  DCHECK(dawn_desc_info);

  // Label
  if (webgpu_desc->hasLabel()) {
    dawn_desc_info->label = webgpu_desc->label().Utf8();
    dawn_desc_info->dawn_desc.label = dawn_desc_info->label.c_str();
  }

  // Layout
  dawn_desc_info->dawn_desc.layout = AsDawnType(webgpu_desc->layout());

  // Vertex
  const GPUVertexState* vertex = webgpu_desc->vertex();
  OwnedVertexState* dawn_vertex = &dawn_desc_info->vertex;
  dawn_vertex->dawn_desc = &dawn_desc_info->dawn_desc.vertex;
  GPUVertexStateAsWGPUVertexState(device, vertex, dawn_vertex);

  // Primitive
  GPUPrimitiveStateAsWGPUPrimitiveState(
      webgpu_desc->primitive(), &dawn_desc_info->primitive);
  dawn_desc_info->dawn_desc.primitive = dawn_desc_info->primitive.dawn_desc;

  // DepthStencil
  if (webgpu_desc->hasDepthStencil()) {
    dawn_desc_info->depth_stencil =
        AsDawnType(device, webgpu_desc->depthStencil(), exception_state);
    dawn_desc_info->dawn_desc.depthStencil = &dawn_desc_info->depth_stencil;
  }

  // Multisample
  dawn_desc_info->dawn_desc.multisample =
      AsDawnType(webgpu_desc->multisample());

  // Fragment
  if (webgpu_desc->hasFragment()) {
    GPUFragmentStateAsWGPUFragmentState(device, webgpu_desc->fragment(),
                                        &dawn_desc_info->fragment,
                                        exception_state);
    dawn_desc_info->dawn_desc.fragment = &dawn_desc_info->fragment.dawn_desc;
  }
}

// static
GPURenderPipeline* GPURenderPipeline::Create(
    ScriptState* script_state,
    GPUDevice* device,
    const GPURenderPipelineDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  v8::Isolate* isolate = script_state->GetIsolate();
  ExceptionState exception_state(
      isolate, ExceptionContextType::kConstructorOperationInvoke,
      "GPURenderPipeline");

  GPURenderPipeline* pipeline;
  OwnedRenderPipelineDescriptor dawn_desc_info;
  ConvertToDawnType(isolate, device, webgpu_desc, &dawn_desc_info,
                    exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  pipeline = MakeGarbageCollected<GPURenderPipeline>(
      device, device->GetProcs().deviceCreateRenderPipeline(
                  device->GetHandle(), &dawn_desc_info.dawn_desc));
  if (webgpu_desc->hasLabel())
    pipeline->setLabel(webgpu_desc->label());
  return pipeline;
}

GPURenderPipeline::GPURenderPipeline(GPUDevice* device,
                                     WGPURenderPipeline render_pipeline)
    : DawnObject<WGPURenderPipeline>(device, render_pipeline) {}

GPUBindGroupLayout* GPURenderPipeline::getBindGroupLayout(uint32_t index) {
  return MakeGarbageCollected<GPUBindGroupLayout>(
      device_, GetProcs().renderPipelineGetBindGroupLayout(GetHandle(), index));
}

}  // namespace blink
