// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/privacy_budget/aggregating_sample_collector.h"

#include <type_traits>
#include <unordered_map>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_sample.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace blink {
namespace internal {
// Per-process singleton.
AggregatingSampleCollector* GetCollectorInstance() {
  static base::NoDestructor<AggregatingSampleCollector> impl;
  return impl.get();
}
}  // namespace internal

namespace {
bool IsStudyActive() {
  return IdentifiabilityStudySettings::Get()->IsActive();
}
}  // namespace

const unsigned AggregatingSampleCollector::kMaxTrackedSources;
const unsigned AggregatingSampleCollector::kMaxTrackedSurfaces;
const unsigned
    AggregatingSampleCollector::kMaxTrackedSamplesPerSurfacePerSourceId;
const unsigned AggregatingSampleCollector::kMaxUnsentSamples;
const unsigned AggregatingSampleCollector::kMaxUnsentSources;
const base::TimeDelta AggregatingSampleCollector::kMaxUnsentSampleAge;

AggregatingSampleCollector::AggregatingSampleCollector() = default;
AggregatingSampleCollector::~AggregatingSampleCollector() = default;

void AggregatingSampleCollector::Record(
    ukm::UkmRecorder* recorder,
    ukm::SourceId source,
    std::vector<IdentifiableSample> samples) {
  // recorder == nullptr or source == kInvalidSourceId can happen, for example,
  // if metrics are being reported against an unsupported ExecutionContext type
  // or for some reason the UkmRecorder or a valid source is unavailable.
  if (!IsStudyActive() || !recorder || source == ukm::kInvalidSourceId)
    return;

  if (TryAcceptSamples(source, std::move(samples)))
    Flush(recorder);
}

void AggregatingSampleCollector::Flush(ukm::UkmRecorder* recorder) {
  if (!recorder)
    return;

  std::unordered_multimap<ukm::SourceId, UkmMetricsContainerType> unsent;
  // Gratuitous block for releasing `lock_` after doing the minimal possible
  // work.
  {
    base::AutoLock l(lock_);
    if (unsent_sample_count_ == 0)
      return;

    unsent_metrics_.swap(unsent);
    unsent_sample_count_ = 0;
  }

  for (auto& kv : unsent) {
    auto entry = ukm::mojom::UkmEntry::New(
        kv.first, ukm::builders::Identifiability::kEntryNameHash,
        std::move(kv.second));
    recorder->AddEntry(std::move(entry));
  }
}

void AggregatingSampleCollector::FlushSource(ukm::UkmRecorder* recorder,
                                             ukm::SourceId source) {
  if (!IsStudyActive() || !recorder)
    return;

  std::vector<UkmMetricsContainerType> metric_sets;

  {
    base::AutoLock l(lock_);
    per_source_per_surface_samples_.erase(source);

    if (unsent_sample_count_ == 0)
      return;

    if (unsent_metrics_.count(source) == 0)
      return;

    const auto bucket = unsent_metrics_.bucket(source);
    for (auto it = unsent_metrics_.begin(bucket);
         it != unsent_metrics_.end(bucket); ++it) {
      if (it->first != source)
        continue;

      DCHECK_GE(unsent_sample_count_, it->second.size());
      unsent_sample_count_ -= it->second.size();
      metric_sets.emplace_back(std::move(it->second));
    }

    unsent_metrics_.erase(source);
  }

  for (auto& metric : metric_sets) {
    auto entry = ukm::mojom::UkmEntry::New(
        source, ukm::builders::Identifiability::kEntryNameHash,
        std::move(metric));
    recorder->AddEntry(std::move(entry));
  }
}

void AggregatingSampleCollector::ResetForTesting() {
  base::AutoLock l(lock_);

  per_source_per_surface_samples_.clear();
  unsent_metrics_.clear();
  unsent_sample_count_ = 0;
}

bool AggregatingSampleCollector::TryAcceptSamples(
    ukm::SourceId source,
    std::vector<IdentifiableSample> samples) {
  base::AutoLock l(lock_);
  for (const auto& sample : samples)
    TryAcceptSingleSample(source, sample);

  // This check needs to happen regardless of whether any new samples could be
  // accepted due to the max age check.
  return unsent_sample_count_ > kMaxUnsentSamples ||
         unsent_metrics_.size() > kMaxUnsentSources ||
         (unsent_sample_count_ > 0 &&
          base::TimeTicks::Now() - time_of_first_unsent_arrival_ >=
              kMaxUnsentSampleAge);
}

void AggregatingSampleCollector::TryAcceptSingleSample(
    ukm::SourceId new_source,
    const IdentifiableSample& new_sample) {
  if (!seen_surfaces_.count(new_sample.surface)) {
    if (seen_surfaces_.size() >= kMaxTrackedSurfaces) {
      // New surface, but can't add any more.
      UMA_HISTOGRAM_ENUMERATION(
          "PrivacyBudget.Identifiability.RecordedSample",
          PrivacyBudgetRecordedSample::kDroppedMaxTrackedSurfaces);
      return;
    }
  }

  auto surfaces_for_source_it =
      per_source_per_surface_samples_.find(new_source);
  if (surfaces_for_source_it == per_source_per_surface_samples_.end()) {
    // First time we see this source id.

    if (per_source_per_surface_samples_.size() >= kMaxTrackedSources) {
      UMA_HISTOGRAM_ENUMERATION(
          "PrivacyBudget.Identifiability.RecordedSample",
          PrivacyBudgetRecordedSample::kDroppedMaxTrackedSources);
      return;
    }

    per_source_per_surface_samples_.emplace(
        new_source,
        std::unordered_map<IdentifiableSurface, Samples,
                           IdentifiableSurfaceHash>(
            {{new_sample.surface, Samples{.samples = {{new_sample.value}},
                                          .total_value_count = 1}}}));
  } else {
    auto samples_for_surface_it =
        surfaces_for_source_it->second.find(new_sample.surface);

    if (samples_for_surface_it == surfaces_for_source_it->second.end()) {
      surfaces_for_source_it->second.emplace(
          new_sample.surface,
          Samples{.samples = {{new_sample.value}}, .total_value_count = 1});
    } else {
      Samples& sample_set = samples_for_surface_it->second;
      ++sample_set.total_value_count;

      // Already exists.
      if (sample_set.samples.contains(new_sample.value))
        return;

      // Want to add one, but can't.
      if (sample_set.samples.size() >=
          kMaxTrackedSamplesPerSurfacePerSourceId) {
        sample_set.overflowed = true;
        UMA_HISTOGRAM_ENUMERATION(
            "PrivacyBudget.Identifiability.RecordedSample",
            PrivacyBudgetRecordedSample::kDroppedMaxTrackedPerSurfacePerSource);
        return;
      }

      sample_set.samples.insert(new_sample.value);
    }
  }

  seen_surfaces_.insert(new_sample.surface);

  UMA_HISTOGRAM_ENUMERATION("PrivacyBudget.Identifiability.RecordedSample",
                            PrivacyBudgetRecordedSample::kAccepted);
  AddNewUnsentSample(new_source, new_sample);
}

void AggregatingSampleCollector::AddNewUnsentSample(
    ukm::SourceId source,
    const IdentifiableSample& new_sample) {
  const auto kNewKey = new_sample.surface.ToUkmMetricHash();
  const auto kNewValue = new_sample.value.ToUkmMetricValue();

  if (!AddNewUnsentSampleToKnownSource(source, kNewKey, kNewValue)) {
    unsent_metrics_.emplace(source,
                            UkmMetricsContainerType({{kNewKey, kNewValue}}));
  }
  DCHECK_LE(unsent_metrics_.count(source),
            kMaxTrackedSamplesPerSurfacePerSourceId);

  ++unsent_sample_count_;

  // Age of the oldest sample determines the expiry of the entire list of unsent
  // samples.
  if (unsent_sample_count_ == 1)
    time_of_first_unsent_arrival_ = base::TimeTicks::Now();
}

bool AggregatingSampleCollector::AddNewUnsentSampleToKnownSource(
    ukm::SourceId source,
    uint64_t key,
    int64_t value) {
  if (unsent_metrics_.bucket_count() == 0)
    return false;

  const auto kSourceBucket = unsent_metrics_.bucket(source);
  for (auto metric_map_it = unsent_metrics_.begin(kSourceBucket);
       metric_map_it != unsent_metrics_.end(kSourceBucket); ++metric_map_it) {
    // There could be bucket collisions.
    if (metric_map_it->first != source)
      continue;

    // result.second is true if the insertion was successful. I.e. `key` didn't
    // exist before.
    auto result = metric_map_it->second.try_emplace(key, value);
    if (result.second)
      return true;
  }
  return false;
}

}  // namespace blink
