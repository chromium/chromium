// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_FRAME_RESOURCE_COORDINATOR_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_FRAME_RESOURCE_COORDINATOR_H_

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_interface.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace resource_coordinator {

class COMPONENT_EXPORT(SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP)
    FrameResourceCoordinator
    : public ResourceCoordinatorInterface<mojom::FrameCoordinationUnitPtr,
                                          mojom::FrameCoordinationUnitRequest> {
 public:
  FrameResourceCoordinator(service_manager::Connector* connector);
  ~FrameResourceCoordinator() override;

  void SetAudibility(bool audible);
  void OnAlertFired();
  void AddChildFrame(const FrameResourceCoordinator& child);
  void RemoveChildFrame(const FrameResourceCoordinator& child);

 private:
  void ConnectToService(mojom::CoordinationUnitProviderPtr& provider,
                        const CoordinationUnitID& cu_id) override;

  void AddChildFrameByID(const CoordinationUnitID& child_id);
  void RemoveChildFrameByID(const CoordinationUnitID& child_id);

  THREAD_CHECKER(thread_checker_);

  // The WeakPtrFactory should come last so the weak ptrs are invalidated
  // before the rest of the member variables.
  base::WeakPtrFactory<FrameResourceCoordinator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameResourceCoordinator);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_FRAME_RESOURCE_COORDINATOR_H_
