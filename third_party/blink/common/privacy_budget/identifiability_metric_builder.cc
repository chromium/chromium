// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>

#include "base/trace_event/trace_event.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"

namespace blink {

IdentifiabilityMetricBuilder::IdentifiabilityMetricBuilder(
    ukm::SourceIdObj source_id)
    : source_id_(source_id) {}

IdentifiabilityMetricBuilder::~IdentifiabilityMetricBuilder() = default;

IdentifiabilityMetricBuilder& IdentifiabilityMetricBuilder::Add(
    IdentifiableSurface surface,
    IdentifiableToken value) {
  // Note: tracing will only work if identifiability study is enabled first
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("identifiability"),
               "IdentifiableSurface", "key",
               base::NumberToString(surface.ToUkmMetricHash()));

  metrics_.emplace_back(surface, value);
  return *this;
}

void IdentifiabilityMetricBuilder::Record(ukm::UkmRecorder* recorder) {
  auto* collector = IdentifiabilitySampleCollector::Get();
  if (collector && !metrics_.empty())
    collector->Record(recorder, source_id_.ToInt64(), std::move(metrics_));
}

}  // namespace blink
