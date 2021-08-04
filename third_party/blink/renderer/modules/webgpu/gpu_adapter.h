// Copyright 2018 The Chromium Authors. All rights reserved.
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
             const String& name,
             uint32_t adapter_service_id,
             const WGPUDeviceProperties& properties,
             scoped_refptr<DawnControlClientHolder> dawn_control_client);

  void Trace(Visitor* visitor) const override;

  const String& name() const;
  GPU* gpu() const { return gpu_; }
  GPUSupportedFeatures* features() const;
  GPUSupportedLimits* limits() const { return limits_; }

  // Software adapters are not currently supported.
  bool isSoftware() const { return false; }

  ScriptPromise requestDevice(ScriptState* script_state,
                              GPUDeviceDescriptor* descriptor);

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
                               WGPUDevice dawn_device);
  void InitializeFeatureNameList();

  String name_;
  uint32_t adapter_service_id_;
  WGPUDeviceProperties adapter_properties_;
  Member<GPU> gpu_;
  Member<GPUSupportedFeatures> features_;
  Member<GPUSupportedLimits> limits_;

  static constexpr int kMaxAllowedConsoleWarnings = 50;
  int allowed_console_warnings_remaining_ = kMaxAllowedConsoleWarnings;

  DISALLOW_COPY_AND_ASSIGN(GPUAdapter);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_ADAPTER_H_
