// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_CUSTOM_EVENT_RECORDER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_CUSTOM_EVENT_RECORDER_H_

#include "base/component_export.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/sequence_checker.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/perfetto/include/perfetto/tracing/internal/track_event_internal.h"

namespace tracing {

// A class that accompanies TraceEventDataSource to record some additional
// events like UMA histogram samples or user action samples.
class COMPONENT_EXPORT(TRACING_CPP) CustomEventRecorder
    : public perfetto::TrackEventSessionObserver {
 public:
  using ActiveProcessesCallback =
      base::RepeatingCallback<std::set<base::ProcessId>()>;

  static CustomEventRecorder* GetInstance();
  static void EmitRecurringUpdates();

  CustomEventRecorder(const CustomEventRecorder&) = delete;
  CustomEventRecorder& operator=(const CustomEventRecorder&) = delete;

  // perfetto::TrackEventSessionObserver implementation
  void OnSetup(const perfetto::DataSourceBase::SetupArgs&) override;
  void OnStop(const perfetto::DataSourceBase::StopArgs&) override;
  void WillClearIncrementalState(
      const perfetto::DataSourceBase::ClearIncrementalStateArgs&) override;

  void OnStartupTracingStarted(
      const base::trace_event::TraceConfig& trace_config,
      bool privacy_filtering_enabled);
  void OnTracingStarted(const perfetto::DataSourceConfig& data_source_config);
  void OnTracingStopped(base::OnceClosure stop_complete_callback);

  void SetActiveProcessesCallback(ActiveProcessesCallback callback) {
    active_processes_callback_ = callback;
  }

  // Registered as a callback to receive every action recorded using
  // base::RecordAction(), when tracing is enabled with a histogram category.
  static void OnUserActionSampleCallback(const std::string& action,
                                         base::TimeTicks action_time);
  // Records trace event for a histogram sample. When histogram_samples category
  // is enabled, it is registered with base::StatisticsRecorder to monitor the
  // histograms listed in the trace config. If there are no histograms listed in
  // the trace config, all the histograms will be monitored.
  static void OnMetricsSampleCallback(const char* histogram_name,
                                      uint64_t name_hash,
                                      base::HistogramBase::Sample sample);
  bool IsPrivacyFilteringEnabled();

 private:
  friend class base::NoDestructor<CustomEventRecorder>;

  CustomEventRecorder();
  ~CustomEventRecorder() override;

  SEQUENCE_CHECKER(perfetto_sequence_checker_);
  // Extracts UMA histogram names that should be logged in traces and logs their
  // starting values.
  void ResetHistograms(const base::trace_event::TraceConfig& trace_config);
  // Logs selected UMA histogram.
  void LogHistograms();
  // Logs a given histogram in traces.
  void LogHistogram(base::HistogramBase* histogram);

  // For each of the Histogram that we are tracking, cache the snapshot of their
  // HistogramSamples from before tracing began. So that we can calculate the
  // delta when we go to LogHistograms.
  std::map<std::string, std::unique_ptr<base::HistogramSamples>>
      startup_histogram_samples_;
  std::vector<std::string> histograms_;
  // Stores the registered histogram callbacks for which OnMetricsSampleCallback
  // was set individually.
  std::unordered_map<
      std::string,
      std::unique_ptr<base::StatisticsRecorder::ScopedHistogramSampleObserver>>
      monitored_histograms_;
  base::ActionCallback user_action_callback_ =
      base::BindRepeating(&CustomEventRecorder::OnUserActionSampleCallback);
  ActiveProcessesCallback active_processes_callback_;

  base::Lock lock_;
  bool privacy_filtering_enabled_ GUARDED_BY(lock_) = false;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_CUSTOM_EVENT_RECORDER_H_
