// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_DEVICE_ALLOCATOR_H_
#define SERVICES_WEBNN_ORT_DEVICE_ALLOCATOR_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/cstring_view.h"
#include "services/webnn/ort/ort_session_options.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-forward.h"

namespace webnn::ort {

class Environment;

// `DeviceAllocator` wraps a device allocator created from a trivial session.
// The allocator can create device tensors for a specific EP used by the
// session. Currently, the device allocator is only used for OpenVINO EP.
class DeviceAllocator final : public base::RefCounted<DeviceAllocator> {
 public:
  // Returns a device allocator for a specific EP if it can be created
  // successfully; otherwise, returns nullptr. Currently, using device allocator
  // to create device tensors is only supported for OpenVINO EP.
  // TODO(crbug.com/445971854): Use device allocator to create tensors for
  // other EPs.
  static scoped_refptr<DeviceAllocator> Create(
      mojom::Device device_type,
      const OrtSessionOptions* session_options,
      scoped_refptr<Environment> env);

  DeviceAllocator(base::PassKey<DeviceAllocator>,
                  ScopedOrtSession trivial_session,
                  ScopedOrtAllocator device_allocator,
                  base::cstring_view ep_name);

  DeviceAllocator(const DeviceAllocator&) = delete;
  DeviceAllocator& operator=(const DeviceAllocator&) = delete;

  OrtAllocator* get() const { return device_allocator_.get(); }

  // Whether to use this device allocator depends on the tensor's
  // usage in `tensor_info`.
  bool ShouldUse(const mojom::TensorInfoPtr& tensor_info) const;
  // Whether the underlying tensor data can be accessed on CPU directly.
  bool CanAccessOnCPU() const { return ep_name_ != kWebGpuExecutionProvider; }

 private:
  friend class base::RefCounted<DeviceAllocator>;

  ~DeviceAllocator();

  // The trivial session is only used to keep the allocator valid. It is not
  // used for inference.
  // It must be declared before the allocator because the allocator wraps the
  // internal allocator from the session and becomes invalid when the session
  // does.
  ScopedOrtSession trivial_session_;
  ScopedOrtAllocator device_allocator_;

  // The name of the EP associated with this allocator.
  std::string ep_name_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_DEVICE_ALLOCATOR_H_
