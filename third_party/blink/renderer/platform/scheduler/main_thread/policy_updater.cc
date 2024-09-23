// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/policy_updater.h"

#include "base/check.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_group_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"

namespace blink::scheduler {

PolicyUpdater::PolicyUpdater() = default;

PolicyUpdater::~PolicyUpdater() {
  // Check: Objects for which to update policy must be in the same hierarchy.
  if (frame_ && page_) {
    CHECK_EQ(frame_->GetPageScheduler(), page_);
  }
  if (frame_ && agent_group_) {
    CHECK_EQ(frame_->GetAgentGroupScheduler(), agent_group_);
  }
  if (page_ && agent_group_) {
    CHECK_EQ(&page_->GetAgentGroupScheduler(), agent_group_);
  }

  // Update policy. Note: Since policy updates are propagated downward, it is
  // only necessary to call `UpdatePolicy()` on the highest object in the
  // hierarchy.
  if (agent_group_) {
    agent_group_->UpdatePolicy();
  } else if (page_) {
    page_->UpdatePolicy();
  } else if (frame_) {
    frame_->UpdatePolicy();
  }
}

void PolicyUpdater::UpdateFramePolicy(FrameSchedulerImpl* frame) {
  CHECK(!frame_ || frame_ == frame);
  frame_ = frame;
}

void PolicyUpdater::UpdatePagePolicy(PageSchedulerImpl* page) {
  CHECK(!page_ || page_ == page);
  page_ = page;
}

void PolicyUpdater::UpdateAgentGroupPolicy(
    AgentGroupSchedulerImpl* agent_group) {
  CHECK(!agent_group_ || agent_group_ == agent_group);
  agent_group_ = agent_group;
}

}  // namespace blink::scheduler
