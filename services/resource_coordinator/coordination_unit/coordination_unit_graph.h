// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_GRAPH_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_GRAPH_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace service_manager {
template <typename... BinderArgs>
class BinderRegistryWithArgs;
struct BindSourceInfo;
class ServiceContextRefFactory;
}  // namespace service_manager

namespace resource_coordinator {

class CoordinationUnitBase;
class CoordinationUnitGraphObserver;
class CoordinationUnitProviderImpl;
class FrameCoordinationUnitImpl;
class PageCoordinationUnitImpl;
class ProcessCoordinationUnitImpl;
class SystemCoordinationUnitImpl;

// The CoordinationUnitGraph represents a graph of the coordination units
// representing a single system. It vends out new instances of coordination
// units and indexes them by ID. It also fires the creation and pre-destruction
// notifications for all coordination units.
class CoordinationUnitGraph {
 public:
  CoordinationUnitGraph();
  ~CoordinationUnitGraph();

  void set_ukm_recorder(ukm::UkmRecorder* ukm_recorder) {
    ukm_recorder_ = ukm_recorder;
  }
  ukm::UkmRecorder* ukm_recorder() const { return ukm_recorder_; }

  void OnStart(service_manager::BinderRegistryWithArgs<
                   const service_manager::BindSourceInfo&>* registry,
               service_manager::ServiceContextRefFactory* service_ref_factory);
  void RegisterObserver(
      std::unique_ptr<CoordinationUnitGraphObserver> observer);
  void OnCoordinationUnitCreated(CoordinationUnitBase* coordination_unit);
  void OnBeforeCoordinationUnitDestroyed(
      CoordinationUnitBase* coordination_unit);

  FrameCoordinationUnitImpl* CreateFrameCoordinationUnit(
      const CoordinationUnitID& id,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  PageCoordinationUnitImpl* CreatePageCoordinationUnit(
      const CoordinationUnitID& id,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ProcessCoordinationUnitImpl* CreateProcessCoordinationUnit(
      const CoordinationUnitID& id,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  SystemCoordinationUnitImpl* FindOrCreateSystemCoordinationUnit(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);

  std::vector<ProcessCoordinationUnitImpl*> GetAllProcessCoordinationUnits();
  std::vector<FrameCoordinationUnitImpl*> GetAllFrameCoordinationUnits();
  std::vector<PageCoordinationUnitImpl*> GetAllPageCoordinationUnits();

  // Retrieves the process CU with PID |pid|, if any.
  ProcessCoordinationUnitImpl* GetProcessCoordinationUnitByPid(
      base::ProcessId pid);
  CoordinationUnitBase* GetCoordinationUnitByID(const CoordinationUnitID cu_id);

  std::vector<std::unique_ptr<CoordinationUnitGraphObserver>>&
  observers_for_testing() {
    return observers_;
  }

 private:
  using CUIDMap = std::unordered_map<CoordinationUnitID,
                                     std::unique_ptr<CoordinationUnitBase>>;
  using ProcessByPidMap =
      std::unordered_map<base::ProcessId, ProcessCoordinationUnitImpl*>;

  // Lifetime management functions for CoordinationUnitBase.
  friend class CoordinationUnitBase;
  CoordinationUnitBase* AddNewCoordinationUnit(
      std::unique_ptr<CoordinationUnitBase> new_cu);
  void DestroyCoordinationUnit(CoordinationUnitBase* cu);

  // Process PID map for use by ProcessCoordinationUnitImpl.
  friend class ProcessCoordinationUnitImpl;
  void BeforeProcessPidChange(ProcessCoordinationUnitImpl* process,
                              base::ProcessId new_pid);

  template <typename CUType>
  std::vector<CUType*> GetAllCoordinationUnitsOfType();

  CoordinationUnitID system_coordination_unit_id_;
  CUIDMap coordination_units_;
  ProcessByPidMap processes_by_pid_;
  std::vector<std::unique_ptr<CoordinationUnitGraphObserver>> observers_;
  ukm::UkmRecorder* ukm_recorder_ = nullptr;
  std::unique_ptr<CoordinationUnitProviderImpl> provider_;

  static void Create(
      service_manager::ServiceContextRefFactory* service_ref_factory);

  DISALLOW_COPY_AND_ASSIGN(CoordinationUnitGraph);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_GRAPH_H_
