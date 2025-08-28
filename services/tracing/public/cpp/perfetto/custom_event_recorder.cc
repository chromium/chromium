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
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_enums.pbzero.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/trace_string_lookup.h"
#include "third_party/perfetto/include/perfetto/tracing/internal/track_event_internal.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_interned_data_index.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_active_processes.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_user_event.pbzero.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#include "base/trace_event/application_state_proto_android.h"
#endif

using TraceConfig = base::trace_event::TraceConfig;
namespace pbzero_enums = perfetto::protos::chrome_enums::pbzero;

namespace tracing {

CustomEventRecorder::CustomEventRecorder() {
  DETACH_FROM_SEQUENCE(perfetto_sequence_checker_);
  base::TrackEvent::AddSessionObserver(this);
}

CustomEventRecorder::~CustomEventRecorder() {
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
  static const pbzero_enums::ProcessType process_type =
      base::CurrentProcess::GetInstance().GetType({});
  if (process_type == pbzero_enums::PROCESS_BROWSER) {
    auto state = base::android::ApplicationStatusListener::GetState();
    TRACE_APPLICATION_STATE(state);
  }
#endif
}

void CustomEventRecorder::OnStart(const perfetto::DataSourceBase::StartArgs&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
  EmitRecurringUpdates();
}

void CustomEventRecorder::WillClearIncrementalState(
    const perfetto::DataSourceBase::ClearIncrementalStateArgs&) {
  EmitRecurringUpdates();
}

void CustomEventRecorder::DetachFromSequence() {
  DETACH_FROM_SEQUENCE(perfetto_sequence_checker_);
}

}  // namespace tracing
