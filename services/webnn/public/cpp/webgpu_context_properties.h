// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_WEBGPU_CONTEXT_PROPERTIES_H_
#define SERVICES_WEBNN_PUBLIC_CPP_WEBGPU_CONTEXT_PROPERTIES_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

struct WGPUDeviceImpl;
struct DawnProcTable;

namespace webnn {

struct WebGpuContextProperties {
  raw_ptr<WGPUDeviceImpl> wgpu_device = nullptr;
  raw_ptr<const DawnProcTable> dawn_procs = nullptr;
  // Closure invoked to flush command buffer IPC across the Dawn Wire channel
  // during buffer readback. This closure also captures underlying C++ handles
  // (such as DawnControlClientHolder and wgpu::Device) to keep them alive for
  // the duration of the WebNN context.
  base::RepeatingClosure webgpu_flush;

  bool IsValid() const {
    return wgpu_device != nullptr && dawn_procs != nullptr &&
           !webgpu_flush.is_null();
  }
};

using WebGpuContextHelperOnceCallback =
    base::OnceCallback<void(WebGpuContextProperties)>;

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_WEBGPU_CONTEXT_PROPERTIES_H_
