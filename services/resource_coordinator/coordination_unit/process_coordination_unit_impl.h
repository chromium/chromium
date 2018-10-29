// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PROCESS_COORDINATION_UNIT_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PROCESS_COORDINATION_UNIT_IMPL_H_

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"

namespace resource_coordinator {

class FrameCoordinationUnitImpl;

class ProcessCoordinationUnitImpl
    : public CoordinationUnitInterface<ProcessCoordinationUnitImpl,
                                       mojom::ProcessCoordinationUnit,
                                       mojom::ProcessCoordinationUnitRequest> {
 public:
  static CoordinationUnitType Type() { return CoordinationUnitType::kProcess; }

  ProcessCoordinationUnitImpl(
      const CoordinationUnitID& id,
      CoordinationUnitGraph* graph,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~ProcessCoordinationUnitImpl() override;

  // mojom::ProcessCoordinationUnit implementation.
  void AddFrame(const CoordinationUnitID& cu_id) override;
  void SetCPUUsage(double cpu_usage) override;
  void SetExpectedTaskQueueingDuration(base::TimeDelta duration) override;
  void SetLaunchTime(base::Time launch_time) override;
  void SetMainThreadTaskLoadIsLow(bool main_thread_task_load_is_low) override;
  void SetPID(base::ProcessId pid) override;
  void OnRendererIsBloated() override;

  // Private implementation properties.
  void set_private_footprint_kb(uint64_t private_footprint_kb) {
    private_footprint_kb_ = private_footprint_kb;
  }
  uint64_t private_footprint_kb() const { return private_footprint_kb_; }
  void set_cumulative_cpu_usage(base::TimeDelta cumulative_cpu_usage) {
    cumulative_cpu_usage_ = cumulative_cpu_usage;
  }
  base::TimeDelta cumulative_cpu_usage() const { return cumulative_cpu_usage_; }

  const std::set<FrameCoordinationUnitImpl*>& GetFrameCoordinationUnits() const;
  std::set<PageCoordinationUnitImpl*> GetAssociatedPageCoordinationUnits()
      const;

  base::ProcessId process_id() const { return process_id_; }
  base::Time launch_time() const { return launch_time_; }

  // Removes |frame_cu| from the set of frames hosted by this process. Invoked
  // from the destructor of FrameCoordinationUnitImpl.
  void RemoveFrame(FrameCoordinationUnitImpl* frame_cu);

  // Invoked when the state of a frame hosted by this process changes.
  void OnFrameLifecycleStateChanged(FrameCoordinationUnitImpl* frame_cu,
                                    mojom::LifecycleState old_state);

 private:
  // CoordinationUnitInterface implementation.
  void OnEventReceived(mojom::Event event) override;
  void OnPropertyChanged(mojom::PropertyType property_type,
                         int64_t value) override;

  void AddFrameImpl(FrameCoordinationUnitImpl* frame_cu);

  void DecrementNumFrozenFrames();
  void IncrementNumFrozenFrames();

  base::TimeDelta cumulative_cpu_usage_;
  uint64_t private_footprint_kb_ = 0u;

  base::ProcessId process_id_ = base::kNullProcessId;
  base::Time launch_time_;

  std::set<FrameCoordinationUnitImpl*> frame_coordination_units_;

  // The number of frames hosted by this process that are frozen.
  int num_frozen_frames_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ProcessCoordinationUnitImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PROCESS_COORDINATION_UNIT_IMPL_H_
