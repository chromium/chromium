// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_RESOURCE_COORDINATOR_INTERFACE_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_RESOURCE_COORDINATOR_INTERFACE_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/macros.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/mojom/coordination_unit_provider.mojom.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace resource_coordinator {

template <class CoordinationUnitMojoPtr, class CoordinationUnitMojoRequest>
class ResourceCoordinatorInterface {
 public:
  ResourceCoordinatorInterface() = default;
  virtual ~ResourceCoordinatorInterface() = default;

  void AddBinding(CoordinationUnitMojoRequest request) {
    if (!service_)
      return;
    service_->AddBinding(std::move(request));
  }

  // Returns the ID. Note that this is meaningless for a singleton CU.
  CoordinationUnitID id() const { return cu_id_; }

  // Returns the remote endpoint interface.
  const CoordinationUnitMojoPtr& service() const { return service_; }

  // Expose the GetID function for testing.
  using CoordinationUnitMojo = typename CoordinationUnitMojoPtr::InterfaceType;
  using GetIDCallback = typename CoordinationUnitMojo::GetIDCallback;
  void GetID(GetIDCallback callback) {
    if (!service_)
      std::move(callback).Run(cu_id_);
    service_->GetID(std::move(callback));
  }

 protected:
  virtual void ConnectToService(mojom::CoordinationUnitProviderPtr& provider,
                                const CoordinationUnitID& cu_id) = 0;

  void ConnectToService(service_manager::Connector* connector,
                        const CoordinationUnitID& cu_id) {
    if (!connector)
      return;
    cu_id_ = cu_id;
    mojom::CoordinationUnitProviderPtr provider;
    connector->BindInterface(mojom::kServiceName, mojo::MakeRequest(&provider));
    ConnectToService(provider, cu_id);
  }

  CoordinationUnitMojoPtr service_;
  CoordinationUnitID cu_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorInterface);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_RESOURCE_COORDINATOR_INTERFACE_H_
