// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_REGISTRY_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_REGISTRY_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/process/process_handle.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

// Interface to register client processes and heap profilers with the memory
// instrumentation coordinator. This is considered privileged and the browser
// should be the only client.
class COMPONENT_EXPORT(
    RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION) Registry {
 public:
  virtual ~Registry() = default;

  virtual void RegisterHeapProfiler(
      mojo::PendingRemote<mojom::HeapProfiler> profiler,
      mojo::PendingReceiver<mojom::HeapProfilerHelper> helper_receiver) = 0;

  // Must be called once for each client process, including the browser process.
  // |client_process| is an endpoint the service can use to push client events
  // to the process. |process_type|, |process_id| and |service_name| are
  // considered to be authoritative information about the client process
  // (verified by the browser process).
  virtual void RegisterClientProcess(
      mojo::PendingReceiver<mojom::Coordinator> receiver,
      mojo::PendingRemote<mojom::ClientProcess> client_process,
      mojom::ProcessType process_type,
      base::ProcessId process_id,
      const std::optional<std::string>& service_name) = 0;
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_REGISTRY_H_
