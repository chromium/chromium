// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/scoped_identifiability_test_sample_collector.h"

#include <memory>

#include "base/notreached.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "third_party/blink/common/privacy_budget/aggregating_sample_collector.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"
#include "third_party/blink/public/common/privacy_budget/scoped_switch_sample_collector.h"

namespace blink {
namespace test {

ScopedIdentifiabilityTestSampleCollector::
    ScopedIdentifiabilityTestSampleCollector()
    : scoped_default_(this) {}

ScopedIdentifiabilityTestSampleCollector::
    ~ScopedIdentifiabilityTestSampleCollector() = default;

void ScopedIdentifiabilityTestSampleCollector::Record(
    ukm::UkmRecorder* recorder,
    ukm::SourceId source,
    std::vector<IdentifiableSample> metrics) {
  entries_.emplace_back(source, std::move(metrics));
  AggregatingSampleCollector::UkmMetricsContainerType metrics_map;
  for (auto metric : entries_.back().metrics) {
    metrics_map.emplace(metric.surface.ToUkmMetricHash(),
                        metric.value.ToUkmMetricValue());
  }
  recorder->AddEntry(ukm::mojom::UkmEntry::New(
      source, ukm::builders::Identifiability::kEntryNameHash,
      std::move(metrics_map)));
}

void ScopedIdentifiabilityTestSampleCollector::Flush(
    ukm::UkmRecorder* recorder) {}

void ScopedIdentifiabilityTestSampleCollector::FlushSource(
    ukm::UkmRecorder* recorder,
    ukm::SourceId source) {}

void ScopedIdentifiabilityTestSampleCollector::ClearEntries() {
  entries_.clear();
}

}  // namespace test
}  // namespace blink
