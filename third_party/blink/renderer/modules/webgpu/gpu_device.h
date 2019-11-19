// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_callback.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class ExecutionContext;
class GPUAdapter;
class GPUAdapter;
class GPUBuffer;
class GPUBufferDescriptor;
class GPUCommandEncoder;
class GPUCommandEncoderDescriptor;
class GPUBindGroup;
class GPUBindGroupDescriptor;
class GPUBindGroupLayout;
class GPUBindGroupLayoutDescriptor;
class GPUComputePipeline;
class GPUComputePipelineDescriptor;
class GPUDeviceDescriptor;
class GPUDeviceLostInfo;
class GPUPipelineLayout;
class GPUPipelineLayoutDescriptor;
class GPUQueue;
class GPURenderBundleEncoder;
class GPURenderBundleEncoderDescriptor;
class GPURenderPipeline;
class GPURenderPipelineDescriptor;
class GPUSampler;
class GPUSamplerDescriptor;
class GPUShaderModule;
class GPUShaderModuleDescriptor;
class GPUTexture;
class GPUTextureDescriptor;
class ScriptPromiseResolver;
class ScriptState;

class GPUDevice final : public EventTargetWithInlineData,
                        public ContextClient,
                        public DawnObject<WGPUDevice> {
  USING_GARBAGE_COLLECTED_MIXIN(GPUDevice);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUDevice* Create(
      ExecutionContext* execution_context,
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      GPUAdapter* adapter,
      const GPUDeviceDescriptor* descriptor);
  explicit GPUDevice(ExecutionContext* execution_context,
                     scoped_refptr<DawnControlClientHolder> dawn_control_client,
                     GPUAdapter* adapter,
                     const GPUDeviceDescriptor* descriptor);
  ~GPUDevice() override;

  void Trace(blink::Visitor* visitor) override;

  // gpu_device.idl
  GPUAdapter* adapter() const;
  ScriptPromise lost(ScriptState* script_state);

  GPUQueue* defaultQueue();

  GPUBuffer* createBuffer(const GPUBufferDescriptor* descriptor);
  HeapVector<ScriptValue> createBufferMapped(
      ScriptState* script_state,
      const GPUBufferDescriptor* descriptor,
      ExceptionState& exception_state);
  ScriptPromise createBufferMappedAsync(ScriptState* script_state,
                                        const GPUBufferDescriptor* descriptor,
                                        ExceptionState& exception_state);
  GPUTexture* createTexture(const GPUTextureDescriptor* descriptor,
                            ExceptionState& exception_state);
  GPUSampler* createSampler(const GPUSamplerDescriptor* descriptor);

  GPUBindGroup* createBindGroup(const GPUBindGroupDescriptor* descriptor);
  GPUBindGroupLayout* createBindGroupLayout(
      const GPUBindGroupLayoutDescriptor* descriptor);
  GPUPipelineLayout* createPipelineLayout(
      const GPUPipelineLayoutDescriptor* descriptor);

  GPUShaderModule* createShaderModule(
      const GPUShaderModuleDescriptor* descriptor);
  GPURenderPipeline* createRenderPipeline(
      ScriptState* script_state,
      const GPURenderPipelineDescriptor* descriptor);
  GPUComputePipeline* createComputePipeline(
      const GPUComputePipelineDescriptor* descriptor);

  GPUCommandEncoder* createCommandEncoder(
      const GPUCommandEncoderDescriptor* descriptor);
  GPURenderBundleEncoder* createRenderBundleEncoder(
      const GPURenderBundleEncoderDescriptor* descriptor);

  void pushErrorScope(const WTF::String& filter);
  ScriptPromise popErrorScope(ScriptState* script_state);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(uncapturederror, kUncapturederror)

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

 private:
  using LostProperty = ScriptPromiseProperty<Member<GPUDevice>,
                                             Member<GPUDeviceLostInfo>,
                                             ToV8UndefinedGenerator>;

  void OnUncapturedError(ExecutionContext* execution_context,
                         WGPUErrorType errorType,
                         const char* message);

  void OnPopErrorScopeCallback(ScriptPromiseResolver* resolver,
                               WGPUErrorType type,
                               const char* message);

  Member<GPUAdapter> adapter_;
  Member<GPUQueue> queue_;
  Member<LostProperty> lost_property_;
  std::unique_ptr<
      DawnCallback<base::RepeatingCallback<void(WGPUErrorType, const char*)>>>
      error_callback_;

  DISALLOW_COPY_AND_ASSIGN(GPUDevice);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_
