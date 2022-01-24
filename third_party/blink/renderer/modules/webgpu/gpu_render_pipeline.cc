// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_blend_component.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_blend_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_color_state_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_color_target_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_depth_stencil_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_depth_stencil_state_descriptor.h"
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

WGPUBlendComponent AsDawnType(const GPUBlendComponent* webgpu_desc,
                              GPUDevice* device) {
  DCHECK(webgpu_desc);

  WGPUBlendComponent dawn_desc = {};
  dawn_desc.dstFactor = AsDawnEnum<WGPUBlendFactor>(webgpu_desc->dstFactor());
  dawn_desc.srcFactor = AsDawnEnum<WGPUBlendFactor>(webgpu_desc->srcFactor());
  dawn_desc.operation =
      AsDawnEnum<WGPUBlendOperation>(webgpu_desc->operation());

  return dawn_desc;
}

WGPUBlendState AsDawnType(const GPUBlendState* webgpu_desc, GPUDevice* device) {
  DCHECK(webgpu_desc);

  WGPUBlendState dawn_desc = {};
  dawn_desc.color = AsDawnType(webgpu_desc->color(), device);
  dawn_desc.alpha = AsDawnType(webgpu_desc->alpha(), device);

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
      AsDawnEnum<WGPUColorWriteMask>(webgpu_desc->writeMask());
  dawn_desc.format = AsDawnEnum<WGPUTextureFormat>(webgpu_desc->format());

  return dawn_desc;
}

namespace {

WGPUStencilFaceState AsDawnType(const GPUStencilFaceState* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUStencilFaceState dawn_desc = {};
  dawn_desc.compare = AsDawnEnum<WGPUCompareFunction>(webgpu_desc->compare());
  dawn_desc.depthFailOp =
      AsDawnEnum<WGPUStencilOperation>(webgpu_desc->depthFailOp());
  dawn_desc.failOp = AsDawnEnum<WGPUStencilOperation>(webgpu_desc->failOp());
  dawn_desc.passOp = AsDawnEnum<WGPUStencilOperation>(webgpu_desc->passOp());

  return dawn_desc;
}

void GPUPrimitiveStateAsWGPUPrimitiveState(
    const GPUPrimitiveState* webgpu_desc, OwnedPrimitiveState* dawn_state) {
  DCHECK(webgpu_desc);
  DCHECK(dawn_state);

  dawn_state->dawn_desc.nextInChain = nullptr;
  dawn_state->dawn_desc.topology =
      AsDawnEnum<WGPUPrimitiveTopology>(webgpu_desc->topology());
  if (webgpu_desc->hasStripIndexFormat()) {
    dawn_state->dawn_desc.stripIndexFormat =
        AsDawnEnum<WGPUIndexFormat>(webgpu_desc->stripIndexFormat());
  }
  dawn_state->dawn_desc.frontFace =
      AsDawnEnum<WGPUFrontFace>(webgpu_desc->frontFace());
  dawn_state->dawn_desc.cullMode =
      AsDawnEnum<WGPUCullMode>(webgpu_desc->cullMode());

  if (webgpu_desc->hasClampDepth()) {
    auto* clamp_state = &dawn_state->depth_clamping_state;
    clamp_state->chain.sType = WGPUSType_PrimitiveDepthClampingState;
    clamp_state->clampDepth = webgpu_desc->clampDepth().has_value() &&
                              webgpu_desc->clampDepth().value();
    dawn_state->dawn_desc.nextInChain =
        reinterpret_cast<WGPUChainedStruct*>(clamp_state);
  }
}

WGPUDepthStencilState AsDawnType(const GPUDepthStencilState* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUDepthStencilState dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.format = AsDawnEnum<WGPUTextureFormat>(webgpu_desc->format());
  dawn_desc.depthWriteEnabled = webgpu_desc->depthWriteEnabled();
  dawn_desc.depthCompare =
      AsDawnEnum<WGPUCompareFunction>(webgpu_desc->depthCompare());
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

void AsDawnVertexBufferLayouts(
    v8::Isolate* isolate,
    GPUDevice* device,
    v8::Local<v8::Value> vertex_buffers_value,
    Vector<WGPUVertexBufferLayout>* dawn_vertex_buffers,
    Vector<WGPUVertexAttribute>* dawn_vertex_attributes,
    ExceptionState& exception_state) {
  if (!vertex_buffers_value->IsArray()) {
    exception_state.ThrowTypeError("vertexBuffers must be an array");
    return;
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> vertex_buffers = vertex_buffers_value.As<v8::Array>();

  // First we collect all the descriptors but we don't set
  // WGPUVertexBufferLayout::attributes
  // TODO(cwallez@chromium.org): Should we validate the Length() first so we
  // don't risk creating HUGE vectors of WGPUVertexBufferLayout from
  // the sparse array?
  for (uint32_t i = 0; i < vertex_buffers->Length(); ++i) {
    // This array can be sparse. Skip empty slots.
    v8::MaybeLocal<v8::Value> maybe_value = vertex_buffers->Get(context, i);
    v8::Local<v8::Value> value;
    if (!maybe_value.ToLocal(&value) || value.IsEmpty() ||
        value->IsNullOrUndefined()) {
      WGPUVertexBufferLayout dawn_vertex_buffer = {};
      dawn_vertex_buffer.arrayStride = 0;
      dawn_vertex_buffer.stepMode = WGPUVertexStepMode_Vertex;
      dawn_vertex_buffer.attributeCount = 0;
      dawn_vertex_buffer.attributes = nullptr;
      dawn_vertex_buffers->push_back(dawn_vertex_buffer);
      continue;
    }

    GPUVertexBufferLayout* vertex_buffer =
        NativeValueTraits<GPUVertexBufferLayout>::NativeValue(isolate, value,
                                                              exception_state);
    if (exception_state.HadException()) {
      return;
    }

    WGPUVertexBufferLayout dawn_vertex_buffer = {};
    dawn_vertex_buffer.arrayStride = vertex_buffer->arrayStride();
    dawn_vertex_buffer.stepMode =
        AsDawnEnum<WGPUVertexStepMode>(vertex_buffer->stepMode());
    dawn_vertex_buffer.attributeCount =
        static_cast<uint32_t>(vertex_buffer->attributes().size());
    dawn_vertex_buffer.attributes = nullptr;
    dawn_vertex_buffers->push_back(dawn_vertex_buffer);

    for (wtf_size_t j = 0; j < vertex_buffer->attributes().size(); ++j) {
      const GPUVertexAttribute* attribute = vertex_buffer->attributes()[j];
      WGPUVertexAttribute dawn_vertex_attribute = {};
      dawn_vertex_attribute.shaderLocation = attribute->shaderLocation();
      dawn_vertex_attribute.offset = attribute->offset();
      dawn_vertex_attribute.format =
          AsDawnEnum<WGPUVertexFormat>(attribute->format());
      dawn_vertex_attributes->push_back(dawn_vertex_attribute);
    }
  }

  // Set up pointers in DawnVertexBufferLayout::attributes only
  // after we stopped appending to the vector so the pointers aren't
  // invalidated.
  uint32_t attributeIndex = 0;
  for (WGPUVertexBufferLayout& buffer : *dawn_vertex_buffers) {
    if (buffer.attributeCount == 0) {
      continue;
    }
    buffer.attributes = &(*dawn_vertex_attributes)[attributeIndex];
    attributeIndex += buffer.attributeCount;
  }
}

void GPUFragmentStateAsWGPUFragmentState(GPUDevice* device,
                                         const GPUFragmentState* descriptor,
                                         OwnedFragmentState* dawn_fragment) {
  DCHECK(descriptor);
  DCHECK(dawn_fragment);

  dawn_fragment->dawn_desc = {};
  dawn_fragment->dawn_desc.nextInChain = nullptr;
  dawn_fragment->dawn_desc.module = descriptor->module()->GetHandle();

  dawn_fragment->entry_point = descriptor->entryPoint().Ascii();
  dawn_fragment->dawn_desc.entryPoint = dawn_fragment->entry_point.c_str();

  dawn_fragment->targets = AsDawnType(descriptor->targets());
  dawn_fragment->dawn_desc.targetCount =
      static_cast<uint32_t>(descriptor->targets().size());
  dawn_fragment->dawn_desc.targets = dawn_fragment->targets.get();

  // In order to maintain proper ownership we have to process the blend states
  // for each target outside of AsDawnType().
  // ReserveCapacity beforehand to make sure our pointers within the vector
  // stay stable.
  dawn_fragment->blend_states.resize(descriptor->targets().size());
  for (wtf_size_t i = 0; i < descriptor->targets().size(); ++i) {
    const GPUColorTargetState* color_target = descriptor->targets()[i];
    if (color_target->hasBlend()) {
      dawn_fragment->blend_states[i] =
          AsDawnType(color_target->blend(), device);
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
  if (webgpu_desc->hasLayout()) {
    dawn_desc_info->dawn_desc.layout = AsDawnType(webgpu_desc->layout());
  }

  // Vertex
  const GPUVertexState* vertex = webgpu_desc->vertex();
  WGPUVertexState* dawn_vertex = &dawn_desc_info->dawn_desc.vertex;
  *dawn_vertex = {};

  dawn_vertex->module = vertex->module()->GetHandle();

  dawn_desc_info->vertex_entry_point = vertex->entryPoint().Ascii();
  dawn_vertex->entryPoint = dawn_desc_info->vertex_entry_point.c_str();

  if (vertex->hasBuffers()) {
    // TODO(crbug.com/951629): Use a sequence of nullable descriptors.
    v8::Local<v8::Value> buffers_value = vertex->buffers().V8Value();
    AsDawnVertexBufferLayouts(isolate, device, buffers_value,
                              &dawn_desc_info->buffers,
                              &dawn_desc_info->attributes, exception_state);
    if (exception_state.HadException()) {
      return;
    }

    dawn_vertex->bufferCount =
        static_cast<uint32_t>(dawn_desc_info->buffers.size());
    dawn_vertex->buffers = dawn_desc_info->buffers.data();
  }

  // Primitive
  GPUPrimitiveStateAsWGPUPrimitiveState(
      webgpu_desc->primitive(), &dawn_desc_info->primitive);
  dawn_desc_info->dawn_desc.primitive = dawn_desc_info->primitive.dawn_desc;

  // DepthStencil
  if (webgpu_desc->hasDepthStencil()) {
    dawn_desc_info->depth_stencil = AsDawnType(webgpu_desc->depthStencil());
    dawn_desc_info->dawn_desc.depthStencil = &dawn_desc_info->depth_stencil;
  }

  // Multisample
  dawn_desc_info->dawn_desc.multisample =
      AsDawnType(webgpu_desc->multisample());

  // Fragment
  if (webgpu_desc->hasFragment()) {
    GPUFragmentStateAsWGPUFragmentState(device, webgpu_desc->fragment(),
                                        &dawn_desc_info->fragment);
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
  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 "GPUVertexStateDescriptor");

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
