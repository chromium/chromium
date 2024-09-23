// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_AGENT_CLIENT_IMPL_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_AGENT_CLIENT_IMPL_H_

#include <optional>
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/mojom/background_tracing_agent.mojom.h"

namespace content {

class BackgroundTracingAgentClientImpl
    : public tracing::mojom::BackgroundTracingAgentClient {
 public:
  static void Create(
      int child_process_id,
      mojo::Remote<tracing::mojom::BackgroundTracingAgentProvider> provider);

  BackgroundTracingAgentClientImpl(const BackgroundTracingAgentClientImpl&) =
      delete;
  BackgroundTracingAgentClientImpl& operator=(
      const BackgroundTracingAgentClientImpl&) = delete;

  ~BackgroundTracingAgentClientImpl() override;

  // tracing::mojom::BackgroundTracingAgentClient methods:
  void OnInitialized() override;
  void OnTriggerBackgroundTrace(
      tracing::mojom::BackgroundTracingRulePtr rule,
      std::optional<int32_t> histogram_value) override;

 private:
  explicit BackgroundTracingAgentClientImpl(
      mojo::Remote<tracing::mojom::BackgroundTracingAgent> agent);

  mojo::Remote<tracing::mojom::BackgroundTracingAgent> agent_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_AGENT_CLIENT_IMPL_H_
