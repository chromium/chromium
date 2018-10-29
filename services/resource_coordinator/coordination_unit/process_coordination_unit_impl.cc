// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"

#include "base/logging.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"

namespace resource_coordinator {

ProcessCoordinationUnitImpl::ProcessCoordinationUnitImpl(
    const CoordinationUnitID& id,
    CoordinationUnitGraph* graph,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : CoordinationUnitInterface(id, graph, std::move(service_ref)) {}

ProcessCoordinationUnitImpl::~ProcessCoordinationUnitImpl() {
  // Make as if we're transitioning to the null PID before we die to clear this
  // instance from the PID map.
  if (process_id_ != base::kNullProcessId)
    graph()->BeforeProcessPidChange(this, base::kNullProcessId);

  for (auto* child_frame : frame_coordination_units_)
    child_frame->RemoveProcessCoordinationUnit(this);
}

void ProcessCoordinationUnitImpl::AddFrame(const CoordinationUnitID& cu_id) {
  DCHECK(cu_id.type == CoordinationUnitType::kFrame);
  auto* frame_cu =
      FrameCoordinationUnitImpl::GetCoordinationUnitByID(graph_, cu_id);
  if (frame_cu)
    AddFrameImpl(frame_cu);
}

void ProcessCoordinationUnitImpl::SetCPUUsage(double cpu_usage) {
  SetProperty(mojom::PropertyType::kCPUUsage, cpu_usage * 1000);
}

void ProcessCoordinationUnitImpl::SetExpectedTaskQueueingDuration(
    base::TimeDelta duration) {
  SetProperty(mojom::PropertyType::kExpectedTaskQueueingDuration,
              duration.InMilliseconds());
}

void ProcessCoordinationUnitImpl::SetLaunchTime(base::Time launch_time) {
  DCHECK(launch_time_.is_null());
  launch_time_ = launch_time;
}

void ProcessCoordinationUnitImpl::SetMainThreadTaskLoadIsLow(
    bool main_thread_task_load_is_low) {
  SetProperty(mojom::PropertyType::kMainThreadTaskLoadIsLow,
              main_thread_task_load_is_low);
}

void ProcessCoordinationUnitImpl::SetPID(base::ProcessId pid) {
  // The PID can only be set once.
  DCHECK_EQ(process_id_, base::kNullProcessId);

  graph()->BeforeProcessPidChange(this, pid);
  process_id_ = pid;
  SetProperty(mojom::PropertyType::kPID, pid);
}

void ProcessCoordinationUnitImpl::OnRendererIsBloated() {
  SendEvent(mojom::Event::kRendererIsBloated);
}

const std::set<FrameCoordinationUnitImpl*>&
ProcessCoordinationUnitImpl::GetFrameCoordinationUnits() const {
  return frame_coordination_units_;
}

// There is currently not a direct relationship between processes and
// pages. However, frames are children of both processes and frames, so we
// find all of the pages that are reachable from the process's child
// frames.
std::set<PageCoordinationUnitImpl*>
ProcessCoordinationUnitImpl::GetAssociatedPageCoordinationUnits() const {
  std::set<PageCoordinationUnitImpl*> page_cus;
  for (auto* frame_cu : frame_coordination_units_) {
    if (auto* page_cu = frame_cu->GetPageCoordinationUnit())
      page_cus.insert(page_cu);
  }
  return page_cus;
}

void ProcessCoordinationUnitImpl::OnFrameLifecycleStateChanged(
    FrameCoordinationUnitImpl* frame_cu,
    mojom::LifecycleState old_state) {
  DCHECK(base::ContainsKey(frame_coordination_units_, frame_cu));
  DCHECK_NE(old_state, frame_cu->lifecycle_state());

  if (old_state == mojom::LifecycleState::kFrozen)
    DecrementNumFrozenFrames();
  else if (frame_cu->lifecycle_state() == mojom::LifecycleState::kFrozen)
    IncrementNumFrozenFrames();
}

void ProcessCoordinationUnitImpl::OnEventReceived(mojom::Event event) {
  for (auto& observer : observers())
    observer.OnProcessEventReceived(this, event);
}

void ProcessCoordinationUnitImpl::OnPropertyChanged(
    const mojom::PropertyType property_type,
    int64_t value) {
  for (auto& observer : observers())
    observer.OnProcessPropertyChanged(this, property_type, value);
}

void ProcessCoordinationUnitImpl::AddFrameImpl(
    FrameCoordinationUnitImpl* frame_cu) {
  const bool inserted = frame_coordination_units_.insert(frame_cu).second;
  if (inserted) {
    frame_cu->AddProcessCoordinationUnit(this);
    if (frame_cu->lifecycle_state() == mojom::LifecycleState::kFrozen)
      IncrementNumFrozenFrames();
  }
}

void ProcessCoordinationUnitImpl::RemoveFrame(
    FrameCoordinationUnitImpl* frame_cu) {
  DCHECK(base::ContainsKey(frame_coordination_units_, frame_cu));
  frame_coordination_units_.erase(frame_cu);

  if (frame_cu->lifecycle_state() == mojom::LifecycleState::kFrozen)
    DecrementNumFrozenFrames();
}

void ProcessCoordinationUnitImpl::DecrementNumFrozenFrames() {
  --num_frozen_frames_;
  DCHECK_GE(num_frozen_frames_, 0);
}

void ProcessCoordinationUnitImpl::IncrementNumFrozenFrames() {
  ++num_frozen_frames_;
  DCHECK_LE(num_frozen_frames_,
            static_cast<int>(frame_coordination_units_.size()));

  if (num_frozen_frames_ ==
      static_cast<int>(frame_coordination_units_.size())) {
    for (auto& observer : observers())
      observer.OnAllFramesInProcessFrozen(this);
  }
}

}  // namespace resource_coordinator
