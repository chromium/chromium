// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_agent_client_impl.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "content/browser/child_process_host_impl.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

// static
void BackgroundTracingAgentClientImpl::Create(
    int child_process_id,
    mojo::Remote<tracing::mojom::BackgroundTracingAgentProvider> provider) {
  mojo::PendingRemote<tracing::mojom::BackgroundTracingAgentClient> client;
  auto client_receiver = client.InitWithNewPipeAndPassReceiver();

  mojo::Remote<tracing::mojom::BackgroundTracingAgent> agent;

  provider->Create(ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(
                       child_process_id),
                   std::move(client), agent.BindNewPipeAndPassReceiver());

  // Lifetime bound to the agent, which means it is bound to the lifetime of
  // the child process. Will be cleaned up when the process exits.
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new BackgroundTracingAgentClientImpl(std::move(agent))),
      std::move(client_receiver));
}

BackgroundTracingAgentClientImpl::~BackgroundTracingAgentClientImpl() {
  BackgroundTracingManagerImpl::GetInstance().RemoveAgent(agent_.get());
}

void BackgroundTracingAgentClientImpl::OnInitialized() {
  BackgroundTracingManagerImpl::GetInstance().AddAgent(agent_.get());
}

void BackgroundTracingAgentClientImpl::OnTriggerBackgroundTrace(
    tracing::mojom::BackgroundTracingRulePtr rule,
    std::optional<int32_t> histogram_value) {
  base::trace_event::EmitNamedTrigger(rule->rule_id, histogram_value);
}

BackgroundTracingAgentClientImpl::BackgroundTracingAgentClientImpl(
    mojo::Remote<tracing::mojom::BackgroundTracingAgent> agent)
    : agent_(std::move(agent)) {
  DCHECK(agent_);
}

}  // namespace content
