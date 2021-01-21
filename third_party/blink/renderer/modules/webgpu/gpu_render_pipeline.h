// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PIPELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PIPELINE_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUBindGroupLayout;
class GPURenderPipelineDescriptor;
class ExceptionState;
class ScriptState;

struct OwnedRenderPipelineDescriptor {
 public:
  OwnedRenderPipelineDescriptor() : dawn_desc({}) {}

  //  This struct should be non-copyable non-movable because it contains
  //  self-referencing pointers that would be invalidated when moved / copied.
  OwnedRenderPipelineDescriptor(const OwnedRenderPipelineDescriptor& desc) =
      delete;
  OwnedRenderPipelineDescriptor(OwnedRenderPipelineDescriptor&& desc) = delete;
  OwnedRenderPipelineDescriptor& operator=(
      const OwnedRenderPipelineDescriptor& desc) = delete;
  OwnedRenderPipelineDescriptor& operator=(
      OwnedRenderPipelineDescriptor&& desc) = delete;

  WGPURenderPipelineDescriptor dawn_desc;
  std::string label;
  OwnedProgrammableStageDescriptor vertex_stage_info;
  OwnedProgrammableStageDescriptor fragment_stage_info;
  WGPUVertexStateDescriptor vertex_state;
  Vector<WGPUVertexBufferLayoutDescriptor> vertex_buffer_layouts;
  Vector<WGPUVertexAttributeDescriptor> vertex_attributes;
  WGPURasterizationStateDescriptor rasterization_state;
  WGPUDepthStencilStateDescriptor depth_stencil_state;
  std::unique_ptr<WGPUColorStateDescriptor[]> color_states;
};

void ConvertToDawnType(v8::Isolate* isolate,
                       const GPURenderPipelineDescriptor* webgpu_desc,
                       OwnedRenderPipelineDescriptor* dawn_desc_info,
                       ExceptionState& exception_state);

class GPURenderPipeline : public DawnObject<WGPURenderPipeline> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPURenderPipeline* Create(
      ScriptState* script_state,
      GPUDevice* device,
      const GPURenderPipelineDescriptor* webgpu_desc);
  explicit GPURenderPipeline(GPUDevice* device,
                             WGPURenderPipeline render_pipeline);

  GPUBindGroupLayout* getBindGroupLayout(uint32_t index);

 private:
  DISALLOW_COPY_AND_ASSIGN(GPURenderPipeline);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PIPELINE_H_
