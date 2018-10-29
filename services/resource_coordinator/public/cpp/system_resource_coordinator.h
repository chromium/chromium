// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_SYSTEM_RESOURCE_COORDINATOR_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_SYSTEM_RESOURCE_COORDINATOR_H_

#include "services/resource_coordinator/public/cpp/resource_coordinator_interface.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace resource_coordinator {

class COMPONENT_EXPORT(SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP)
    SystemResourceCoordinator : public ResourceCoordinatorInterface<
                                    mojom::SystemCoordinationUnitPtr,
                                    mojom::SystemCoordinationUnitRequest> {
 public:
  SystemResourceCoordinator(service_manager::Connector* connector);
  ~SystemResourceCoordinator() override;

  void DistributeMeasurementBatch(
      mojom::ProcessResourceMeasurementBatchPtr batch);

 private:
  void ConnectToService(mojom::CoordinationUnitProviderPtr& provider,
                        const CoordinationUnitID& cu_id) override;

  DISALLOW_COPY_AND_ASSIGN(SystemResourceCoordinator);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_SYSTEM_RESOURCE_COORDINATOR_H_
