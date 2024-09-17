// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_BACKGROUND_TRACING_AGENT_IMPL_H_
#define SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_BACKGROUND_TRACING_AGENT_IMPL_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time/time.h"
#include "base/trace_event/named_trigger.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/mojom/background_tracing_agent.mojom.h"

namespace tracing {

// This class sends and receives trace messages on child processes.
class COMPONENT_EXPORT(BACKGROUND_TRACING_CPP) BackgroundTracingAgentImpl
    : public mojom::BackgroundTracingAgent,
      public base::trace_event::NamedTriggerManager {
 public:
  explicit BackgroundTracingAgentImpl(
      mojo::PendingRemote<mojom::BackgroundTracingAgentClient> client);

  BackgroundTracingAgentImpl(const BackgroundTracingAgentImpl&) = delete;
  BackgroundTracingAgentImpl& operator=(const BackgroundTracingAgentImpl&) =
      delete;

  ~BackgroundTracingAgentImpl() override;

  // mojom::BackgroundTracingAgent methods:
  void SetUMACallback(tracing::mojom::BackgroundTracingRulePtr rule,
                      const std::string& histogram_name,
                      int32_t histogram_lower_value,
                      int32_t histogram_upper_value) override;
  void ClearUMACallback(tracing::mojom::BackgroundTracingRulePtr rule) override;

  // base::trace_event::NamedTriggerManager
  bool DoEmitNamedTrigger(const std::string& trigger_name,
                          std::optional<int32_t> value) override;

 private:
  void OnHistogramChanged(const std::string& rule_id,
                          base::Histogram::Sample reference_lower_value,
                          base::Histogram::Sample reference_upper_value,
                          const char* histogram_name,
                          uint64_t name_hash,
                          base::Histogram::Sample actual_value);

  mojo::Remote<mojom::BackgroundTracingAgentClient> client_;
  base::Time histogram_last_changed_;
  // Tracks histogram names and corresponding registered callbacks.
  std::map<
      std::string,
      std::unique_ptr<base::StatisticsRecorder::ScopedHistogramSampleObserver>>
      histogram_callback_map_;

  base::WeakPtrFactory<BackgroundTracingAgentImpl> weak_factory_{this};
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_BACKGROUND_TRACING_AGENT_IMPL_H_
