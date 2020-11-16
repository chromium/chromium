// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_blend_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_color_state_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_depth_stencil_state_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_rasterization_state_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pipeline_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_stencil_state_face_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_vertex_attribute_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_vertex_buffer_layout_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_vertex_state_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

WGPUBlendDescriptor AsDawnType(const GPUBlendDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUBlendDescriptor dawn_desc = {};
  dawn_desc.dstFactor = AsDawnEnum<WGPUBlendFactor>(webgpu_desc->dstFactor());
  dawn_desc.srcFactor = AsDawnEnum<WGPUBlendFactor>(webgpu_desc->srcFactor());
  dawn_desc.operation =
      AsDawnEnum<WGPUBlendOperation>(webgpu_desc->operation());

  return dawn_desc;
}

}  // anonymous namespace

WGPUColorStateDescriptor AsDawnType(
    const GPUColorStateDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUColorStateDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.alphaBlend = AsDawnType(webgpu_desc->alphaBlend());
  dawn_desc.colorBlend = AsDawnType(webgpu_desc->colorBlend());
  dawn_desc.writeMask =
      AsDawnEnum<WGPUColorWriteMask>(webgpu_desc->writeMask());
  dawn_desc.format = AsDawnEnum<WGPUTextureFormat>(webgpu_desc->format());

  return dawn_desc;
}

namespace {

WGPUStencilStateFaceDescriptor AsDawnType(
    const GPUStencilStateFaceDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUStencilStateFaceDescriptor dawn_desc = {};
  dawn_desc.compare = AsDawnEnum<WGPUCompareFunction>(webgpu_desc->compare());
  dawn_desc.depthFailOp =
      AsDawnEnum<WGPUStencilOperation>(webgpu_desc->depthFailOp());
  dawn_desc.failOp = AsDawnEnum<WGPUStencilOperation>(webgpu_desc->failOp());
  dawn_desc.passOp = AsDawnEnum<WGPUStencilOperation>(webgpu_desc->passOp());

  return dawn_desc;
}

WGPUDepthStencilStateDescriptor AsDawnType(
    const GPUDepthStencilStateDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUDepthStencilStateDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.depthCompare =
      AsDawnEnum<WGPUCompareFunction>(webgpu_desc->depthCompare());
  dawn_desc.depthWriteEnabled = webgpu_desc->depthWriteEnabled();
  dawn_desc.format = AsDawnEnum<WGPUTextureFormat>(webgpu_desc->format());
  dawn_desc.stencilBack = AsDawnType(webgpu_desc->stencilBack());
  dawn_desc.stencilFront = AsDawnType(webgpu_desc->stencilFront());
  dawn_desc.stencilReadMask = webgpu_desc->stencilReadMask();
  dawn_desc.stencilWriteMask = webgpu_desc->stencilWriteMask();

  return dawn_desc;
}

using WGPUVertexStateInfo = std::tuple<WGPUVertexStateDescriptor,
                                       Vector<WGPUVertexBufferLayoutDescriptor>,
                                       Vector<WGPUVertexAttributeDescriptor>>;

WGPUVertexStateInfo GPUVertexStateAsWGPUVertexState(
    v8::Isolate* isolate,
    const GPUVertexStateDescriptor* descriptor,
    ExceptionState& exception_state) {
  WGPUVertexStateDescriptor dawn_desc = {};
  dawn_desc.indexFormat =
      AsDawnEnum<WGPUIndexFormat>(descriptor->indexFormat());
  dawn_desc.vertexBufferCount = 0;
  dawn_desc.vertexBuffers = nullptr;

  Vector<WGPUVertexBufferLayoutDescriptor> dawn_vertex_buffers;
  Vector<WGPUVertexAttributeDescriptor> dawn_vertex_attributes;

  if (descriptor->hasVertexBuffers()) {
    // TODO(crbug.com/951629): Use a sequence of nullable descriptors.
    v8::Local<v8::Value> vertex_buffers_value =
        descriptor->vertexBuffers().V8Value();
    if (!vertex_buffers_value->IsArray()) {
      exception_state.ThrowTypeError("vertexBuffers must be an array");

      return std::make_tuple(dawn_desc, std::move(dawn_vertex_buffers),
                             std::move(dawn_vertex_attributes));
    }

    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Array> vertex_buffers = vertex_buffers_value.As<v8::Array>();

    // First we collect all the descriptors but we don't set
    // WGPUVertexBufferLayoutDescriptor::attributes
    // TODO(cwallez@chromium.org): Should we validate the Length() first so we
    // don't risk creating HUGE vectors of WGPUVertexBufferLayoutDescriptor from
    // the sparse array?
    for (uint32_t i = 0; i < vertex_buffers->Length(); ++i) {
      // This array can be sparse. Skip empty slots.
      v8::MaybeLocal<v8::Value> maybe_value = vertex_buffers->Get(context, i);
      v8::Local<v8::Value> value;
      if (!maybe_value.ToLocal(&value) || value.IsEmpty() ||
          value->IsNullOrUndefined()) {
        WGPUVertexBufferLayoutDescriptor dawn_vertex_buffer = {};
        dawn_vertex_buffer.arrayStride = 0;
        dawn_vertex_buffer.stepMode = WGPUInputStepMode_Vertex;
        dawn_vertex_buffer.attributeCount = 0;
        dawn_vertex_buffer.attributes = nullptr;
        dawn_vertex_buffers.push_back(dawn_vertex_buffer);
        continue;
      }

      GPUVertexBufferLayoutDescriptor* vertex_buffer =
          NativeValueTraits<GPUVertexBufferLayoutDescriptor>::NativeValue(
              isolate, value, exception_state);
      if (exception_state.HadException()) {
        return std::make_tuple(dawn_desc, std::move(dawn_vertex_buffers),
                               std::move(dawn_vertex_attributes));
      }

      WGPUVertexBufferLayoutDescriptor dawn_vertex_buffer = {};
      dawn_vertex_buffer.arrayStride = vertex_buffer->arrayStride();
      dawn_vertex_buffer.stepMode =
          AsDawnEnum<WGPUInputStepMode>(vertex_buffer->stepMode());
      dawn_vertex_buffer.attributeCount =
          static_cast<uint32_t>(vertex_buffer->attributes().size());
      dawn_vertex_buffer.attributes = nullptr;
      dawn_vertex_buffers.push_back(dawn_vertex_buffer);

      for (wtf_size_t j = 0; j < vertex_buffer->attributes().size(); ++j) {
        const GPUVertexAttributeDescriptor* attribute =
            vertex_buffer->attributes()[j];
        WGPUVertexAttributeDescriptor dawn_vertex_attribute = {};
        dawn_vertex_attribute.shaderLocation = attribute->shaderLocation();
        dawn_vertex_attribute.offset = attribute->offset();
        dawn_vertex_attribute.format =
            AsDawnEnum<WGPUVertexFormat>(attribute->format());
        dawn_vertex_attributes.push_back(dawn_vertex_attribute);
      }
    }

    // Set up pointers in DawnVertexBufferLayoutDescriptor::attributes only
    // after we stopped appending to the vector so the pointers aren't
    // invalidated.
    uint32_t attributeIndex = 0;
    for (WGPUVertexBufferLayoutDescriptor& buffer : dawn_vertex_buffers) {
      if (buffer.attributeCount == 0) {
        continue;
      }
      buffer.attributes = &dawn_vertex_attributes[attributeIndex];
      attributeIndex += buffer.attributeCount;
    }
  }

  dawn_desc.vertexBufferCount =
      static_cast<uint32_t>(dawn_vertex_buffers.size());
  dawn_desc.vertexBuffers = dawn_vertex_buffers.data();

  return std::make_tuple(dawn_desc, std::move(dawn_vertex_buffers),
                         std::move(dawn_vertex_attributes));
}

WGPURasterizationStateDescriptor AsDawnType(
    const GPURasterizationStateDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPURasterizationStateDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.frontFace = AsDawnEnum<WGPUFrontFace>(webgpu_desc->frontFace());
  dawn_desc.cullMode = AsDawnEnum<WGPUCullMode>(webgpu_desc->cullMode());
  dawn_desc.depthBias = webgpu_desc->depthBias();
  dawn_desc.depthBiasSlopeScale = webgpu_desc->depthBiasSlopeScale();
  dawn_desc.depthBiasClamp = webgpu_desc->depthBiasClamp();

  return dawn_desc;
}

}  // anonymous namespace

// static
GPURenderPipeline* GPURenderPipeline::Create(
    ScriptState* script_state,
    GPUDevice* device,
    const GPURenderPipelineDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  std::string label;
  WGPURenderPipelineDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  if (webgpu_desc->hasLayout()) {
    dawn_desc.layout = AsDawnType(webgpu_desc->layout());
  }
  if (webgpu_desc->hasLabel()) {
    label = webgpu_desc->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  OwnedProgrammableStageDescriptor vertex_stage_info =
      AsDawnType(webgpu_desc->vertexStage());
  dawn_desc.vertexStage = std::get<0>(vertex_stage_info);
  OwnedProgrammableStageDescriptor fragment_stage_info;
  if (webgpu_desc->hasFragmentStage()) {
    fragment_stage_info = AsDawnType(webgpu_desc->fragmentStage());
    dawn_desc.fragmentStage = &std::get<0>(fragment_stage_info);
  } else {
    dawn_desc.fragmentStage = nullptr;
  }

  dawn_desc.primitiveTopology =
      AsDawnEnum<WGPUPrimitiveTopology>(webgpu_desc->primitiveTopology());

  v8::Isolate* isolate = script_state->GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 "GPUVertexStateDescriptor");
  WGPUVertexStateInfo vertex_state_info = GPUVertexStateAsWGPUVertexState(
      isolate, webgpu_desc->vertexState(), exception_state);
  WGPUVertexStateDescriptor dawn_vertex_state = std::get<0>(vertex_state_info);

  dawn_desc.vertexState = &dawn_vertex_state;

  if (exception_state.HadException()) {
    return nullptr;
  }

  WGPURasterizationStateDescriptor rasterization_state;
  rasterization_state = AsDawnType(webgpu_desc->rasterizationState());
  dawn_desc.rasterizationState = &rasterization_state;

  dawn_desc.sampleCount = webgpu_desc->sampleCount();

  WGPUDepthStencilStateDescriptor depth_stencil_state = {};
  if (webgpu_desc->hasDepthStencilState()) {
    depth_stencil_state = AsDawnType(webgpu_desc->depthStencilState());
    dawn_desc.depthStencilState = &depth_stencil_state;
  } else {
    dawn_desc.depthStencilState = nullptr;
  }

  std::unique_ptr<WGPUColorStateDescriptor[]> color_states =
      AsDawnType(webgpu_desc->colorStates());
  dawn_desc.colorStateCount =
      static_cast<uint32_t>(webgpu_desc->colorStates().size());

  dawn_desc.colorStates = color_states.get();

  dawn_desc.sampleMask = webgpu_desc->sampleMask();
  dawn_desc.alphaToCoverageEnabled = webgpu_desc->alphaToCoverageEnabled();

  return MakeGarbageCollected<GPURenderPipeline>(
      device, device->GetProcs().deviceCreateRenderPipeline(device->GetHandle(),
                                                            &dawn_desc));
}

GPURenderPipeline::GPURenderPipeline(GPUDevice* device,
                                     WGPURenderPipeline render_pipeline)
    : DawnObject<WGPURenderPipeline>(device, render_pipeline) {}

GPURenderPipeline::~GPURenderPipeline() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().renderPipelineRelease(GetHandle());
}

GPUBindGroupLayout* GPURenderPipeline::getBindGroupLayout(uint32_t index) {
  return MakeGarbageCollected<GPUBindGroupLayout>(
      device_, GetProcs().renderPipelineGetBindGroupLayout(GetHandle(), index));
}

}  // namespace blink
