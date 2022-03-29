// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_instance_group.h"

#include "base/observer_list.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {
SiteInstanceGroupId::Generator site_instance_group_id_generator;
}  // namespace

SiteInstanceGroup::SiteInstanceGroup(BrowsingInstanceId browsing_instance_id,
                                     RenderProcessHost* process)
    : id_(site_instance_group_id_generator.GenerateNextId()),
      browsing_instance_id_(browsing_instance_id) {
  SetProcessAndAgentSchedulingGroup(process);
}

SiteInstanceGroup::~SiteInstanceGroup() {
  if (!process_)
    return;

  process_->RemoveObserver(this);

  // Ensure the RenderProcessHost gets deleted if this SiteInstanceGroup
  // created a process which was never used by any listeners.
  process_->Cleanup();
}

SiteInstanceGroupId SiteInstanceGroup::GetId() const {
  return id_;
}

base::SafeRef<SiteInstanceGroup> SiteInstanceGroup::GetSafeRef() {
  return weak_ptr_factory_.GetSafeRef();
}

void SiteInstanceGroup::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SiteInstanceGroup::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SiteInstanceGroup::IncrementActiveFrameCount() {
  active_frame_count_++;
}

void SiteInstanceGroup::DecrementActiveFrameCount() {
  if (--active_frame_count_ == 0) {
    for (auto& observer : observers_)
      observer.ActiveFrameCountIsZero(this);
  }
}

void SiteInstanceGroup::SetProcessAndAgentSchedulingGroup(
    RenderProcessHost* process) {
  // It is never safe to change |process_| without going through
  // RenderProcessHostDestroyed first to set it to null. Otherwise, same-site
  // frames will end up in different processes and everything will get confused.
  CHECK(!process_);
  process->AddObserver(this);
  process_ = process;
  agent_scheduling_group_ =
      AgentSchedulingGroupHost::GetOrCreate(*this, *process);
}

void SiteInstanceGroup::RenderProcessHostDestroyed(RenderProcessHost* host) {
  DCHECK_EQ(process_, host);
  process_->RemoveObserver(this);
  process_ = nullptr;
  agent_scheduling_group_ = nullptr;
}

void SiteInstanceGroup::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  for (auto& observer : observers_)
    observer.RenderProcessGone(this, info);
}

void SiteInstanceGroup::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto->set_site_instance_group_id(GetId().value());
  proto->set_active_frame_count(active_frame_count());
  proto.Set(TraceProto::kProcess, process());
}

}  // namespace content
