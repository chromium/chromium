// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_SERVICE_H_
#define SERVICES_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_SERVICE_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"
#include "services/resource_coordinator/public/mojom/resource_coordinator_service.mojom.h"

namespace resource_coordinator {

class ResourceCoordinatorService : public mojom::ResourceCoordinatorService {
 public:
  explicit ResourceCoordinatorService(
      mojo::PendingReceiver<mojom::ResourceCoordinatorService> receiver);
  ~ResourceCoordinatorService() override;

  // mojom::ResourceCoordinatorService implementation:
  void BindMemoryInstrumentationCoordinatorController(
      mojo::PendingReceiver<
          memory_instrumentation::mojom::CoordinatorController> receiver)
      override;
  void RegisterHeapProfiler(
      mojo::PendingRemote<memory_instrumentation::mojom::HeapProfiler> profiler,
      mojo::PendingReceiver<memory_instrumentation::mojom::HeapProfilerHelper>
          receiver) override;

 private:
  const mojo::Receiver<mojom::ResourceCoordinatorService> receiver_;
  memory_instrumentation::CoordinatorImpl memory_instrumentation_coordinator_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorService);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_SERVICE_H_
