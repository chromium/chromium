// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_group_scheduler_impl.h"

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

static AgentGroupSchedulerImpl* g_current_agent_group_scheduler_impl;

// static
AgentGroupSchedulerImpl* AgentGroupSchedulerImpl::GetCurrent() {
  DCHECK(WTF::IsMainThread());
  return g_current_agent_group_scheduler_impl;
}

// static
void AgentGroupSchedulerImpl::SetCurrent(
    AgentGroupSchedulerImpl* agent_group_scheduler_impl) {
  DCHECK(WTF::IsMainThread());
  g_current_agent_group_scheduler_impl = agent_group_scheduler_impl;
}

AgentGroupSchedulerImpl::AgentGroupSchedulerImpl(
    MainThreadSchedulerImpl* main_thread_scheduler)
    : main_thread_scheduler_(main_thread_scheduler) {}

AgentGroupSchedulerImpl::~AgentGroupSchedulerImpl() {
  if (main_thread_scheduler_) {
    main_thread_scheduler_->RemoveAgentGroupScheduler(this);
  }
}

}  // namespace scheduler
}  // namespace blink
