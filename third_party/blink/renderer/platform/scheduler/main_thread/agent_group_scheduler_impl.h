// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_AGENT_GROUP_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_AGENT_GROUP_SCHEDULER_IMPL_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"

#include <memory>

namespace blink {
namespace scheduler {
class MainThreadSchedulerImpl;

// AgentGroupScheduler implementation which schedules per-AgentSchedulingGroup
// tasks.
class PLATFORM_EXPORT AgentGroupSchedulerImpl
    : public blink::AgentGroupScheduler {
 public:
  static AgentGroupSchedulerImpl* GetCurrent();
  static void SetCurrent(AgentGroupSchedulerImpl*);
  explicit AgentGroupSchedulerImpl(
      MainThreadSchedulerImpl* main_thread_scheduler);
  AgentGroupSchedulerImpl(const AgentGroupSchedulerImpl&) = delete;
  AgentGroupSchedulerImpl& operator=(const AgentGroupSchedulerImpl&) = delete;
  ~AgentGroupSchedulerImpl() override;
  MainThreadSchedulerImpl* GetMainThreadScheduler() {
    return main_thread_scheduler_;
  }

 private:
  MainThreadSchedulerImpl* main_thread_scheduler_;  // Not owned.
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_AGENT_GROUP_SCHEDULER_IMPL_H_
