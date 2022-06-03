// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/background_tracing/background_tracing_agent_provider_impl.h"

#include <memory>

#include "base/metrics/statistics_recorder.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/tracing/public/cpp/background_tracing/background_tracing_agent_impl.h"

namespace tracing {

BackgroundTracingAgentProviderImpl::BackgroundTracingAgentProviderImpl() =
    default;

BackgroundTracingAgentProviderImpl::~BackgroundTracingAgentProviderImpl() =
    default;

void BackgroundTracingAgentProviderImpl::AddBinding(
    mojo::PendingReceiver<mojom::BackgroundTracingAgentProvider> provider) {
  self_receiver_set_.Add(this, std::move(provider));
}

void BackgroundTracingAgentProviderImpl::Create(
    uint64_t tracing_process_id,
    mojo::PendingRemote<mojom::BackgroundTracingAgentClient> client,
    mojo::PendingReceiver<mojom::BackgroundTracingAgent> agent) {
  base::trace_event::MemoryDumpManager::GetInstance()->set_tracing_process_id(
      tracing_process_id);

  agent_receiver_set_.Add(
      std::make_unique<BackgroundTracingAgentImpl>(std::move(client)),
      std::move(agent));
}

}  // namespace tracing
