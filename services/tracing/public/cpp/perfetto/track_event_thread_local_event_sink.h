// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_EVENT_THREAD_LOCAL_EVENT_SINK_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_EVENT_THREAD_LOCAL_EVENT_SINK_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/time/time.h"
#include "base/trace_event/thread_instruction_count.h"
#include "services/tracing/public/cpp/perfetto/interning_index.h"
#include "services/tracing/public/cpp/perfetto/thread_local_event_sink.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/protozero/message_handle.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"

namespace perfetto {
class StartupTraceWriter;
}  // namespace perfetto

namespace tracing {

// ThreadLocalEventSink that emits TrackEvent protos.
class COMPONENT_EXPORT(TRACING_CPP) TrackEventThreadLocalEventSink
    : public ThreadLocalEventSink,
      public base::ThreadIdNameManager::Observer {
 public:
  TrackEventThreadLocalEventSink(
      std::unique_ptr<perfetto::StartupTraceWriter> trace_writer,
      uint32_t session_id,
      bool disable_interning,
      bool proto_writer_filtering_enabled);
  ~TrackEventThreadLocalEventSink() override;

  // Resets emitted incremental state on all threads and causes incremental data
  // (e.g. interning index entries and a ThreadDescriptor) to be emitted again.
  static void ClearIncrementalState();

  // ThreadLocalEventSink implementation:
  void AddTraceEvent(base::trace_event::TraceEvent* trace_event,
                     base::trace_event::TraceEventHandle* handle) override;
  void UpdateDuration(base::trace_event::TraceEventHandle handle,
                      const base::TimeTicks& now,
                      const base::ThreadTicks& thread_now,
                      base::trace_event::ThreadInstructionCount
                          thread_instruction_now) override;
  void Flush() override;

  // ThreadIdNameManager::Observer implementation:
  void OnThreadNameChanged(const char* name) override;

 private:
  static constexpr size_t kMaxCompleteEventDepth = 30;

  void EmitThreadDescriptor(
      protozero::MessageHandle<perfetto::protos::pbzero::TracePacket>*
          trace_packet,
      base::trace_event::TraceEvent* trace_event,
      bool explicit_timestamp,
      const char* maybe_new_name = nullptr);
  void DoResetIncrementalState(base::trace_event::TraceEvent* trace_event,
                               bool explicit_timestamp);

  // TODO(eseckler): Make it possible to register new indexes for use from
  // TRACE_EVENT macros.
  InterningIndex<TypeList<const char*>, SizeList<128>>
      interned_event_categories_;
  InterningIndex<TypeList<const char*, std::string>, SizeList<512, 64>>
      interned_event_names_;
  InterningIndex<TypeList<const char*, std::string>, SizeList<512, 64>>
      interned_annotation_names_;
  InterningIndex<TypeList<std::tuple<const char*, const char*, int>>,
                 SizeList<512>>
      interned_source_locations_;
  InterningIndex<TypeList<std::string>, SizeList<128>>
      interned_log_message_bodies_;

  static std::atomic<uint32_t> incremental_state_reset_id_;

  bool reset_incremental_state_ = true;
  uint32_t last_incremental_state_reset_id_ = 0;
  base::TimeTicks last_timestamp_;
  base::ThreadTicks last_thread_time_;
  base::trace_event::ThreadInstructionCount last_thread_instruction_count_;
  int process_id_;
  int thread_id_;
  std::string thread_name_;
  perfetto::protos::pbzero::ThreadDescriptor::ChromeThreadType thread_type_ =
      perfetto::protos::pbzero::ThreadDescriptor::CHROME_THREAD_UNSPECIFIED;

  base::trace_event::TraceEvent complete_event_stack_[kMaxCompleteEventDepth];
  uint32_t current_stack_depth_ = 0;

  const bool privacy_filtering_enabled_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_EVENT_THREAD_LOCAL_EVENT_SINK_H_
