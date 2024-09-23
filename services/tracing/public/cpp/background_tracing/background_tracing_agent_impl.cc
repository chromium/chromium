// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/background_tracing/background_tracing_agent_impl.h"

#include <memory>

#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_histogram_sample.pbzero.h"

namespace tracing {

BackgroundTracingAgentImpl::BackgroundTracingAgentImpl(
    mojo::PendingRemote<mojom::BackgroundTracingAgentClient> client)
    : client_(std::move(client)) {
  client_->OnInitialized();
  NamedTriggerManager::SetInstance(this);
}

BackgroundTracingAgentImpl::~BackgroundTracingAgentImpl() {
  NamedTriggerManager::SetInstance(nullptr);
}

void BackgroundTracingAgentImpl::SetUMACallback(
    tracing::mojom::BackgroundTracingRulePtr rule,
    const std::string& histogram_name,
    int32_t histogram_lower_value,
    int32_t histogram_upper_value) {
  // This callback will run on a random thread, so we need to proxy back to the
  // current sequence before touching |this|.
  auto histogram_observer =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindRepeating(&BackgroundTracingAgentImpl::OnHistogramChanged,
                              weak_factory_.GetWeakPtr(), rule->rule_id,
                              histogram_lower_value, histogram_upper_value));
  histogram_callback_map_.insert(
      {rule->rule_id, std::move(histogram_observer)});
}

void BackgroundTracingAgentImpl::ClearUMACallback(
    tracing::mojom::BackgroundTracingRulePtr rule) {
  histogram_callback_map_.erase(rule->rule_id);
}

bool BackgroundTracingAgentImpl::DoEmitNamedTrigger(
    const std::string& trigger_name,
    std::optional<int32_t> value) {
  client_->OnTriggerBackgroundTrace(
      tracing::mojom::BackgroundTracingRule::New(trigger_name), value);
  return true;
}

void BackgroundTracingAgentImpl::OnHistogramChanged(
    const std::string& rule_id,
    base::Histogram::Sample histogram_lower_value,
    base::Histogram::Sample histogram_upper_value,
    const char* histogram_name,
    uint64_t name_hash,
    base::Histogram::Sample actual_value) {
  if (actual_value < histogram_lower_value ||
      actual_value > histogram_upper_value) {
    return;
  }
  TRACE_EVENT("toplevel", "HistogramSampleTrigger",
              [&](perfetto::EventContext ctx) {
                perfetto::protos::pbzero::ChromeHistogramSample* new_sample =
                    ctx.event()->set_chrome_histogram_sample();
                new_sample->set_name_hash(name_hash);
                new_sample->set_sample(actual_value);
              });

  client_->OnTriggerBackgroundTrace(
      tracing::mojom::BackgroundTracingRule::New(rule_id), actual_value);
}

}  // namespace tracing
