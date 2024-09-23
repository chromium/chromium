// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"

namespace blink::scheduler {

WebAgentGroupScheduler::WebAgentGroupScheduler(
    AgentGroupScheduler* agent_group_scheduler)
    : private_(agent_group_scheduler) {}

WebAgentGroupScheduler::~WebAgentGroupScheduler() {
  private_.Reset();
}

AgentGroupScheduler& WebAgentGroupScheduler::GetAgentGroupScheduler() {
  return *private_;
}

scoped_refptr<base::SingleThreadTaskRunner>
WebAgentGroupScheduler::DefaultTaskRunner() {
  return private_->DefaultTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
WebAgentGroupScheduler::CompositorTaskRunner() {
  return private_->CompositorTaskRunner();
}

v8::Isolate* WebAgentGroupScheduler::Isolate() {
  return private_->Isolate();
}

void WebAgentGroupScheduler::OnUrgentMessageReceived() {
  private_->OnUrgentMessageReceived();
}

void WebAgentGroupScheduler::OnUrgentMessageProcessed() {
  private_->OnUrgentMessageProcessed();
}

}  // namespace blink::scheduler
