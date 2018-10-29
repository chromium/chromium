// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_SERVICE_H_
#define SERVICES_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_graph.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_introspector_impl.h"
#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"
#include "services/resource_coordinator/webui_graph_dump_impl.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace resource_coordinator {

class ResourceCoordinatorService : public service_manager::Service {
 public:
  ResourceCoordinatorService();
  ~ResourceCoordinatorService() override;

  // service_manager::Service:
  // Factory function for use as an embedded service.
  static std::unique_ptr<service_manager::Service> Create();

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  service_manager::ServiceContextRefFactory* ref_factory() {
    return ref_factory_.get();
  }
  ukm::MojoUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }
  CoordinationUnitGraph* coordination_unit_graph() {
    return &coordination_unit_graph_;
  }

 private:
  void BindWebUIGraphDump(mojom::WebUIGraphDumpRequest request,
                          const service_manager::BindSourceInfo& source_info);
  void OnGraphDumpConnectionError(WebUIGraphDumpImpl* graph_dump);

  service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>
      registry_;
  CoordinationUnitGraph coordination_unit_graph_;
  CoordinationUnitIntrospectorImpl introspector_;
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;
  std::unique_ptr<memory_instrumentation::CoordinatorImpl>
      memory_instrumentation_coordinator_;
  std::unique_ptr<service_manager::ServiceContextRefFactory> ref_factory_;

  // Current graph dump instances.
  std::vector<std::unique_ptr<WebUIGraphDumpImpl>> graph_dumps_;

  // WeakPtrFactory members should always come last so WeakPtrs are destructed
  // before other members.
  base::WeakPtrFactory<ResourceCoordinatorService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorService);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_SERVICE_H_
