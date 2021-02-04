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

class GPUDeviceDescriptor;
class ScriptPromiseResolver;

class GPUAdapter final : public ScriptWrappable, public DawnObjectBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  GPUAdapter(const String& name,
             uint32_t adapter_service_id,
             const WGPUDeviceProperties& properties,
             scoped_refptr<DawnControlClientHolder> dawn_control_client);

  const String& name() const;
  Vector<String> extensions(ScriptState* script_state) const;

  ScriptPromise requestDevice(ScriptState* script_state,
                              const GPUDeviceDescriptor* descriptor);

 private:
  void OnRequestDeviceCallback(ScriptPromiseResolver* resolver,
                               const GPUDeviceDescriptor* descriptor,
                               WGPUDevice dawn_device);
  void InitializeExtensionNameList();

  String name_;
  uint32_t adapter_service_id_;
  WGPUDeviceProperties adapter_properties_;
  Vector<String> extension_name_list_;

  DISALLOW_COPY_AND_ASSIGN(GPUAdapter);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_ADAPTER_H_
