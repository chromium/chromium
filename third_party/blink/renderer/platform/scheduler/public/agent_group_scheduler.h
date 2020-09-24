// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_AGENT_GROUP_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_AGENT_GROUP_SCHEDULER_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// AgentGroupScheduler schedules per-AgentSchedulingGroup tasks.
class PLATFORM_EXPORT AgentGroupScheduler {
 public:
  virtual ~AgentGroupScheduler() = default;
  virtual scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_AGENT_GROUP_SCHEDULER_H_
