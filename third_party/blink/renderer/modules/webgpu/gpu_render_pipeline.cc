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
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_vertex_state_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

WGPUBlendComponent AsDawnType(const GPUBlendComponent* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUBlendComponent dawn_desc = {};
  dawn_desc.dstFactor = AsDawnEnum<WGPUBlendFactor>(webgpu_desc->dstFactor());
  dawn_desc.srcFactor = AsDawnEnum<WGPUBlendFactor>(webgpu_desc->srcFactor());
  dawn_desc.operation =
      AsDawnEnum<WGPUBlendOperation>(webgpu_desc->operation());

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

WGPUStencilStateFaceDescriptor AsDawnType(
    const GPUStencilFaceState* webgpu_desc) {
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

WGPUPrimitiveState AsDawnType(const GPUPrimitiveState* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUPrimitiveState dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.topology =
      AsDawnEnum<WGPUPrimitiveTopology>(webgpu_desc->topology());
  dawn_desc.stripIndexFormat =
      AsDawnEnum<WGPUIndexFormat>(webgpu_desc->stripIndexFormat());
  dawn_desc.frontFace = AsDawnEnum<WGPUFrontFace>(webgpu_desc->frontFace());
  dawn_desc.cullMode = AsDawnEnum<WGPUCullMode>(webgpu_desc->cullMode());

  return dawn_desc;
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

const char* GetUpdatedVertexFormat(WGPUVertexFormat format) {
  switch (format) {
    case WGPUVertexFormat_UChar2:
      return "uint8x2";
    case WGPUVertexFormat_UChar4:
      return "uint8x4";
    case WGPUVertexFormat_Char2:
      return "sint8x2";
    case WGPUVertexFormat_Char4:
      return "sint8x4";
    case WGPUVertexFormat_UChar2Norm:
      return "unorm8x2";
    case WGPUVertexFormat_UChar4Norm:
      return "unorm8x4";
    case WGPUVertexFormat_Char2Norm:
      return "snorm8x2";
    case WGPUVertexFormat_Char4Norm:
      return "snorm8x4";
    case WGPUVertexFormat_UShort2:
      return "uint16x2";
    case WGPUVertexFormat_UShort4:
      return "uint16x4";
    case WGPUVertexFormat_Short2:
      return "sint16x2";
    case WGPUVertexFormat_Short4:
      return "sint16x4";
    case WGPUVertexFormat_UShort2Norm:
      return "unorm16x2";
    case WGPUVertexFormat_UShort4Norm:
      return "unorm16x4";
    case WGPUVertexFormat_Short2Norm:
      return "snorm16x2";
    case WGPUVertexFormat_Short4Norm:
      return "snorm16x4";
    case WGPUVertexFormat_Half2:
      return "float16x2";
    case WGPUVertexFormat_Half4:
      return "float16x4";
    case WGPUVertexFormat_Float:
      return "float32";
    case WGPUVertexFormat_Float2:
      return "float32x2";
    case WGPUVertexFormat_Float3:
      return "float32x3";
    case WGPUVertexFormat_Float4:
      return "float32x4";
    case WGPUVertexFormat_UInt:
      return "uint32";
    case WGPUVertexFormat_UInt2:
      return "uint32x2";
    case WGPUVertexFormat_UInt3:
      return "uint32x3";
    case WGPUVertexFormat_UInt4:
      return "uint32x4";
    case WGPUVertexFormat_Int:
      return "sint32";
    case WGPUVertexFormat_Int2:
      return "sint32x2";
    case WGPUVertexFormat_Int3:
      return "sint32x3";
    case WGPUVertexFormat_Int4:
      return "sint32x4";
    default:
      return "";
  }
}

void AsDawnVertexBufferLayouts(
    v8::Isolate* isolate,
    GPUDevice* device,
    v8::Local<v8::Value> vertex_buffers_value,
    Vector<WGPUVertexBufferLayoutDescriptor>* dawn_vertex_buffers,
    Vector<WGPUVertexAttributeDescriptor>* dawn_vertex_attributes,
    ExceptionState& exception_state) {
  if (!vertex_buffers_value->IsArray()) {
    exception_state.ThrowTypeError("vertexBuffers must be an array");
    return;
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
      dawn_vertex_buffers->push_back(dawn_vertex_buffer);
      continue;
    }

    GPUVertexBufferLayout* vertex_buffer =
        NativeValueTraits<GPUVertexBufferLayout>::NativeValue(isolate, value,
                                                              exception_state);
    if (exception_state.HadException()) {
      return;
    }

    WGPUVertexBufferLayoutDescriptor dawn_vertex_buffer = {};
    dawn_vertex_buffer.arrayStride = vertex_buffer->arrayStride();
    dawn_vertex_buffer.stepMode =
        AsDawnEnum<WGPUInputStepMode>(vertex_buffer->stepMode());
    dawn_vertex_buffer.attributeCount =
        static_cast<uint32_t>(vertex_buffer->attributes().size());
    dawn_vertex_buffer.attributes = nullptr;
    dawn_vertex_buffers->push_back(dawn_vertex_buffer);

    for (wtf_size_t j = 0; j < vertex_buffer->attributes().size(); ++j) {
      const GPUVertexAttribute* attribute = vertex_buffer->attributes()[j];
      WGPUVertexAttributeDescriptor dawn_vertex_attribute = {};
      dawn_vertex_attribute.shaderLocation = attribute->shaderLocation();
      dawn_vertex_attribute.offset = attribute->offset();
      dawn_vertex_attribute.format =
          AsDawnEnum<WGPUVertexFormat>(attribute->format());
      if (dawn_vertex_attribute.format >= WGPUVertexFormat_UChar2) {
        WTF::String message =
            String("The vertex format '") +
            IDLEnumAsString(attribute->format()) +
            String("' has been deprecated in favor of '") +
            GetUpdatedVertexFormat(dawn_vertex_attribute.format) + "'.";
        device->AddConsoleWarning(message.Utf8().data());
      }
      dawn_vertex_attributes->push_back(dawn_vertex_attribute);
    }
  }

  // Set up pointers in DawnVertexBufferLayoutDescriptor::attributes only
  // after we stopped appending to the vector so the pointers aren't
  // invalidated.
  uint32_t attributeIndex = 0;
  for (WGPUVertexBufferLayoutDescriptor& buffer : *dawn_vertex_buffers) {
    if (buffer.attributeCount == 0) {
      continue;
    }
    buffer.attributes = &(*dawn_vertex_attributes)[attributeIndex];
    attributeIndex += buffer.attributeCount;
  }
}

void GPUVertexStateAsWGPUVertexState(
    v8::Isolate* isolate,
    GPUDevice* device,
    const GPUVertexStateDescriptor* descriptor,
    WGPUVertexStateDescriptor* dawn_desc,
    Vector<WGPUVertexBufferLayoutDescriptor>* dawn_vertex_buffers,
    Vector<WGPUVertexAttributeDescriptor>* dawn_vertex_attributes,
    ExceptionState& exception_state) {
  DCHECK(isolate);
  DCHECK(descriptor);
  DCHECK(dawn_desc);
  DCHECK(dawn_vertex_buffers);
  DCHECK(dawn_vertex_attributes);

  *dawn_desc = {};
  dawn_desc->indexFormat =
      AsDawnEnum<WGPUIndexFormat>(descriptor->indexFormat());
  dawn_desc->vertexBufferCount = 0;
  dawn_desc->vertexBuffers = nullptr;

  if (descriptor->hasVertexBuffers()) {
    // TODO(crbug.com/951629): Use a sequence of nullable descriptors.
    v8::Local<v8::Value> vertex_buffers_value =
        descriptor->vertexBuffers().V8Value();
    AsDawnVertexBufferLayouts(isolate, device, vertex_buffers_value,
                              dawn_vertex_buffers, dawn_vertex_attributes,
                              exception_state);
  }

  dawn_desc->vertexBufferCount =
      static_cast<uint32_t>(dawn_vertex_buffers->size());
  dawn_desc->vertexBuffers = dawn_vertex_buffers->data();
}

void GPUFragmentStateAsWGPUFragmentState(const GPUFragmentState* descriptor,
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
      dawn_fragment->blend_states[i] = AsDawnType(color_target->blend());
      dawn_fragment->targets[i].blend = &dawn_fragment->blend_states[i];
    }
  }
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

void ConvertToDawnType(v8::Isolate* isolate,
                       GPUDevice* device,
                       const GPURenderPipelineDescriptor* webgpu_desc,
                       OwnedRenderPipelineDescriptor* dawn_desc_info,
                       ExceptionState& exception_state) {
  DCHECK(isolate);
  DCHECK(webgpu_desc);
  DCHECK(dawn_desc_info);
  DCHECK(!webgpu_desc->hasVertex());

  device->AddConsoleWarning(
      "The format of GPURenderPipelineDescriptor has "
      "changed, and will soon require the new structure. Please begin using "
      "the vertex, primitive, depthStencil, multisample, and fragment members");

  // Check for required members. Can't do this in the IDL because then the
  // deprecated members would be required for the new layout.
  if (!webgpu_desc->vertexStage()) {
    exception_state.ThrowTypeError("required member vertexStage is undefined.");
    return;
  }

  if (!webgpu_desc->hasPrimitiveTopology()) {
    exception_state.ThrowTypeError(
        "required member primitiveTopology is undefined.");
    return;
  }

  if (!webgpu_desc->hasColorStates()) {
    exception_state.ThrowTypeError("required member colorStates is undefined.");
    return;
  }

  GPUVertexStateAsWGPUVertexState(
      isolate, device, webgpu_desc->vertexState(),
      &dawn_desc_info->vertex_state, &dawn_desc_info->vertex_buffer_layouts,
      &dawn_desc_info->vertex_attributes, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  dawn_desc_info->dawn_desc.vertexState = &dawn_desc_info->vertex_state;

  if (webgpu_desc->hasLayout()) {
    dawn_desc_info->dawn_desc.layout = AsDawnType(webgpu_desc->layout());
  }

  if (webgpu_desc->hasLabel()) {
    dawn_desc_info->label = webgpu_desc->label().Utf8();
    dawn_desc_info->dawn_desc.label = dawn_desc_info->label.c_str();
  }

  dawn_desc_info->vertex_stage_info = AsDawnType(webgpu_desc->vertexStage());
  dawn_desc_info->dawn_desc.vertexStage =
      std::get<0>(dawn_desc_info->vertex_stage_info);
  if (webgpu_desc->hasFragmentStage()) {
    dawn_desc_info->fragment_stage_info =
        AsDawnType(webgpu_desc->fragmentStage());
    dawn_desc_info->dawn_desc.fragmentStage =
        &std::get<0>(dawn_desc_info->fragment_stage_info);
  }

  dawn_desc_info->dawn_desc.primitiveTopology =
      AsDawnEnum<WGPUPrimitiveTopology>(webgpu_desc->primitiveTopology());

  dawn_desc_info->rasterization_state =
      AsDawnType(webgpu_desc->rasterizationState());
  dawn_desc_info->dawn_desc.rasterizationState =
      &dawn_desc_info->rasterization_state;

  dawn_desc_info->dawn_desc.sampleCount = webgpu_desc->sampleCount();

  if (webgpu_desc->hasDepthStencilState()) {
    dawn_desc_info->depth_stencil_state =
        AsDawnType(webgpu_desc->depthStencilState());
    dawn_desc_info->dawn_desc.depthStencilState =
        &dawn_desc_info->depth_stencil_state;
  }

  dawn_desc_info->color_states = AsDawnType(webgpu_desc->colorStates());
  dawn_desc_info->dawn_desc.colorStateCount =
      static_cast<uint32_t>(webgpu_desc->colorStates().size());
  dawn_desc_info->dawn_desc.colorStates = dawn_desc_info->color_states.get();

  dawn_desc_info->dawn_desc.sampleMask = webgpu_desc->sampleMask();
  dawn_desc_info->dawn_desc.alphaToCoverageEnabled =
      webgpu_desc->alphaToCoverageEnabled();
}

void ConvertToDawnType(v8::Isolate* isolate,
                       GPUDevice* device,
                       const GPURenderPipelineDescriptor* webgpu_desc,
                       OwnedRenderPipelineDescriptor2* dawn_desc_info,
                       ExceptionState& exception_state) {
  DCHECK(isolate);
  DCHECK(webgpu_desc);
  DCHECK(dawn_desc_info);

  // Check for required members. Can't do this in the IDL because then new
  // members would be required for the deprecated layout.
  if (!webgpu_desc->hasVertex()) {
    exception_state.ThrowTypeError("required member vertex is undefined.");
    return;
  }

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

  // Primitive,
  dawn_desc_info->dawn_desc.primitive = AsDawnType(webgpu_desc->primitive());

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
    GPUFragmentStateAsWGPUFragmentState(webgpu_desc->fragment(),
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
  if (webgpu_desc->hasVertex()) {
    OwnedRenderPipelineDescriptor2 dawn_desc_info;
    ConvertToDawnType(isolate, device, webgpu_desc, &dawn_desc_info,
                      exception_state);
    if (exception_state.HadException()) {
      return nullptr;
    }

    pipeline = MakeGarbageCollected<GPURenderPipeline>(
        device, device->GetProcs().deviceCreateRenderPipeline2(
                    device->GetHandle(), &dawn_desc_info.dawn_desc));
  } else {
    OwnedRenderPipelineDescriptor dawn_desc_info;
    ConvertToDawnType(isolate, device, webgpu_desc, &dawn_desc_info,
                      exception_state);
    if (exception_state.HadException()) {
      return nullptr;
    }

    pipeline = MakeGarbageCollected<GPURenderPipeline>(
        device, device->GetProcs().deviceCreateRenderPipeline(
                    device->GetHandle(), &dawn_desc_info.dawn_desc));
  }

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
