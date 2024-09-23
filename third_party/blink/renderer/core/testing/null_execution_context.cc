// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/null_execution_context.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/security_context_init.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

NullExecutionContext::NullExecutionContext()
    : NullExecutionContext(v8::Isolate::GetCurrent()) {}

NullExecutionContext::NullExecutionContext(v8::Isolate* isolate)
    : NullExecutionContext(scheduler::CreateDummyFrameScheduler(isolate)) {}

NullExecutionContext::NullExecutionContext(
    std::unique_ptr<FrameScheduler> scheduler)
    : ExecutionContext(scheduler->GetAgentGroupScheduler()->Isolate(),
                       MakeGarbageCollected<Agent>(
                           scheduler->GetAgentGroupScheduler()->Isolate(),
                           base::UnguessableToken::Create(),
                           v8::MicrotaskQueue::New(
                               scheduler->GetAgentGroupScheduler()->Isolate(),
                               v8::MicrotasksPolicy::kScoped))),
      scheduler_(std::move(scheduler)) {
  SetPolicyContainer(PolicyContainer::CreateEmpty());
}

NullExecutionContext::~NullExecutionContext() {}

void NullExecutionContext::SetUpSecurityContextForTesting() {
  SetPolicyContainer(PolicyContainer::CreateEmpty());
  auto* policy = MakeGarbageCollected<ContentSecurityPolicy>();
  GetSecurityContext().SetSecurityOriginForTesting(
      SecurityOrigin::Create(url_));
  policy->BindToDelegate(GetContentSecurityPolicyDelegate());
  SetContentSecurityPolicy(policy);
}

FrameOrWorkerScheduler* NullExecutionContext::GetScheduler() {
  return scheduler_.get();
}

scoped_refptr<base::SingleThreadTaskRunner> NullExecutionContext::GetTaskRunner(
    TaskType task_type) {
  return scheduler_->GetTaskRunner(task_type);
}

const BrowserInterfaceBrokerProxy&
NullExecutionContext::GetBrowserInterfaceBroker() const {
  return GetEmptyBrowserInterfaceBroker();
}

}  // namespace blink
