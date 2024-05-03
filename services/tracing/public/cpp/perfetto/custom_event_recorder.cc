// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/custom_event_recorder.h"

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/process/current_process.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/trace_string_lookup.h"
#include "third_party/perfetto/include/perfetto/tracing/internal/track_event_internal.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_interned_data_index.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_active_processes.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_histogram_sample.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_process_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_user_event.pbzero.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#include "base/trace_event/application_state_proto_android.h"
#endif

using TraceConfig = base::trace_event::TraceConfig;
using perfetto::protos::pbzero::ChromeProcessDescriptor;

namespace tracing {
namespace {

constexpr char kHistogramSamplesCategory[] =
    TRACE_DISABLED_BY_DEFAULT("histogram_samples");
constexpr char kUserActionSamplesCategory[] =
    TRACE_DISABLED_BY_DEFAULT("user_action_samples");

base::SequencedTaskRunner* GetTaskRunner() {
  return PerfettoTracedProcess::Get()
      ->GetTaskRunner()
      ->GetOrCreateTaskRunner()
      .get();
}

struct InternedHistogramName
    : public perfetto::TrackEventInternedDataIndex<
          InternedHistogramName,
          perfetto::protos::pbzero::InternedData::kHistogramNamesFieldNumber,
          const char*> {
  static void Add(perfetto::protos::pbzero::InternedData* interned_data,
                  size_t iid,
                  const char* histogram_name) {
    auto* msg = interned_data->add_histogram_names();
    msg->set_iid(iid);
    msg->set_name(histogram_name);
  }
};

}  // namespace

CustomEventRecorder::CustomEventRecorder() {
  DETACH_FROM_SEQUENCE(perfetto_sequence_checker_);
  base::TrackEvent::AddSessionObserver(this);
}

CustomEventRecorder::~CustomEventRecorder()
{
  base::TrackEvent::RemoveSessionObserver(this);
}

// static
CustomEventRecorder* CustomEventRecorder::GetInstance() {
  static base::NoDestructor<CustomEventRecorder> instance;
  return instance.get();
}

// static
void CustomEventRecorder::EmitRecurringUpdates() {
  auto* instance = CustomEventRecorder::GetInstance();
  if (instance && instance->active_processes_callback_) {
    const auto pids = instance->active_processes_callback_.Run();
    TRACE_EVENT_INSTANT("__metadata", "ActiveProcesses",
                        perfetto::Track::Global(0),
                        [&pids](perfetto::EventContext ctx) {
                          auto* active_processes =
                              ctx.event<perfetto::protos::pbzero::TrackEvent>()
                                  ->set_chrome_active_processes();
                          for (const auto& pid : pids) {
                            active_processes->add_pid(pid);
                          }
                        });
  }
#if BUILDFLAG(IS_ANDROID)
  static const ChromeProcessDescriptor::ProcessType process_type =
      base::CurrentProcess::GetInstance().GetType({});
  if (process_type == ChromeProcessDescriptor::PROCESS_BROWSER) {
    auto state = base::android::ApplicationStatusListener::GetState();
    TRACE_APPLICATION_STATE(state);
  }
#endif
}

void CustomEventRecorder::OnSetup(
    const perfetto::DataSourceBase::SetupArgs& args) {
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&CustomEventRecorder::OnTracingStarted,
                                base::Unretained(this), *args.config));
}

void CustomEventRecorder::OnStop(
    const perfetto::DataSourceBase::StopArgs& args) {
  std::function<void()> finish_async_stop = args.HandleStopAsynchronously();
  base::OnceClosure stop_callback = base::BindOnce(
      [](std::function<void()> callback) { callback(); }, finish_async_stop);
  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CustomEventRecorder::OnTracingStopped,
                     base::Unretained(this), std::move(stop_callback)));
}

void CustomEventRecorder::WillClearIncrementalState(
    const perfetto::DataSourceBase::ClearIncrementalStateArgs&) {
  EmitRecurringUpdates();
}

void CustomEventRecorder::OnStartupTracingStarted(
    const TraceConfig& trace_config,
    bool /*privacy_filtering_enabled*/) {
  DCHECK(monitored_histograms_.empty());
  if (trace_config.IsCategoryGroupEnabled(kHistogramSamplesCategory) &&
      trace_config.histogram_names().empty()) {
    // The global callback can be added early at startup before main message
    // loop is created. But histogram specific observers need task runner and
    // are added when tracing service is setup in OnTracingStarted() instead.
    base::StatisticsRecorder::SetGlobalSampleCallback(
        &CustomEventRecorder::OnMetricsSampleCallback);
  }
}

// TODO(khokhlov): In SDK build, this method can be called at startup, before
// the task runner is created. Factor out the parts that can be called early
// into OnStartupTracingStarted, and make sure each part is called at the
// appropriate time.
void CustomEventRecorder::OnTracingStarted(
    const perfetto::DataSourceConfig& data_source_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);

  auto trace_config =
      TraceConfig(data_source_config.chrome_config().trace_config());

  EmitRecurringUpdates();
  ResetHistograms(trace_config);

  if (trace_config.IsCategoryGroupEnabled(kHistogramSamplesCategory)) {
    if (trace_config.histogram_names().empty() &&
        !base::StatisticsRecorder::global_sample_callback()) {
      // Add the global callback if it wasn't already.
      base::StatisticsRecorder::SetGlobalSampleCallback(
          &CustomEventRecorder::OnMetricsSampleCallback);
    }
    for (const std::string& histogram_name : trace_config.histogram_names()) {
      if (monitored_histograms_.count(histogram_name)) {
        continue;
      }
      monitored_histograms_[histogram_name] = std::make_unique<
          base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindRepeating(&CustomEventRecorder::OnMetricsSampleCallback));
    }
  }

  if (trace_config.IsCategoryGroupEnabled(kUserActionSamplesCategory)) {
    auto task_runner = base::GetRecordActionTaskRunner();
    if (task_runner) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce([]() {
            // Attempt to remove an existing callback (this will do nothing if
            // there's no callback), to ensure that at most one callback is
            // registered in the presence of multiple active tracing sessions.
            base::RemoveActionCallback(
                CustomEventRecorder::GetInstance()->user_action_callback_);
            base::AddActionCallback(
                CustomEventRecorder::GetInstance()->user_action_callback_);
          }));
    }
  }
}

void CustomEventRecorder::OnTracingStopped(
    base::OnceClosure stop_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);

  // Write metadata events etc.
  LogHistograms();

  // We have to flush explicitly because we're using the asynchronous stop
  // mechanism.
  base::TrackEvent::Flush();
  std::move(stop_complete_callback).Run();

  // Clean up callbacks if no tracing sessions are recording samples.
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kHistogramSamplesCategory, &enabled);
  if (!enabled) {
    base::StatisticsRecorder::SetGlobalSampleCallback(nullptr);
    monitored_histograms_.clear();
  }

  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kUserActionSamplesCategory, &enabled);
  if (!enabled) {
    auto task_runner = base::GetRecordActionTaskRunner();
    if (task_runner) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce([]() {
            base::RemoveActionCallback(
                CustomEventRecorder::GetInstance()->user_action_callback_);
          }));
    }
  }
}

void CustomEventRecorder::OnUserActionSampleCallback(
    const std::string& action,
    base::TimeTicks action_time) {
  constexpr uint64_t kGlobalInstantTrackId = 0;
  TRACE_EVENT_INSTANT(
      kUserActionSamplesCategory, "UserAction",
      perfetto::Track::Global(kGlobalInstantTrackId),
      [&](perfetto::EventContext ctx) {
        perfetto::protos::pbzero::ChromeUserEvent* new_sample =
            ctx.event()->set_chrome_user_event();
        if (!ctx.ShouldFilterDebugAnnotations()) {
          new_sample->set_action(action);
        }
        new_sample->set_action_hash(base::HashMetricName(action));
      });
}

void CustomEventRecorder::LogHistogram(base::HistogramBase* histogram) {
  if (!histogram) {
    return;
  }
  // For the purpose of calculating metrics from histograms we only want the
  // delta of the events.
  auto samples = histogram->SnapshotSamples();

  // If there were HistogramSamples recorded during startup, then those should
  // be subtracted from the overall set. This way we only report the samples
  // that occurred during the run.
  auto it = startup_histogram_samples_.find(histogram->histogram_name());
  if (it != startup_histogram_samples_.end()) {
    samples->Subtract(*it->second.get());
  }
  base::Pickle pickle;
  samples->Serialize(&pickle);
  std::string buckets =
      base::Base64Encode(std::string(pickle.data_as_char(), pickle.size()));
  TRACE_EVENT_INSTANT2("benchmark,uma", "UMAHistogramSamples",
                       TRACE_EVENT_SCOPE_PROCESS, "name",
                       histogram->histogram_name(), "buckets", buckets);
}

void CustomEventRecorder::ResetHistograms(const TraceConfig& trace_config) {
  histograms_.clear();
  startup_histogram_samples_.clear();
  for (const std::string& histogram_name : trace_config.histogram_names()) {
    histograms_.push_back(histogram_name);
    auto* histogram = base::StatisticsRecorder::FindHistogram(histogram_name);
    if (!histogram) {
      continue;
    }

    // For the purpose of calculating metrics from histograms we only want the
    // delta of the events. However we do not want to emit the results when
    // resetting. This will allow LogHistogram to emit one UMAHistogramSamples
    // which encompasses only the histograms recorded during the trace. We
    // cache the initial HistogramSamples so that they can be subtracted from
    // the full snapshot at the end.
    startup_histogram_samples_.emplace(histogram_name,
                                       histogram->SnapshotSamples());
  }
}

void CustomEventRecorder::LogHistograms() {
  for (const std::string& histogram_name : histograms_) {
    LogHistogram(base::StatisticsRecorder::FindHistogram(histogram_name));
  }
}

// static
void CustomEventRecorder::OnMetricsSampleCallback(
    const char* histogram_name,
    uint64_t name_hash,
    base::HistogramBase::Sample sample) {
  TRACE_EVENT_INSTANT(
      kHistogramSamplesCategory, "HistogramSample",
      [&](perfetto::EventContext ctx) {
        perfetto::protos::pbzero::ChromeHistogramSample* new_sample =
            ctx.event()->set_chrome_histogram_sample();
        new_sample->set_name_hash(name_hash);
        new_sample->set_sample(sample);
        if (!ctx.ShouldFilterDebugAnnotations()) {
          size_t iid = InternedHistogramName::Get(&ctx, histogram_name);
          new_sample->set_name_iid(iid);
        }
      });
}

}  // namespace tracing
