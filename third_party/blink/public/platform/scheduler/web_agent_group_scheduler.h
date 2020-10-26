// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_AGENT_GROUP_SCHEDULER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_AGENT_GROUP_SCHEDULER_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {
class AgentGroupScheduler;
namespace scheduler {

// WebAgentGroupScheduler schedules per-AgentSchedulingGroup tasks.
// AgentSchedulingGroup is Blink's unit of scheduling and performance isolation.
// And WebAgentGroupScheduler is a dedicated scheduler for each
// AgentSchedulingGroup. Any task posted on WebAgentGroupScheduler shouldn’t be
// run on a different WebAgentGroupScheduler.
class BLINK_PLATFORM_EXPORT WebAgentGroupScheduler {
 public:
  // Create a dummy AgentGroupScheduler only for testing
  static std::unique_ptr<blink::scheduler::WebAgentGroupScheduler>
  CreateForTesting();

  virtual ~WebAgentGroupScheduler() = default;

  virtual AgentGroupScheduler& AsAgentGroupScheduler() = 0;

  // Default task runner for an AgentSchedulingGroup.
  // Default task runners for different AgentSchedulingGroup would be
  // independent and won't have any ordering guarantees between them.
  virtual scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() = 0;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_AGENT_GROUP_SCHEDULER_H_
