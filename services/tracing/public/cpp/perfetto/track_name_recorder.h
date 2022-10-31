// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_NAME_RECORDER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_NAME_RECORDER_H_

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/perfetto/include/perfetto/tracing/internal/track_event_internal.h"

namespace tracing {

absl::optional<uint64_t> GetTraceCrashId();

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
// A class that emits track descriptors for Chrome processes and threads.
class COMPONENT_EXPORT(TRACING_CPP) TrackNameRecorder
    : public perfetto::TrackEventSessionObserver,
      base::ThreadIdNameManager::Observer {
 public:
  static TrackNameRecorder* GetInstance();

  TrackNameRecorder(const TrackNameRecorder&) = delete;
  TrackNameRecorder& operator=(const TrackNameRecorder&) = delete;

  // perfetto::TrackEventSessionObserver implementation
  void OnSetup(const perfetto::DataSourceBase::SetupArgs&) override;
  void OnStop(const perfetto::DataSourceBase::StopArgs&) override;
  void WillClearIncrementalState(
      const perfetto::DataSourceBase::ClearIncrementalStateArgs&) override;

  // base::ThreadIdNameManager::Observer implementation.
  void OnThreadNameChanged(const char* name) override;

 private:
  friend class base::NoDestructor<TrackNameRecorder>;

  TrackNameRecorder();
  ~TrackNameRecorder() override;

  uint64_t process_start_timestamp_;
};
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_NAME_RECORDER_H_
