// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_AGENT_GROUP_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_AGENT_GROUP_SCHEDULER_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
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
  // TODO(dtapuska): Remove usage of this prefinalizer. The MainThreadTaskQueues
  // need to be removed from the MainThreadScheduler and are created from both
  // oilpanned objects and non-oilpanned objects. This finalizer should be able
  // to be removed once more scheduling classes are moved to oilpan.
  USING_PRE_FINALIZER(AgentGroupSchedulerImpl, Dispose);

 public:
  explicit AgentGroupSchedulerImpl(
      MainThreadSchedulerImpl& main_thread_scheduler);
  AgentGroupSchedulerImpl(const AgentGroupSchedulerImpl&) = delete;
  AgentGroupSchedulerImpl& operator=(const AgentGroupSchedulerImpl&) = delete;
  ~AgentGroupSchedulerImpl() override = default;

  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override;
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<MainThreadTaskQueue> CompositorTaskQueue();
  WebThreadScheduler& GetMainThreadScheduler() override;
  v8::Isolate* Isolate() override;

  void BindInterfaceBroker(
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> remote_broker)
      override;
  BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker() override;
  void AddAgent(Agent* agent) override;
  void Trace(Visitor*) const override;

  void PerformMicrotaskCheckpoint();

  void Dispose();

 private:
  scoped_refptr<MainThreadTaskQueue> default_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<MainThreadTaskQueue> compositor_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  MainThreadSchedulerImpl& main_thread_scheduler_;  // Not owned.
  HeapHashSet<WeakMember<Agent>> agents_;

  BrowserInterfaceBrokerProxy broker_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_AGENT_GROUP_SCHEDULER_IMPL_H_
