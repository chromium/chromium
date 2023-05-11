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

SiteInstanceGroup::SiteInstanceGroup(BrowsingInstance* browsing_instance,
                                     RenderProcessHost* process)
    : id_(site_instance_group_id_generator.GenerateNextId()),
      browsing_instance_(browsing_instance),
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

base::WeakPtr<SiteInstanceGroup>
SiteInstanceGroup::GetWeakPtrToAllowDangling() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SiteInstanceGroup::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SiteInstanceGroup::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SiteInstanceGroup::AddSiteInstance(SiteInstanceImpl* site_instance) {
  CHECK_EQ(browsing_instance_id(), site_instance->GetBrowsingInstanceId());
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

bool SiteInstanceGroup::IsRelatedSiteInstanceGroup(SiteInstanceGroup* group) {
  return browsing_instance_id() == group->browsing_instance_id();
}

bool SiteInstanceGroup::IsCoopRelatedSiteInstanceGroup(
    SiteInstanceGroup* group) {
  return coop_related_group_token() == group->coop_related_group_token();
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

// static
SiteInstanceGroup* SiteInstanceGroup::CreateForTesting(
    BrowserContext* browser_context,
    RenderProcessHost* process) {
  return new SiteInstanceGroup(
      new BrowsingInstance(browser_context,
                           WebExposedIsolationInfo::CreateNonIsolated(),
                           /*is_guest=*/false,
                           /*is_fenced=*/false, /*coop_related_group=*/nullptr,
                           /*common_coop_origin=*/absl::nullopt),
      process);
}

// static
SiteInstanceGroup* SiteInstanceGroup::CreateForTesting(
    SiteInstanceGroup* group,
    RenderProcessHost* process) {
  return new SiteInstanceGroup(
      group->browsing_instance_for_testing(),  // IN-TEST
      process);
}

void SiteInstanceGroup::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto->set_site_instance_group_id(GetId().value());
  proto->set_active_frame_count(active_frame_count());
  proto.Set(TraceProto::kProcess, process());
}

}  // namespace content
