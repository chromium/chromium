// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_OBJECT_H_

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu.h>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"

namespace gpu {
namespace webgpu {

class WebGPUInterface;

}  // namespace webgpu
}  // namespace gpu

namespace blink {

class GPUDevice;
class Visitor;

// This class allows objects to hold onto a DawnControlClientHolder.
// The DawnControlClientHolder is used to hold the WebGPUInterface and keep
// track of whether or not the client has been destroyed. If the client is
// destroyed, we should not call any Dawn functions.
class DawnObjectBase {
 public:
  DawnObjectBase(scoped_refptr<DawnControlClientHolder> dawn_control_client);

  const scoped_refptr<DawnControlClientHolder>& GetDawnControlClient() const;
  bool IsDawnControlClientDestroyed() const;
  gpu::webgpu::WebGPUInterface* GetInterface() const;
  const DawnProcTable& GetProcs() const;

 private:
  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
};

class DawnObjectImpl : public ScriptWrappable, public DawnObjectBase {
 public:
  DawnObjectImpl(GPUDevice* device);
  ~DawnObjectImpl() override;

  void Trace(blink::Visitor* visitor) override;

 protected:
  Member<GPUDevice> device_;
};

template <typename Handle>
class DawnObject : public DawnObjectImpl {
 public:
  DawnObject(GPUDevice* device, Handle handle)
      : DawnObjectImpl(device), handle_(handle) {}
  ~DawnObject() override = default;

  Handle GetHandle() const { return handle_; }

 private:
  Handle const handle_;
};

template <>
class DawnObject<WGPUDevice> : public DawnObjectBase {
 public:
  DawnObject(scoped_refptr<DawnControlClientHolder> dawn_control_client,
             WGPUDevice handle)
      : DawnObjectBase(std::move(dawn_control_client)), handle_(handle) {}

  WGPUDevice GetHandle() const { return handle_; }

 private:
  WGPUDevice const handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_OBJECT_H_
