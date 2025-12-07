// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_HISTOGRAM_SAMPLES_DATA_SOURCE_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_HISTOGRAM_SAMPLES_DATA_SOURCE_H_

#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/histogram_samples.gen.h"

namespace tracing {

struct HistogramSamplesIncrementalState;
struct HistogramSamplesTlsState;
struct HistogramSamplesTraits : public perfetto::DefaultDataSourceTraits {
  using IncrementalStateType = HistogramSamplesIncrementalState;
  using TlsStateType = HistogramSamplesTlsState;
};

// A data source that record UMA histogram samples. This data source needs
// "track_event" data source to be enabled for track descriptors to be emitted.
class COMPONENT_EXPORT(TRACING_CPP) HistogramSamplesDataSource
    : public perfetto::DataSource<HistogramSamplesDataSource,
                                  HistogramSamplesTraits> {
 public:
  static constexpr bool kRequiresCallbacksUnderLock = false;

  static void Register();

  HistogramSamplesDataSource();
  ~HistogramSamplesDataSource() override;

  void OnSetup(const SetupArgs&) override;
  void OnStart(const StartArgs&) override;
  void OnFlush(const FlushArgs&) override;
  void OnStop(const StopArgs&) override;

  bool filter_histogram_names() const { return filter_histogram_names_; }
  bool records_all_histograms() const { return monitored_histograms_.empty(); }

 private:
  void OnMetricSample(
      std::optional<base::HistogramBase::Sample32> reference_lower_value,
      std::optional<base::HistogramBase::Sample32> reference_upper_value,
      std::optional<uint64_t> event_id,
      std::string_view histogram_name,
      uint64_t name_hash,
      base::HistogramBase::Sample32 actual_value);
  static void OnAnyMetricSample(std::string_view histogram_name,
                                uint64_t name_hash,
                                base::HistogramBase::Sample32 sample,
                                std::optional<uint64_t> event_id);
  // `instance` identifies the instance that registered a callback, or nullopt
  // if this is a global callback.
  static void OnMetricSampleImpl(std::optional<uint64_t> event_id,
                                 std::string_view histogram_name,
                                 uint64_t name_hash,
                                 base::HistogramBase::Sample32 sample,
                                 std::optional<uintptr_t> instance);

  static void ResetIncrementalState(TraceContext& ctx,
                                    bool records_all_histograms);

  std::vector<
      perfetto::protos::gen::ChromiumHistogramSamplesConfig::HistogramSample>
      monitored_histograms_;

  // Stores the registered histogram callbacks for which OnMetricSample
  // was set individually.
  std::vector<
      std::unique_ptr<base::StatisticsRecorder::ScopedHistogramSampleObserver>>
      histogram_observers_;

  bool filter_histogram_names_ = false;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_HISTOGRAM_SAMPLES_DATA_SOURCE_H_
