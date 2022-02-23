// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_instance_group.h"

#include "content/browser/renderer_host/render_process_host_impl.h"

namespace content {

namespace {
SiteInstanceGroupId::Generator site_instance_group_id_generator;
}  // namespace

SiteInstanceGroup::SiteInstanceGroup(RenderProcessHost* process)
    : id_(site_instance_group_id_generator.GenerateNextId()) {
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

SiteInstanceGroupId SiteInstanceGroup::GetId() {
  return id_;
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

  // Protect `this` from being deleted inside of the observers.
  scoped_refptr<SiteInstanceGroup> protect(this);

  for (auto& observer : observers_)
    observer.RenderProcessHostDestroyed();
}

void SiteInstanceGroup::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  for (auto& observer : observers_)
    observer.RenderProcessGone(this, info);
}

}  // namespace content
