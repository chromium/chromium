// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_ADAPTER_H_

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class GPU;
class GPUDeviceDescriptor;
class GPUSupportedFeatures;
class GPUSupportedLimits;
class ScriptPromiseResolver;

class GPUAdapter final : public ScriptWrappable, public DawnObjectBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  GPUAdapter(GPU* gpu,
             WGPUAdapter handle,
             scoped_refptr<DawnControlClientHolder> dawn_control_client);

  GPUAdapter(const GPUAdapter&) = delete;
  GPUAdapter& operator=(const GPUAdapter&) = delete;

  void Trace(Visitor* visitor) const override;

  GPU* gpu() const { return gpu_; }
  GPUSupportedFeatures* features() const;
  GPUSupportedLimits* limits() const { return limits_; }
  bool isFallbackAdapter() const;
  WGPUBackendType backendType() const;
  bool SupportsMultiPlanarFormats() const;
  bool isCompatibilityMode() const;

  ScriptPromise requestDevice(ScriptState* script_state,
                              GPUDeviceDescriptor* descriptor);

  ScriptPromise requestAdapterInfo(ScriptState* script_state,
                                   const Vector<String>& unmask_hints);

  // Console warnings should generally be attributed to a GPUDevice, but in
  // cases where there is no device warnings can be surfaced here. It's expected
  // that very few warning will need to be shown for a given adapter, and as a
  // result the maximum allowed warnings is lower than the per-device count.
  void AddConsoleWarning(ExecutionContext* execution_context,
                         const char* message);

 private:
  void OnRequestDeviceCallback(ScriptState* script_state,
                               ScriptPromiseResolver* resolver,
                               const GPUDeviceDescriptor* descriptor,
                               WGPURequestDeviceStatus status,
                               WGPUDevice dawn_device,
                               const char* error_message);

  void setLabelImpl(const String&) override {
    // There isn't a wgpu::Adapter::SetLabel, just skip.
  }

  WGPUAdapter handle_;
  Member<GPU> gpu_;
  bool is_fallback_adapter_;
  WGPUBackendType backend_type_;
  bool is_consumed_ = false;
  bool is_compatibility_mode_;
  Member<GPUSupportedLimits> limits_;
  Member<GPUSupportedFeatures> features_;

  String vendor_;
  String architecture_;
  String device_;
  String description_;
  String driver_;

  static constexpr int kMaxAllowedConsoleWarnings = 50;
  int allowed_console_warnings_remaining_ = kMaxAllowedConsoleWarnings;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_ADAPTER_H_
