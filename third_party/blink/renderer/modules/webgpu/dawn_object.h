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
  explicit DawnObjectBase(
      scoped_refptr<DawnControlClientHolder> dawn_control_client);

  const scoped_refptr<DawnControlClientHolder>& GetDawnControlClient() const;
  bool IsDawnControlClientDestroyed() const;
  gpu::webgpu::WebGPUInterface* GetInterface() const;
  const DawnProcTable& GetProcs() const;

 private:
  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
};

// This class allows objects to hold onto a DawnControlClientHolder and a
// device client id. Now one GPUDevice is related to one WebGPUSerializer in
// the client side of WebGPU context. When the GPUDevice and all the other
// WebGPU objects that are created from the GPUDevice are destroyed, this
// object will be destroyed and in the destructor of this object we will
// trigger the clean-ups to the corresponding WebGPUSerailzer and other data
// structures in the GPU process.
class DawnDeviceClientSerializerHolder
    : public RefCounted<DawnDeviceClientSerializerHolder> {
 public:
  DawnDeviceClientSerializerHolder(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      uint64_t device_client_id);

 private:
  friend class RefCounted<DawnDeviceClientSerializerHolder>;
  friend class DeviceTreeObject;
  ~DawnDeviceClientSerializerHolder();

  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  uint64_t device_client_id_;
};

// This class is the parent of GPUDevice and all the WebGPU objects that are
// created from a GPUDevice, which holds a
// scoped_refptr<DawnDeviceClientSerializerHolder> and provides functions to
// access all the members inside it. When a GPUDevice and all the WebGPU
// objects created from it are destroyed, the refcount of
// DawnDeviceClientSerializerHolder will become 0 and the clean-ups to the
// corresponding WebGPUSerailzer and other data structures in the GPU process
// will be triggered.
class DeviceTreeObject {
 public:
  explicit DeviceTreeObject(scoped_refptr<DawnDeviceClientSerializerHolder>
                                device_client_seralizer_holder)
      : device_client_serializer_holder_(
            std::move(device_client_seralizer_holder)) {}

  const scoped_refptr<DawnControlClientHolder>& GetDawnControlClient() const;
  bool IsDawnControlClientDestroyed() const;
  gpu::webgpu::WebGPUInterface* GetInterface() const;
  const DawnProcTable& GetProcs() const;

  uint64_t GetDeviceClientID() const;

  // Ensure commands up until now on this object's parent device are flushed by
  // the end of the task.
  void EnsureFlush();

 protected:
  scoped_refptr<DawnDeviceClientSerializerHolder>
      device_client_serializer_holder_;
};

class DawnObjectImpl : public ScriptWrappable, public DeviceTreeObject {
 public:
  explicit DawnObjectImpl(GPUDevice* device);
  ~DawnObjectImpl() override;

  void Trace(Visitor* visitor) const override;

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
class DawnObject<WGPUDevice> : public DeviceTreeObject {
 public:
  DawnObject(scoped_refptr<DawnControlClientHolder> dawn_control_client,
             uint64_t device_client_id,
             WGPUDevice handle)
      : DeviceTreeObject(base::MakeRefCounted<DawnDeviceClientSerializerHolder>(
            std::move(dawn_control_client),
            device_client_id)),
        handle_(handle) {}

  WGPUDevice GetHandle() const { return handle_; }

  const scoped_refptr<DawnDeviceClientSerializerHolder>&
  GetDeviceClientSerializerHolder() const {
    return device_client_serializer_holder_;
  }

 private:
  WGPUDevice const handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_OBJECT_H_
