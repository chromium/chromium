// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_AGENT_GROUP_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_AGENT_GROUP_SCHEDULER_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
namespace scheduler {
class MainThreadSchedulerImpl;
class MainThreadTaskQueue;
class WebThreadScheduler;

// AgentGroupScheduler implementation which schedules per-AgentSchedulingGroup
// tasks.
class PLATFORM_EXPORT AgentGroupSchedulerImpl : public AgentGroupScheduler {
 public:
  explicit AgentGroupSchedulerImpl(
      MainThreadSchedulerImpl& main_thread_scheduler);
  AgentGroupSchedulerImpl(const AgentGroupSchedulerImpl&) = delete;
  AgentGroupSchedulerImpl& operator=(const AgentGroupSchedulerImpl&) = delete;
  ~AgentGroupSchedulerImpl() override;

  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override;
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  WebThreadScheduler& GetMainThreadScheduler() override;
  AgentGroupScheduler& AsAgentGroupScheduler() override;

  void BindInterfaceBroker(
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> remote_broker)
      override;
  BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker() override;

 private:
  scoped_refptr<MainThreadTaskQueue> default_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  MainThreadSchedulerImpl& main_thread_scheduler_;  // Not owned.

  BrowserInterfaceBrokerProxy broker_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_AGENT_GROUP_SCHEDULER_IMPL_H_
