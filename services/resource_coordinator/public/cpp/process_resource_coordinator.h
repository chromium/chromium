// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_PROCESS_RESOURCE_COORDINATOR_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_PROCESS_RESOURCE_COORDINATOR_H_

#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_checker.h"
#include "services/resource_coordinator/public/cpp/frame_resource_coordinator.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_interface.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace resource_coordinator {

class COMPONENT_EXPORT(SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP)
    ProcessResourceCoordinator : public ResourceCoordinatorInterface<
                                     mojom::ProcessCoordinationUnitPtr,
                                     mojom::ProcessCoordinationUnitRequest> {
 public:
  ProcessResourceCoordinator(service_manager::Connector* connector);
  ~ProcessResourceCoordinator() override;

  void SetCPUUsage(double usage);
  void SetLaunchTime(base::Time launch_time);
  void SetPID(base::ProcessId pid);

  void AddFrame(const FrameResourceCoordinator& frame);

 private:
  void ConnectToService(mojom::CoordinationUnitProviderPtr& provider,
                        const CoordinationUnitID& cu_id) override;

  void AddFrameByID(const CoordinationUnitID& cu_id);

  THREAD_CHECKER(thread_checker_);

  // The WeakPtrFactory should come last so the weak ptrs are invalidated
  // before the rest of the member variables.
  base::WeakPtrFactory<ProcessResourceCoordinator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProcessResourceCoordinator);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_PROCESS_RESOURCE_COORDINATOR_H_
