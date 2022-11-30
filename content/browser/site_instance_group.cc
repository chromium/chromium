// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_instance_group.h"

#include "base/observer_list.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {
SiteInstanceGroupId::Generator site_instance_group_id_generator;
}  // namespace

SiteInstanceGroup::SiteInstanceGroup(BrowsingInstanceId browsing_instance_id,
                                     RenderProcessHost* process)
    : id_(site_instance_group_id_generator.GenerateNextId()),
      browsing_instance_id_(browsing_instance_id),
      process_(process->GetSafeRef()),
      agent_scheduling_group_(
          AgentSchedulingGroupHost::GetOrCreate(*this, *process)
              ->GetSafeRef()) {
  process->AddObserver(this);
}

SiteInstanceGroup::~SiteInstanceGroup() {
  process_->RemoveObserver(this);
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

void SiteInstanceGroup::AddSiteInstance(SiteInstanceImpl* site_instance) {
  site_instances_.insert(site_instance);
}

void SiteInstanceGroup::RemoveSiteInstance(SiteInstanceImpl* site_instance) {
  site_instances_.erase(site_instance);
  if (site_instances_.empty())
    process_->Cleanup();
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

void SiteInstanceGroup::RenderProcessHostDestroyed(RenderProcessHost* host) {
  DCHECK_EQ(process_->GetID(), host->GetID());
  process_->RemoveObserver(this);

  // Remove references to `this` from all SiteInstances in this group. That will
  // cause `this` to be destructed, to enforce the invariant that a
  // SiteInstanceGroup must have a RenderProcessHost.
  for (auto* instance : site_instances_)
    instance->ResetSiteInstanceGroup();
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
