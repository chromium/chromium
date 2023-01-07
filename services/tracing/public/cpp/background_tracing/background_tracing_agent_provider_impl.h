// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_BACKGROUND_TRACING_AGENT_PROVIDER_IMPL_H_
#define SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_BACKGROUND_TRACING_AGENT_PROVIDER_IMPL_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/tracing/public/mojom/background_tracing_agent.mojom.h"

namespace tracing {

class COMPONENT_EXPORT(BACKGROUND_TRACING_CPP)
    BackgroundTracingAgentProviderImpl
    : public mojom::BackgroundTracingAgentProvider {
 public:
  BackgroundTracingAgentProviderImpl();

  BackgroundTracingAgentProviderImpl(
      const BackgroundTracingAgentProviderImpl&) = delete;
  BackgroundTracingAgentProviderImpl& operator=(
      const BackgroundTracingAgentProviderImpl&) = delete;

  ~BackgroundTracingAgentProviderImpl() override;

  void AddBinding(
      mojo::PendingReceiver<mojom::BackgroundTracingAgentProvider> provider);

  // mojom::BackgroundTracingAgentProvider methods:
  void Create(
      uint64_t tracing_process_id,
      mojo::PendingRemote<mojom::BackgroundTracingAgentClient> client,
      mojo::PendingReceiver<mojom::BackgroundTracingAgent> agent) override;

 private:
  mojo::ReceiverSet<mojom::BackgroundTracingAgentProvider> self_receiver_set_;
  mojo::UniqueReceiverSet<mojom::BackgroundTracingAgent> agent_receiver_set_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_BACKGROUND_TRACING_AGENT_PROVIDER_IMPL_H_
