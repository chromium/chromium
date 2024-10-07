// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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

wgpu::BlendComponent AsDawnType(const GPUBlendComponent* webgpu_desc) {
  DCHECK(webgpu_desc);

  wgpu::BlendComponent dawn_desc = {
      .operation = AsDawnEnum(webgpu_desc->getOperationOr(
          V8GPUBlendOperation(V8GPUBlendOperation::Enum::kAdd))),
      .srcFactor = AsDawnEnum(webgpu_desc->getSrcFactorOr(
          V8GPUBlendFactor(V8GPUBlendFactor::Enum::kOne))),
      .dstFactor = AsDawnEnum(webgpu_desc->getDstFactorOr(
          V8GPUBlendFactor(V8GPUBlendFactor::Enum::kZero))),
  };
  return dawn_desc;
}

wgpu::BlendState AsDawnType(const GPUBlendState* webgpu_desc) {
  DCHECK(webgpu_desc);

  wgpu::BlendState dawn_desc = {
      .color = AsDawnType(webgpu_desc->color()),
      .alpha = AsDawnType(webgpu_desc->alpha()),
  };

  return dawn_desc;
}

bool ValidateBlendComponent(GPUDevice* device,
                            const GPUBlendComponent* webgpu_desc,
                            ExceptionState& exception_state) {
  DCHECK(webgpu_desc);

  return device->ValidateBlendFactor(
             webgpu_desc->getSrcFactorOr(
                 V8GPUBlendFactor(V8GPUBlendFactor::Enum::kOne)),
             exception_state) &&
         device->ValidateBlendFactor(
             webgpu_desc->getDstFactorOr(
                 V8GPUBlendFactor(V8GPUBlendFactor::Enum::kZero)),
             exception_state);
}

}  // anonymous namespace

wgpu::ColorTargetState AsDawnType(const GPUColorTargetState* webgpu_desc) {
  DCHECK(webgpu_desc);

  wgpu::ColorTargetState dawn_desc = {
      // .blend is handled in ConvertToDawnType
      .format = AsDawnEnum(webgpu_desc->format()),
      .writeMask = AsDawnFlags<wgpu::ColorWriteMask>(webgpu_desc->writeMask()),
  };
  return dawn_desc;
}

wgpu::VertexBufferLayout AsDawnType(const GPUVertexBufferLayout* webgpu_desc) {
  DCHECK(webgpu_desc);

  wgpu::VertexBufferLayout dawn_desc = {
      .arrayStride = webgpu_desc->arrayStride(),
      .stepMode = AsDawnEnum(webgpu_desc->stepMode()),
      .attributeCount = webgpu_desc->attributes().size(),
      // .attributes is handled outside separately
  };

  return dawn_desc;
}

wgpu::VertexAttribute AsDawnType(const GPUVertexAttribute* webgpu_desc) {
  DCHECK(webgpu_desc);

  wgpu::VertexAttribute dawn_desc = {
      .format = AsDawnEnum(webgpu_desc->format()),
      .offset = webgpu_desc->offset(),
      .shaderLocation = webgpu_desc->shaderLocation(),
  };

  return dawn_desc;
}

namespace {

wgpu::StencilFaceState AsDawnType(const GPUStencilFaceState* webgpu_desc) {
  DCHECK(webgpu_desc);

  wgpu::StencilFaceState dawn_desc = {
      .compare = AsDawnEnum(webgpu_desc->compare()),
      .failOp = AsDawnEnum(webgpu_desc->failOp()),
      .depthFailOp = AsDawnEnum(webgpu_desc->depthFailOp()),
      .passOp = AsDawnEnum(webgpu_desc->passOp()),
  };

  return dawn_desc;
}

wgpu::PrimitiveState AsDawnType(const GPUPrimitiveState* webgpu_desc) {
  DCHECK(webgpu_desc);

  wgpu::PrimitiveState dawn_desc = {};
  dawn_desc.topology = AsDawnEnum(webgpu_desc->topology());

  if (webgpu_desc->hasStripIndexFormat()) {
    dawn_desc.stripIndexFormat = AsDawnEnum(webgpu_desc->stripIndexFormat());
  }

  dawn_desc.frontFace = AsDawnEnum(webgpu_desc->frontFace());
  dawn_desc.cullMode = AsDawnEnum(webgpu_desc->cullMode());
  dawn_desc.unclippedDepth = webgpu_desc->unclippedDepth();

  return dawn_desc;
}

wgpu::DepthStencilState AsDawnType(GPUDevice* device,
                                   const GPUDepthStencilState* webgpu_desc,
                                   ExceptionState& exception_state) {
  DCHECK(webgpu_desc);

  if (!device->ValidateTextureFormatUsage(webgpu_desc->format(),
                                          exception_state)) {
    return {};
  }

  wgpu::DepthStencilState dawn_desc = {};
  dawn_desc.format = AsDawnEnum(webgpu_desc->format());

  if (webgpu_desc->hasDepthWriteEnabled()) {
    dawn_desc.depthWriteEnabled = webgpu_desc->depthWriteEnabled()
                                      ? wgpu::OptionalBool::True
                                      : wgpu::OptionalBool::False;
  }

  if (webgpu_desc->hasDepthCompare()) {
    dawn_desc.depthCompare = AsDawnEnum(webgpu_desc->depthCompare());
  }

  dawn_desc.stencilFront = AsDawnType(webgpu_desc->stencilFront());
  dawn_desc.stencilBack = AsDawnType(webgpu_desc->stencilBack());
  dawn_desc.stencilReadMask = webgpu_desc->stencilReadMask();
  dawn_desc.stencilWriteMask = webgpu_desc->stencilWriteMask();
  dawn_desc.depthBias = webgpu_desc->depthBias();
  dawn_desc.depthBiasSlopeScale = webgpu_desc->depthBiasSlopeScale();
  dawn_desc.depthBiasClamp = webgpu_desc->depthBiasClamp();

  return dawn_desc;
}

wgpu::MultisampleState AsDawnType(const GPUMultisampleState* webgpu_desc) {
  DCHECK(webgpu_desc);

  wgpu::MultisampleState dawn_desc = {
      .count = webgpu_desc->count(),
      .mask = webgpu_desc->mask(),
      .alphaToCoverageEnabled = webgpu_desc->alphaToCoverageEnabled(),
  };

  return dawn_desc;
}

void AsDawnVertexBufferLayouts(GPUDevice* device,
                               const GPUVertexState* descriptor,
                               OwnedVertexState* dawn_desc_info) {
  DCHECK(descriptor);
  DCHECK(dawn_desc_info);

  wgpu::VertexState* dawn_vertex = dawn_desc_info->dawn_desc;
  dawn_vertex->bufferCount = descriptor->buffers().size();

  if (dawn_vertex->bufferCount == 0) {
    dawn_vertex->buffers = nullptr;
    return;
  }

  // TODO(cwallez@chromium.org): Should we validate the Length() first so we
  // don't risk creating HUGE vectors of wgpu::VertexBufferLayout from
  // the sparse array?
  dawn_desc_info->buffers = AsDawnType(descriptor->buffers());
  dawn_vertex->buffers = dawn_desc_info->buffers.get();

  // Handle wgpu::VertexBufferLayout::attributes separately to guarantee the
  // lifetime.
  dawn_desc_info->attributes =
      std::make_unique<std::unique_ptr<wgpu::VertexAttribute[]>[]>(
          dawn_vertex->bufferCount);
  for (wtf_size_t i = 0; i < dawn_vertex->bufferCount; ++i) {
    const auto& maybe_buffer = descriptor->buffers()[i];
    if (!maybe_buffer) {
      // This buffer layout is empty.
      // Explicitly set VertexBufferNotUsed step mode to represent
      // this slot is empty for Dawn, and continue the loop.
      dawn_desc_info->buffers[i].stepMode =
          wgpu::VertexStepMode::VertexBufferNotUsed;
      continue;
    }
    const GPUVertexBufferLayout* buffer = maybe_buffer.Get();
    dawn_desc_info->attributes.get()[i] = AsDawnType(buffer->attributes());
    wgpu::VertexBufferLayout* dawn_buffer = &dawn_desc_info->buffers[i];
    dawn_buffer->attributes = dawn_desc_info->attributes.get()[i].get();
  }
}

void GPUVertexStateAsWGPUVertexState(GPUDevice* device,
                                     const GPUVertexState* descriptor,
                                     OwnedVertexState* dawn_vertex) {
  DCHECK(descriptor);
  DCHECK(dawn_vertex);

  *dawn_vertex->dawn_desc = {};

  GPUProgrammableStageAsWGPUProgrammableStage(descriptor, dawn_vertex);
  dawn_vertex->dawn_desc->constantCount = dawn_vertex->constantCount;
  dawn_vertex->dawn_desc->constants = dawn_vertex->constants.get();
  dawn_vertex->dawn_desc->module = descriptor->module()->GetHandle();
  dawn_vertex->dawn_desc->entryPoint =
      dawn_vertex->entry_point ? dawn_vertex->entry_point->c_str() : nullptr;

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

  GPUProgrammableStageAsWGPUProgrammableStage(descriptor, dawn_fragment);
  dawn_fragment->dawn_desc.constantCount = dawn_fragment->constantCount;
  dawn_fragment->dawn_desc.constants = dawn_fragment->constants.get();
  dawn_fragment->dawn_desc.module = descriptor->module()->GetHandle();
  dawn_fragment->dawn_desc.entryPoint =
      dawn_fragment->entry_point ? dawn_fragment->entry_point->c_str()
                                 : nullptr;

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

      if (!ValidateBlendComponent(device, blend_state->color(),
                                  exception_state) ||
          !ValidateBlendComponent(device, blend_state->alpha(),
                                  exception_state)) {
        return;
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
  if (!webgpu_desc->label().empty()) {
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
  dawn_desc_info->dawn_desc.primitive = AsDawnType(webgpu_desc->primitive());

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
  ExceptionState exception_state(isolate, v8::ExceptionContext::kConstructor,
                                 "GPURenderPipeline");

  GPURenderPipeline* pipeline;
  OwnedRenderPipelineDescriptor dawn_desc_info;
  ConvertToDawnType(isolate, device, webgpu_desc, &dawn_desc_info,
                    exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  pipeline = MakeGarbageCollected<GPURenderPipeline>(
      device,
      device->GetHandle().CreateRenderPipeline(&dawn_desc_info.dawn_desc),
      webgpu_desc->label());
  return pipeline;
}

GPURenderPipeline::GPURenderPipeline(GPUDevice* device,
                                     wgpu::RenderPipeline render_pipeline,
                                     const String& label)
    : DawnObject<wgpu::RenderPipeline>(device,
                                       std::move(render_pipeline),
                                       label) {}

GPUBindGroupLayout* GPURenderPipeline::getBindGroupLayout(uint32_t index) {
  return MakeGarbageCollected<GPUBindGroupLayout>(
      device_, GetHandle().GetBindGroupLayout(index), String());
}

}  // namespace blink
