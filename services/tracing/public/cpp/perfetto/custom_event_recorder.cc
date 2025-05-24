// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/custom_event_recorder.h"

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/process/current_process.h"
#include "base/sequence_checker.h"
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

constexpr char kUserActionSamplesCategory[] =
    TRACE_DISABLED_BY_DEFAULT("user_action_samples");

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);

  // The legacy chrome_config is only used to specify histogram names.
  auto legacy_config = TraceConfig(args.config->chrome_config().trace_config());
  ResetHistograms(legacy_config.histogram_names());
}

void CustomEventRecorder::OnStart(const perfetto::DataSourceBase::StartArgs&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
  EmitRecurringUpdates();

  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kUserActionSamplesCategory, &enabled);
  if (enabled) {
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

void CustomEventRecorder::OnStop(const perfetto::DataSourceBase::StopArgs&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);

  // Write metadata events etc.
  LogHistograms();

  bool enabled;
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

void CustomEventRecorder::WillClearIncrementalState(
    const perfetto::DataSourceBase::ClearIncrementalStateArgs&) {
  EmitRecurringUpdates();
}

void CustomEventRecorder::OnUserActionSampleCallback(
    const std::string& action,
    base::TimeTicks action_time) {
  constexpr uint64_t kGlobalInstantTrackId = 0;
  TRACE_EVENT_INSTANT(
      kUserActionSamplesCategory, "UserAction",
      perfetto::NamedTrack("UserAction", 0,
                           perfetto::Track::Global(kGlobalInstantTrackId)),
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
  std::string buckets = base::Base64Encode(
      std::string_view(pickle.data_as_char(), pickle.size()));
  TRACE_EVENT_INSTANT2("benchmark,uma", "UMAHistogramSamples",
                       TRACE_EVENT_SCOPE_PROCESS, "name",
                       histogram->histogram_name(), "buckets", buckets);
}

void CustomEventRecorder::ResetHistograms(
    const std::unordered_set<std::string>& histogram_names) {
  histograms_.clear();
  startup_histogram_samples_.clear();
  for (const std::string& histogram_name : histogram_names) {
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

void CustomEventRecorder::DetachFromSequence() {
  DETACH_FROM_SEQUENCE(perfetto_sequence_checker_);
}

}  // namespace tracing
