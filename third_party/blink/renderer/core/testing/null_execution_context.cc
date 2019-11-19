// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/null_execution_context.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/dom_timer.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

NullExecutionContext::NullExecutionContext(
    OriginTrialContext* origin_trial_context)
    : ExecutionContext(
          v8::Isolate::GetCurrent(),
          MakeGarbageCollected<Agent>(v8::Isolate::GetCurrent(),
                                      base::UnguessableToken::Null()),
          origin_trial_context),
      tasks_need_pause_(false),
      is_secure_context_(true),
      scheduler_(scheduler::CreateDummyFrameScheduler()) {}

NullExecutionContext::~NullExecutionContext() {}

void NullExecutionContext::SetIsSecureContext(bool is_secure_context) {
  is_secure_context_ = is_secure_context;
}

bool NullExecutionContext::IsSecureContext(String& error_message) const {
  if (!is_secure_context_)
    error_message = "A secure context is required";
  return is_secure_context_;
}

void NullExecutionContext::SetUpSecurityContext() {
  auto* policy = MakeGarbageCollected<ContentSecurityPolicy>();
  SecurityContext::SetSecurityOrigin(SecurityOrigin::Create(url_));
  policy->BindToDelegate(GetContentSecurityPolicyDelegate());
  SecurityContext::SetContentSecurityPolicy(policy);
}

FrameOrWorkerScheduler* NullExecutionContext::GetScheduler() {
  return scheduler_.get();
}

scoped_refptr<base::SingleThreadTaskRunner> NullExecutionContext::GetTaskRunner(
    TaskType) {
  return Thread::Current()->GetTaskRunner();
}

BrowserInterfaceBrokerProxy& NullExecutionContext::GetBrowserInterfaceBroker() {
  return GetEmptyBrowserInterfaceBroker();
}

}  // namespace blink
