// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_EVENT_THREAD_LOCAL_EVENT_SINK_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_EVENT_THREAD_LOCAL_EVENT_SINK_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/trace_event/trace_event.h"

#include "base/component_export.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "base/trace_event/typed_macros_embedder_support.h"
#include "services/tracing/public/cpp/perfetto/interning_index.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/protozero/message_handle.h"
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_thread_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/counter_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace tracing {

// ThreadLocalEventSink that emits TrackEvent protos.
class COMPONENT_EXPORT(TRACING_CPP) TrackEventThreadLocalEventSink
    : public base::ThreadIdNameManager::Observer,
      public base::trace_event::TrackEventHandle::CompletionListener,
      public base::trace_event::TracePacketHandle::CompletionListener {
 public:
  TrackEventThreadLocalEventSink(
      std::unique_ptr<perfetto::TraceWriter> trace_writer,
      uint32_t session_id,
      bool disable_interning,
      bool proto_writer_filtering_enabled);

  TrackEventThreadLocalEventSink(const TrackEventThreadLocalEventSink&) =
      delete;
  TrackEventThreadLocalEventSink& operator=(
      const TrackEventThreadLocalEventSink&) = delete;

  ~TrackEventThreadLocalEventSink() override;

  // Resets emitted incremental state on all threads and causes incremental data
  // (e.g. interning index entries and a ThreadDescriptor) to be emitted again.
  static void ClearIncrementalState();

  void AddLegacyTraceEvent(base::trace_event::TraceEvent* trace_event,
                           base::trace_event::TraceEventHandle* handle);

  base::trace_event::TrackEventHandle AddTypedTraceEvent(
      base::trace_event::TraceEvent* trace_event);
  base::trace_event::TracePacketHandle AddTracePacket();
  void AddEmptyPacket();

  void UpdateDuration(const unsigned char* category_group_enabled,
                      const char* name,
                      base::trace_event::TraceEventHandle handle,
                      int thread_id,
                      bool explicit_timestamps,
                      const base::TimeTicks& now,
                      const base::ThreadTicks& thread_now);
  void Flush();

  uint32_t session_id() const { return session_id_; }

  // ThreadIdNameManager::Observer implementation:
  void OnThreadNameChanged(const char* name) override;

  // base::trace_event::TrackEventHandle::CompletionListener implementation:
  void OnTrackEventCompleted() override;

  // base::trace_event::TracePacketHandle::CompletionListener implementation:
  void OnTracePacketCompleted() override;

 private:
  static constexpr size_t kMaxCompleteEventDepth = 30;

  enum class PacketType { kDefault, kEmpty };
  perfetto::TraceWriter::TracePacketHandle NewTracePacket(
      PacketType = PacketType::kDefault);

  // Emit any necessary descriptors that we haven't emitted yet and, if
  // required, perform an incremental state reset.
  void UpdateIncrementalStateIfNeeded(
      base::trace_event::TraceEvent* trace_event);

  // Fills in all the fields in |trace_packet| that can be directly deduced from
  // |trace_event|. Also fills all updates needed to be emitted into the
  // |InternedData| field into |pending_interning_updates_|. Returns a pointer
  // to the prepared TrackEvent proto, on which the caller may set further
  // fields.
  perfetto::protos::pbzero::TrackEvent* PrepareTrackEvent(
      base::trace_event::TraceEvent* trace_event,
      base::trace_event::TraceEventHandle* handle,
      protozero::MessageHandle<perfetto::protos::pbzero::TracePacket>*
          trace_packet);

  void EmitThreadTrackDescriptor(base::trace_event::TraceEvent* trace_event,
                                 base::TimeTicks timestamp,
                                 const char* maybe_new_name = nullptr);
  void EmitCounterTrackDescriptor(
      base::TimeTicks timestamp,
      uint64_t thread_track_uuid,
      uint64_t counter_track_uuid_bit,
      perfetto::protos::pbzero::CounterDescriptor::BuiltinCounterType
          counter_type,
      uint64_t unit_multiplier = 0u);
  void DoResetIncrementalState(base::trace_event::TraceEvent* trace_event,
                               bool explicit_timestamp);
  void SetPacketTimestamp(
      protozero::MessageHandle<perfetto::protos::pbzero::TracePacket>*
          trace_packet,
      base::TimeTicks timestamp,
      bool force_absolute_timestamp = false);

  // Add a copy of this string to |copied_strings_| and return a pointer which
  // is valid until |copied_strings_| are cleared.
  const char* CopyString(const std::string& value);

  // Write interned data (both from |incremental_state_| and interned indexes
  // into the given packet.
  void WriteInternedDataIntoTracePacket(
      perfetto::protos::pbzero::TracePacket* packet);

  // Track event interning state.
  perfetto::internal::TrackEventIncrementalState incremental_state_;

  std::vector<uint64_t> extra_emitted_track_descriptor_uuids_;

  static std::atomic<uint32_t> incremental_state_reset_id_;

  bool reset_incremental_state_ = true;
  uint32_t last_incremental_state_reset_id_ = 0;
  base::TimeTicks last_timestamp_;
  base::ThreadTicks last_thread_time_;
  base::ProcessId process_id_;
  base::PlatformThreadId thread_id_;
  std::string thread_name_;
  perfetto::protos::pbzero::ChromeThreadDescriptor::ThreadType thread_type_ =
      perfetto::protos::pbzero::ChromeThreadDescriptor::THREAD_UNSPECIFIED;

  const bool privacy_filtering_enabled_;

  std::unique_ptr<perfetto::TraceWriter> trace_writer_;
  uint32_t session_id_;
  bool disable_interning_;
  uint32_t sink_id_;
  bool last_packet_was_empty_ = true;

  // Stores the trace packet handle for a typed TrackEvent until the TrackEvent
  // was finalized after the code in //base filled its typed argument fields.
  perfetto::TraceWriter::TracePacketHandle pending_trace_packet_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_EVENT_THREAD_LOCAL_EVENT_SINK_H_
