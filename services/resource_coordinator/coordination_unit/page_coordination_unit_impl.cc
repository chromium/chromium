// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"

#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/observers/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/resource_coordinator_clock.h"

namespace resource_coordinator {

PageCoordinationUnitImpl::PageCoordinationUnitImpl(
    const CoordinationUnitID& id,
    CoordinationUnitGraph* graph,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : CoordinationUnitInterface(id, graph, std::move(service_ref)) {}

PageCoordinationUnitImpl::~PageCoordinationUnitImpl() {
  for (auto* child_frame : frame_coordination_units_)
    child_frame->RemovePageCoordinationUnit(this);
}

void PageCoordinationUnitImpl::AddFrame(const CoordinationUnitID& cu_id) {
  DCHECK(cu_id.type == CoordinationUnitType::kFrame);
  FrameCoordinationUnitImpl* frame_cu =
      FrameCoordinationUnitImpl::GetCoordinationUnitByID(graph_, cu_id);
  if (!frame_cu)
    return;
  if (AddFrame(frame_cu))
    frame_cu->AddPageCoordinationUnit(this);
}

void PageCoordinationUnitImpl::RemoveFrame(const CoordinationUnitID& cu_id) {
  DCHECK(cu_id != id());
  FrameCoordinationUnitImpl* frame_cu =
      FrameCoordinationUnitImpl::GetCoordinationUnitByID(graph_, cu_id);
  if (!frame_cu)
    return;
  if (RemoveFrame(frame_cu))
    frame_cu->RemovePageCoordinationUnit(this);
}

void PageCoordinationUnitImpl::SetIsLoading(bool is_loading) {
  SetProperty(mojom::PropertyType::kIsLoading, is_loading);
}

void PageCoordinationUnitImpl::SetVisibility(bool visible) {
  SetProperty(mojom::PropertyType::kVisible, visible);
}

void PageCoordinationUnitImpl::SetUKMSourceId(int64_t ukm_source_id) {
  SetProperty(mojom::PropertyType::kUKMSourceId, ukm_source_id);
}

void PageCoordinationUnitImpl::OnFaviconUpdated() {
  SendEvent(mojom::Event::kFaviconUpdated);
}

void PageCoordinationUnitImpl::OnTitleUpdated() {
  SendEvent(mojom::Event::kTitleUpdated);
}

void PageCoordinationUnitImpl::OnMainFrameNavigationCommitted(
    base::TimeTicks navigation_committed_time,
    int64_t navigation_id,
    const std::string& url) {
  navigation_committed_time_ = navigation_committed_time;
  main_frame_url_ = url;
  navigation_id_ = navigation_id;
  SendEvent(mojom::Event::kNavigationCommitted);
}

std::set<ProcessCoordinationUnitImpl*>
PageCoordinationUnitImpl::GetAssociatedProcessCoordinationUnits() const {
  std::set<ProcessCoordinationUnitImpl*> process_cus;

  for (auto* frame_cu : frame_coordination_units_) {
    if (auto* process_cu = frame_cu->GetProcessCoordinationUnit()) {
      process_cus.insert(process_cu);
    }
  }
  return process_cus;
}

bool PageCoordinationUnitImpl::IsVisible() const {
  int64_t is_visible = 0;
  bool has_property = GetProperty(mojom::PropertyType::kVisible, &is_visible);
  DCHECK(has_property && (is_visible == 0 || is_visible == 1));
  return is_visible;
}

double PageCoordinationUnitImpl::GetCPUUsage() const {
  double cpu_usage = 0.0;

  for (auto* process_cu : GetAssociatedProcessCoordinationUnits()) {
    size_t pages_in_process =
        process_cu->GetAssociatedPageCoordinationUnits().size();
    DCHECK_LE(1u, pages_in_process);

    int64_t process_cpu_usage = 0;
    if (process_cu->GetProperty(mojom::PropertyType::kCPUUsage,
                                &process_cpu_usage)) {
      cpu_usage += static_cast<double>(process_cpu_usage) / pages_in_process;
    }
  }

  return cpu_usage / 1000;
}

bool PageCoordinationUnitImpl::GetExpectedTaskQueueingDuration(
    int64_t* output) {
  // Calculate the EQT for the process of the main frame only because
  // the smoothness of the main frame may affect the users the most.
  FrameCoordinationUnitImpl* main_frame_cu = GetMainFrameCoordinationUnit();
  if (!main_frame_cu)
    return false;
  auto* process_cu = main_frame_cu->GetProcessCoordinationUnit();
  if (!process_cu)
    return false;
  return process_cu->GetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, output);
}

base::TimeDelta PageCoordinationUnitImpl::TimeSinceLastNavigation() const {
  if (navigation_committed_time_.is_null())
    return base::TimeDelta();
  return ResourceCoordinatorClock::NowTicks() - navigation_committed_time_;
}

base::TimeDelta PageCoordinationUnitImpl::TimeSinceLastVisibilityChange()
    const {
  return ResourceCoordinatorClock::NowTicks() - visibility_change_time_;
}

FrameCoordinationUnitImpl*
PageCoordinationUnitImpl::GetMainFrameCoordinationUnit() const {
  for (auto* frame_cu : frame_coordination_units_) {
    if (frame_cu->IsMainFrame())
      return frame_cu;
  }
  return nullptr;
}

void PageCoordinationUnitImpl::OnFrameLifecycleStateChanged(
    FrameCoordinationUnitImpl* frame_cu,
    mojom::LifecycleState old_state) {
  DCHECK(base::ContainsKey(frame_coordination_units_, frame_cu));
  DCHECK_NE(old_state, frame_cu->lifecycle_state());

  int delta = 0;
  if (old_state == mojom::LifecycleState::kFrozen)
    delta = -1;
  else if (frame_cu->lifecycle_state() == mojom::LifecycleState::kFrozen)
    delta = 1;
  if (delta != 0)
    OnNumFrozenFramesStateChange(delta);
}

void PageCoordinationUnitImpl::OnEventReceived(mojom::Event event) {
  for (auto& observer : observers())
    observer.OnPageEventReceived(this, event);
}

void PageCoordinationUnitImpl::OnPropertyChanged(
    const mojom::PropertyType property_type,
    int64_t value) {
  if (property_type == mojom::PropertyType::kVisible)
    visibility_change_time_ = ResourceCoordinatorClock::NowTicks();
  for (auto& observer : observers())
    observer.OnPagePropertyChanged(this, property_type, value);
}

bool PageCoordinationUnitImpl::AddFrame(FrameCoordinationUnitImpl* frame_cu) {
  const bool inserted = frame_coordination_units_.insert(frame_cu).second;
  if (inserted) {
    OnNumFrozenFramesStateChange(
        frame_cu->lifecycle_state() == mojom::LifecycleState::kFrozen ? 1 : 0);
  }
  return inserted;
}

bool PageCoordinationUnitImpl::RemoveFrame(
    FrameCoordinationUnitImpl* frame_cu) {
  bool removed = frame_coordination_units_.erase(frame_cu) > 0;
  if (removed) {
    OnNumFrozenFramesStateChange(
        frame_cu->lifecycle_state() == mojom::LifecycleState::kFrozen ? -1 : 0);
  }
  return removed;
}

void PageCoordinationUnitImpl::OnNumFrozenFramesStateChange(
    int num_frozen_frames_delta) {
  num_frozen_frames_ += num_frozen_frames_delta;
  DCHECK_GE(num_frozen_frames_, 0u);
  DCHECK_LE(num_frozen_frames_, frame_coordination_units_.size());

  const int64_t kRunning =
      static_cast<int64_t>(mojom::LifecycleState::kRunning);
  const int64_t kFrozen = static_cast<int64_t>(mojom::LifecycleState::kFrozen);

  // We are interested in knowing when we have transitioned to or from
  // "fully frozen". A page with no frames is considered to be running by
  // default.
  bool was_fully_frozen =
      GetPropertyOrDefault(mojom::PropertyType::kLifecycleState, kRunning) ==
      kFrozen;
  bool is_fully_frozen = frame_coordination_units_.size() > 0 &&
                         num_frozen_frames_ == frame_coordination_units_.size();
  if (was_fully_frozen == is_fully_frozen)
    return;

  if (is_fully_frozen) {
    // Aggregate the beforeunload handler information from the entire frame
    // tree.
    bool has_nonempty_beforeunload = false;
    for (auto* frame : frame_coordination_units_) {
      if (frame->has_nonempty_beforeunload()) {
        has_nonempty_beforeunload = true;
        break;
      }
    }
    set_has_nonempty_beforeunload(has_nonempty_beforeunload);
  }

  // TODO(fdoray): Store the lifecycle state as a member on the
  // PageCoordinationUnit rather than as a non-typed property.
  SetProperty(mojom::PropertyType::kLifecycleState,
              is_fully_frozen ? kFrozen : kRunning);
}

}  // namespace resource_coordinator
