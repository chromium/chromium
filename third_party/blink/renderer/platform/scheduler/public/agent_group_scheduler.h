// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_AGENT_GROUP_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_AGENT_GROUP_SCHEDULER_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"

namespace blink {

// AgentGroupScheduler schedules per-AgentSchedulingGroup tasks.
// AgentSchedulingGroup is Blink's unit of scheduling and performance isolation.
class BLINK_PLATFORM_EXPORT AgentGroupScheduler
    : public scheduler::WebAgentGroupScheduler {
 public:
  // Creates a new PageScheduler for a given Page. Must be called from the
  // associated WebThread.
  virtual std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_AGENT_GROUP_SCHEDULER_H_
