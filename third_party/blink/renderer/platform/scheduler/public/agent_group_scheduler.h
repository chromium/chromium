// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_AGENT_GROUP_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_AGENT_GROUP_SCHEDULER_H_

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"

namespace blink {

namespace scheduler {
class WebThreadScheduler;
}  // namespace scheduler

// AgentGroupScheduler schedules per-AgentSchedulingGroup tasks.
// AgentSchedulingGroup is Blink's unit of scheduling and performance isolation.
class BLINK_PLATFORM_EXPORT AgentGroupScheduler
    : public GarbageCollected<AgentGroupScheduler> {
 public:
  class Agent : public GarbageCollectedMixin {
   public:
    virtual void PerformMicrotaskCheckpoint() = 0;
  };

  virtual ~AgentGroupScheduler() = default;

  // Creates a new PageScheduler for a given Page. Must be called from the
  // associated WebThread.
  virtual std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) = 0;

  virtual void BindInterfaceBroker(
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          remote_broker) = 0;
  virtual BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker() = 0;

  virtual void AddAgent(Agent* agent) = 0;

  virtual void Trace(Visitor*) const {}

  virtual scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  CompositorTaskRunner() = 0;
  virtual scheduler::WebThreadScheduler& GetMainThreadScheduler() = 0;
  virtual v8::Isolate* Isolate() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_AGENT_GROUP_SCHEDULER_H_
