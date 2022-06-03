// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_FAKE_AGENT_GROUP_SCHEDULER_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_FAKE_AGENT_GROUP_SCHEDULER_SCHEDULER_H_

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"

namespace blink {
namespace scheduler {

class FakeAgentGroupScheduler : public AgentGroupScheduler {
 public:
  explicit FakeAgentGroupScheduler(WebThreadScheduler& web_thread_scheduler)
      : web_thread_scheduler_(web_thread_scheduler) {}
  ~FakeAgentGroupScheduler() override = default;

  AgentGroupScheduler& AsAgentGroupScheduler() override { return *this; }

  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override {
    return nullptr;
  }

  WebThreadScheduler& GetMainThreadScheduler() override {
    return web_thread_scheduler_;
  }

  BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker() override {
    return GetEmptyBrowserInterfaceBroker();
  }

  void BindInterfaceBroker(
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>) override {}

 private:
  WebThreadScheduler& web_thread_scheduler_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_FAKE_AGENT_GROUP_SCHEDULER_SCHEDULER_H_
