// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_CUSTOM_EVENT_RECORDER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_CUSTOM_EVENT_RECORDER_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/perfetto/include/perfetto/tracing/internal/track_event_internal.h"

namespace base {
template <typename T>
class NoDestructor;
}

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
  void OnStart(const perfetto::DataSourceBase::StartArgs&) override;
  void WillClearIncrementalState(
      const perfetto::DataSourceBase::ClearIncrementalStateArgs&) override;

  void SetActiveProcessesCallback(ActiveProcessesCallback callback) {
    active_processes_callback_ = callback;
  }

  // Thread can restart in Linux and ChromeOS when entering sandbox, so rebind
  // sequence checker.
  void DetachFromSequence();

 private:
  friend class base::NoDestructor<CustomEventRecorder>;

  CustomEventRecorder();
  ~CustomEventRecorder() override;

  SEQUENCE_CHECKER(perfetto_sequence_checker_);

  ActiveProcessesCallback active_processes_callback_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_CUSTOM_EVENT_RECORDER_H_
