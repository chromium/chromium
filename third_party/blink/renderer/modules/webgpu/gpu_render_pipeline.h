// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PIPELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PIPELINE_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_programmable_stage.h"
namespace blink {

class GPUBindGroupLayout;
class GPURenderPipelineDescriptor;
class ExceptionState;
class ScriptState;

struct OwnedVertexState : OwnedProgrammableStage {
  OwnedVertexState() = default;

  //  This struct should be non-copyable non-movable because it contains
  //  self-referencing pointers that would be invalidated when moved / copied.
  OwnedVertexState(const OwnedVertexState& desc) = delete;
  OwnedVertexState(OwnedVertexState&& desc) = delete;
  OwnedVertexState& operator=(const OwnedVertexState& desc) = delete;
  OwnedVertexState& operator=(OwnedVertexState&& desc) = delete;

  // Points to OwnedRenderPipelineDescriptor::dawn_desc::vertex as it's a
  // non-pointer member of WGPURenderPipelineDescriptor
  WGPUVertexState* dawn_desc = nullptr;
  std::unique_ptr<WGPUVertexBufferLayout[]> buffers;
  std::unique_ptr<std::unique_ptr<WGPUVertexAttribute[]>[]> attributes;
};

struct OwnedFragmentState : OwnedProgrammableStage {
  OwnedFragmentState() = default;

  //  This struct should be non-copyable non-movable because it contains
  //  self-referencing pointers that would be invalidated when moved / copied.
  OwnedFragmentState(const OwnedFragmentState& desc) = delete;
  OwnedFragmentState(OwnedFragmentState&& desc) = delete;
  OwnedFragmentState& operator=(const OwnedFragmentState& desc) = delete;
  OwnedFragmentState& operator=(OwnedFragmentState&& desc) = delete;

  WGPUFragmentState dawn_desc = {};
  std::unique_ptr<WGPUColorTargetState[]> targets;
  Vector<WGPUBlendState> blend_states;
};

struct OwnedPrimitiveState {
  OwnedPrimitiveState() = default;

  //  This struct should be non-copyable non-movable because it contains
  //  self-referencing pointers that would be invalidated when moved / copied.
  OwnedPrimitiveState(const OwnedPrimitiveState& desc) = delete;
  OwnedPrimitiveState(OwnedPrimitiveState&& desc) = delete;
  OwnedPrimitiveState& operator=(const OwnedPrimitiveState& desc) = delete;
  OwnedPrimitiveState& operator=(OwnedPrimitiveState&& desc) = delete;

  WGPUPrimitiveState dawn_desc = {};
  WGPUPrimitiveDepthClipControl depth_clip_control = {};
};

struct OwnedRenderPipelineDescriptor {
  OwnedRenderPipelineDescriptor() = default;

  //  This struct should be non-copyable non-movable because it contains
  //  self-referencing pointers that would be invalidated when moved / copied.
  OwnedRenderPipelineDescriptor(const OwnedRenderPipelineDescriptor& desc) =
      delete;
  OwnedRenderPipelineDescriptor(OwnedRenderPipelineDescriptor&& desc) = delete;
  OwnedRenderPipelineDescriptor& operator=(
      const OwnedRenderPipelineDescriptor& desc) = delete;
  OwnedRenderPipelineDescriptor& operator=(
      OwnedRenderPipelineDescriptor&& desc) = delete;

  WGPURenderPipelineDescriptor dawn_desc = {};
  std::string label;
  OwnedVertexState vertex;
  OwnedPrimitiveState primitive;
  WGPUDepthStencilState depth_stencil;
  OwnedFragmentState fragment;
};

void ConvertToDawnType(v8::Isolate* isolate,
                       GPUDevice* device,
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

  GPURenderPipeline(const GPURenderPipeline&) = delete;
  GPURenderPipeline& operator=(const GPURenderPipeline&) = delete;

  GPUBindGroupLayout* getBindGroupLayout(uint32_t index);

 private:
  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().renderPipelineSetLabel(GetHandle(), utf8_label.c_str());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PIPELINE_H_
