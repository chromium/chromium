// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_graph.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_provider_impl.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/system_coordination_unit_impl.h"
#include "services/resource_coordinator/observers/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace ukm {
class UkmEntryBuilder;
}  // namespace ukm

namespace resource_coordinator {

CoordinationUnitGraph::CoordinationUnitGraph()
    : system_coordination_unit_id_(CoordinationUnitType::kSystem,
                                   CoordinationUnitID::RANDOM_ID) {}

CoordinationUnitGraph::~CoordinationUnitGraph() {
  // Because the graph has ownership of the CUs, and because the process CUs
  // unregister on destruction, there is reentrancy to this class on
  // destruction. The order of operations here is optimized to minimize the work
  // done on destruction, as well as to make sure the cleanup is independent of
  // the declaration order of member variables.

  // Kill all the observers first.
  observers_.clear();
  // Then clear up the CUs to ensure this happens before the PID map is
  // destructed.
  coordination_units_.clear();

  DCHECK_EQ(0u, processes_by_pid_.size());
}

void CoordinationUnitGraph::OnStart(
    service_manager::BinderRegistryWithArgs<
        const service_manager::BindSourceInfo&>* registry,
    service_manager::ServiceContextRefFactory* service_ref_factory) {
  // Create the singleton CoordinationUnitProvider.
  provider_ =
      std::make_unique<CoordinationUnitProviderImpl>(service_ref_factory, this);
  registry->AddInterface(base::BindRepeating(
      &CoordinationUnitProviderImpl::Bind, base::Unretained(provider_.get())));
}

void CoordinationUnitGraph::RegisterObserver(
    std::unique_ptr<CoordinationUnitGraphObserver> observer) {
  observer->set_coordination_unit_graph(this);
  observers_.push_back(std::move(observer));
}

void CoordinationUnitGraph::OnCoordinationUnitCreated(
    CoordinationUnitBase* coordination_unit) {
  for (auto& observer : observers_) {
    if (observer->ShouldObserve(coordination_unit)) {
      coordination_unit->AddObserver(observer.get());
      observer->OnCoordinationUnitCreated(coordination_unit);
    }
  }
}

void CoordinationUnitGraph::OnBeforeCoordinationUnitDestroyed(
    CoordinationUnitBase* coordination_unit) {
  coordination_unit->BeforeDestroyed();
}

FrameCoordinationUnitImpl* CoordinationUnitGraph::CreateFrameCoordinationUnit(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
  return FrameCoordinationUnitImpl::Create(id, this, std::move(service_ref));
}

PageCoordinationUnitImpl* CoordinationUnitGraph::CreatePageCoordinationUnit(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
  return PageCoordinationUnitImpl::Create(id, this, std::move(service_ref));
}

ProcessCoordinationUnitImpl*
CoordinationUnitGraph::CreateProcessCoordinationUnit(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
  return ProcessCoordinationUnitImpl::Create(id, this, std::move(service_ref));
}

SystemCoordinationUnitImpl*
CoordinationUnitGraph::FindOrCreateSystemCoordinationUnit(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
  CoordinationUnitBase* system_cu =
      GetCoordinationUnitByID(system_coordination_unit_id_);
  if (system_cu)
    return SystemCoordinationUnitImpl::FromCoordinationUnitBase(system_cu);

  // Create the singleton SystemCU instance. Ownership is taken by the graph.
  return SystemCoordinationUnitImpl::Create(system_coordination_unit_id_, this,
                                            std::move(service_ref));
}

CoordinationUnitBase* CoordinationUnitGraph::GetCoordinationUnitByID(
    const CoordinationUnitID cu_id) {
  const auto& it = coordination_units_.find(cu_id);
  if (it == coordination_units_.end())
    return nullptr;
  return it->second.get();
}

ProcessCoordinationUnitImpl*
CoordinationUnitGraph::GetProcessCoordinationUnitByPid(base::ProcessId pid) {
  auto it = processes_by_pid_.find(pid);
  if (it == processes_by_pid_.end())
    return nullptr;

  return ProcessCoordinationUnitImpl::FromCoordinationUnitBase(it->second);
}

std::vector<ProcessCoordinationUnitImpl*>
CoordinationUnitGraph::GetAllProcessCoordinationUnits() {
  return GetAllCoordinationUnitsOfType<ProcessCoordinationUnitImpl>();
}

std::vector<FrameCoordinationUnitImpl*>
CoordinationUnitGraph::GetAllFrameCoordinationUnits() {
  return GetAllCoordinationUnitsOfType<FrameCoordinationUnitImpl>();
}

std::vector<PageCoordinationUnitImpl*>
CoordinationUnitGraph::GetAllPageCoordinationUnits() {
  return GetAllCoordinationUnitsOfType<PageCoordinationUnitImpl>();
}

CoordinationUnitBase* CoordinationUnitGraph::AddNewCoordinationUnit(
    std::unique_ptr<CoordinationUnitBase> new_cu) {
  auto it = coordination_units_.emplace(new_cu->id(), std::move(new_cu));
  DCHECK(it.second);  // Inserted successfully

  CoordinationUnitBase* added_cu = it.first->second.get();
  OnCoordinationUnitCreated(added_cu);

  return added_cu;
}

void CoordinationUnitGraph::DestroyCoordinationUnit(CoordinationUnitBase* cu) {
  OnBeforeCoordinationUnitDestroyed(cu);

  size_t erased = coordination_units_.erase(cu->id());
  DCHECK_EQ(1u, erased);
}

void CoordinationUnitGraph::BeforeProcessPidChange(
    ProcessCoordinationUnitImpl* process,
    base::ProcessId new_pid) {
  // On Windows, PIDs are agressively reused, and because not all process
  // creation/death notifications are synchronized, it's possible for more than
  // one CU to have the same PID. To handle this, the second and subsequent
  // registration override earlier registrations, while unregistration will only
  // unregister the current holder of the PID.
  if (process->process_id() != base::kNullProcessId) {
    auto it = processes_by_pid_.find(process->process_id());
    if (it != processes_by_pid_.end() && it->second == process)
      processes_by_pid_.erase(it);
  }
  if (new_pid != base::kNullProcessId)
    processes_by_pid_[new_pid] = process;
}

template <typename CUType>
std::vector<CUType*> CoordinationUnitGraph::GetAllCoordinationUnitsOfType() {
  const auto type = CUType::Type();
  std::vector<CUType*> ret;
  for (const auto& el : coordination_units_) {
    if (el.first.type == type)
      ret.push_back(CUType::FromCoordinationUnitBase(el.second.get()));
  }
  return ret;
}

}  // namespace resource_coordinator
