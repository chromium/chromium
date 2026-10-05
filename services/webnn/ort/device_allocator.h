// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_DEVICE_ALLOCATOR_H_
#define SERVICES_WEBNN_ORT_DEVICE_ALLOCATOR_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "services/webnn/ort/ort_session_options.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-forward.h"

namespace webnn::ort {

class Environment;

// `DeviceAllocator` wraps a device allocator created from a trivial session.
// The allocator can create device tensors for a specific EP used by the
// session.
class DeviceAllocator final : public base::RefCounted<DeviceAllocator> {
 public:
  // Returns a device allocator for a specific EP if it can be created
  // successfully; otherwise, returns nullptr.
  // TODO(crbug.com/445971854): Use device allocator to create tensors for
  // other EPs.
  static scoped_refptr<DeviceAllocator> Create(
      scoped_refptr<SessionOptions> session_options,
      scoped_refptr<Environment> env);

  DeviceAllocator(base::PassKey<DeviceAllocator>,
                  scoped_refptr<Environment> env,
                  ScopedOrtSession trivial_session,
                  ScopedOrtAllocator device_allocator);

  DeviceAllocator(const DeviceAllocator&) = delete;
  DeviceAllocator& operator=(const DeviceAllocator&) = delete;

  OrtAllocator* get() const { return device_allocator_.get(); }

  // Returns whether tensors from this allocator can be read and written
  // directly by the CPU. Device-only memory is not mappable, so callers must
  // consult this before taking a span over a tensor's contents.
  bool CanAccessOnCpu() const { return can_access_on_cpu_; }

  // Returns whether `tensor_info` should be allocated on the device. Tensors
  // the renderer reads or writes need CPU-reachable memory, so they stay on
  // the default CPU allocator when this allocator hands out device-only
  // memory. Everything else, notably KV caches, stays device resident.
  bool ShouldUse(const mojom::TensorInfo& tensor_info) const;

 private:
  friend class base::RefCounted<DeviceAllocator>;

  ~DeviceAllocator();

  // The environment must outlive the session and allocator because their
  // destruction may call into execution provider DLLs that could potentially
  // be unloaded when the environment is released. Hold a reference to
  // prevent premature environment release.
  scoped_refptr<Environment> env_;

  // The trivial session is only used to keep the allocator valid. It is not
  // used for inference.
  // It must be declared before the allocator because the allocator wraps the
  // internal allocator from the session and becomes invalid when the session
  // does.
  ScopedOrtSession trivial_session_;
  ScopedOrtAllocator device_allocator_;
  const bool can_access_on_cpu_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_DEVICE_ALLOCATOR_H_
