// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_format.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_callback.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ExecutionContext;
class ExternalTextureCache;
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
class GPUExternalTexture;
class GPUExternalTextureDescriptor;
class GPUPipelineLayout;
class GPUPipelineLayoutDescriptor;
class GPUQuerySet;
class GPUQuerySetDescriptor;
class GPUQueue;
class GPURenderBundleEncoder;
class GPURenderBundleEncoderDescriptor;
class GPURenderPipeline;
class GPURenderPipelineDescriptor;
class GPUSampler;
class GPUSamplerDescriptor;
class GPUShaderModule;
class GPUShaderModuleDescriptor;
class GPUSupportedFeatures;
class GPUSupportedLimits;
class GPUTexture;
class GPUTextureDescriptor;
class ScriptPromiseResolver;
class ScriptState;
class V8GPUErrorFilter;
class GPUDevice final : public EventTargetWithInlineData,
                        public ExecutionContextClient,
                        public DawnObject<WGPUDevice> {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(GPUDevice, Dispose);

 public:
  explicit GPUDevice(ExecutionContext* execution_context,
                     scoped_refptr<DawnControlClientHolder> dawn_control_client,
                     GPUAdapter* adapter,
                     WGPUDevice dawn_device,
                     const GPUDeviceDescriptor* descriptor);

  GPUDevice(const GPUDevice&) = delete;
  GPUDevice& operator=(const GPUDevice&) = delete;

  ~GPUDevice() override;

  void Trace(Visitor* visitor) const override;

  // gpu_device.idl
  GPUAdapter* adapter() const;
  GPUSupportedFeatures* features() const;
  GPUSupportedLimits* limits() const { return limits_; }
  ScriptPromise lost(ScriptState* script_state);

  GPUQueue* queue();
  bool destroyed() const;

  void destroy(v8::Isolate* isolate);

  GPUBuffer* createBuffer(const GPUBufferDescriptor* descriptor,
                          ExceptionState& exception_state);
  GPUTexture* createTexture(const GPUTextureDescriptor* descriptor,
                            ExceptionState& exception_state);
  GPUSampler* createSampler(const GPUSamplerDescriptor* descriptor);

  GPUExternalTexture* importExternalTexture(
      ScriptState* script_state,
      const GPUExternalTextureDescriptor* descriptor,
      ExceptionState& exception_state);

  GPUBindGroup* createBindGroup(const GPUBindGroupDescriptor* descriptor,
                                ExceptionState& exception_state);
  GPUBindGroupLayout* createBindGroupLayout(
      const GPUBindGroupLayoutDescriptor* descriptor,
      ExceptionState& exception_state);
  GPUPipelineLayout* createPipelineLayout(
      const GPUPipelineLayoutDescriptor* descriptor);

  GPUShaderModule* createShaderModule(
      const GPUShaderModuleDescriptor* descriptor,
      ExceptionState& exception_state);
  GPURenderPipeline* createRenderPipeline(
      ScriptState* script_state,
      const GPURenderPipelineDescriptor* descriptor);
  GPUComputePipeline* createComputePipeline(
      const GPUComputePipelineDescriptor* descriptor,
      ExceptionState& exception_state);
  ScriptPromise createRenderPipelineAsync(
      ScriptState* script_state,
      const GPURenderPipelineDescriptor* descriptor);
  ScriptPromise createComputePipelineAsync(
      ScriptState* script_state,
      const GPUComputePipelineDescriptor* descriptor);

  GPUCommandEncoder* createCommandEncoder(
      const GPUCommandEncoderDescriptor* descriptor);
  GPURenderBundleEncoder* createRenderBundleEncoder(
      const GPURenderBundleEncoderDescriptor* descriptor,
      ExceptionState& exception_state);

  GPUQuerySet* createQuerySet(const GPUQuerySetDescriptor* descriptor,
                              ExceptionState& exception_state);

  void pushErrorScope(const V8GPUErrorFilter& filter);
  ScriptPromise popErrorScope(ScriptState* script_state);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(uncapturederror, kUncapturederror)

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  void InjectError(WGPUErrorType type, const char* message);
  void AddConsoleWarning(const char* message);

  void TrackTextureWithMailbox(GPUTexture* texture);
  void UntrackTextureWithMailbox(GPUTexture* texture);

  bool ValidateTextureFormatUsage(V8GPUTextureFormat format,
                                  ExceptionState& exception_state);
  std::string formattedLabel() const;

  // Store the buffer in a weak hash set so we can unmap it when the
  // device is destroyed.
  void TrackMappableBuffer(GPUBuffer* buffer);
  // Untrack the GPUBuffer. This is called eagerly when the buffer is
  // destroyed.
  void UntrackMappableBuffer(GPUBuffer* buffer);

 private:
  using LostProperty =
      ScriptPromiseProperty<Member<GPUDeviceLostInfo>, ToV8UndefinedGenerator>;

  // Used by USING_PRE_FINALIZER.
  void Dispose();
  void DissociateMailboxes();
  void UnmapAllMappableBuffers(v8::Isolate* isolate);

  void OnUncapturedError(WGPUErrorType errorType, const char* message);
  void OnLogging(WGPULoggingType loggingType, const char* message);
  void OnDeviceLostError(WGPUDeviceLostReason, const char* message);

  void OnPopErrorScopeCallback(ScriptPromiseResolver* resolver,
                               WGPUErrorType type,
                               const char* message);

  void OnCreateRenderPipelineAsyncCallback(ScriptPromiseResolver* resolver,
                                           absl::optional<String> label,
                                           WGPUCreatePipelineAsyncStatus status,
                                           WGPURenderPipeline render_pipeline,
                                           const char* message);
  void OnCreateComputePipelineAsyncCallback(
      ScriptPromiseResolver* resolver,
      absl::optional<String> label,
      WGPUCreatePipelineAsyncStatus status,
      WGPUComputePipeline compute_pipeline,
      const char* message);

  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().deviceSetLabel(GetHandle(), utf8_label.c_str());
  }

  Member<GPUAdapter> adapter_;
  Member<GPUSupportedFeatures> features_;
  Member<GPUSupportedLimits> limits_;
  Member<GPUQueue> queue_;
  Member<LostProperty> lost_property_;
  std::unique_ptr<WGPURepeatingCallback<void(WGPUErrorType, const char*)>>
      error_callback_;
  std::unique_ptr<WGPURepeatingCallback<void(WGPULoggingType, const char*)>>
      logging_callback_;
  // lost_callback_ is stored as a unique_ptr since it may never be called.
  // We need to be sure to free it on deletion of the device.
  // Inside OnDeviceLostError we'll release the unique_ptr to avoid a double
  // free.
  std::unique_ptr<
      WGPURepeatingCallback<void(WGPUDeviceLostReason, const char*)>>
      lost_callback_;

  static constexpr int kMaxAllowedConsoleWarnings = 500;
  int allowed_console_warnings_remaining_ = kMaxAllowedConsoleWarnings;

  // Textures with mailboxes that should be dissociated before device.destroy().
  HeapHashSet<WeakMember<GPUTexture>> textures_with_mailbox_;

  HeapHashSet<WeakMember<GPUBuffer>> mappable_buffers_;

  Member<ExternalTextureCache> external_texture_cache_;

  // This attribute records that whether GPUDevice is destroyed (via destroy()).
  bool destroyed_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_
