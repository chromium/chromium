// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"

#include <iterator>

#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"

namespace blink {

IdentifiabilityMetricBuilder::IdentifiabilityMetricBuilder(
    ukm::SourceIdObj source_id)
    : source_id_(source_id) {}

IdentifiabilityMetricBuilder::~IdentifiabilityMetricBuilder() = default;

IdentifiabilityMetricBuilder& IdentifiabilityMetricBuilder::Set(
    IdentifiableSurface surface,
    IdentifiableToken value) {
  metrics_.emplace_back(surface, value);
  return *this;
}

void IdentifiabilityMetricBuilder::Record(ukm::UkmRecorder* recorder) {
  auto* collector = IdentifiabilitySampleCollector::Get();
  if (collector && !metrics_.empty())
    collector->Record(recorder, source_id_.ToInt64(), std::move(metrics_));
}

}  // namespace blink
