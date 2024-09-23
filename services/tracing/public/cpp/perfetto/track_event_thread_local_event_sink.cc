// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/track_event_thread_local_event_sink.h"

#include <algorithm>
#include <atomic>
#include <string_view>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/log_message.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/traced_value.h"
#include "base/tracing/trace_time.h"
#include "base/tracing/tracing_tls.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/perfetto/trace_string_lookup.h"
#include "services/tracing/public/cpp/perfetto/traced_value_proto_writer.h"
#include "third_party/perfetto/include/perfetto/tracing/internal/track_event_interned_fields.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_interned_data_index.h"
#include "third_party/perfetto/protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet_defaults.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/task_execution.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.pbzero.h"

using TraceLog = base::trace_event::TraceLog;
using perfetto::protos::pbzero::ChromeThreadDescriptor;
using perfetto::protos::pbzero::ClockSnapshot;
using perfetto::protos::pbzero::CounterDescriptor;
using perfetto::protos::pbzero::DebugAnnotation;
using perfetto::protos::pbzero::InternedData;
using perfetto::protos::pbzero::ThreadDescriptor;
using perfetto::protos::pbzero::TracePacket;
using perfetto::protos::pbzero::TracePacketDefaults;
using perfetto::protos::pbzero::TrackDescriptor;
using perfetto::protos::pbzero::TrackEvent;
using perfetto::protos::pbzero::TrackEventDefaults;

namespace tracing {

namespace {

// Replacement string for names of events with TRACE_EVENT_FLAG_COPY.
const char* const kPrivacyFiltered = "PRIVACY_FILTERED";

// Packet-sequence-scoped clocks that encode timestamps in microseconds in
// the kTraceClockId clock domain.
constexpr uint32_t kClockIdAbsolute = 64;
constexpr uint32_t kClockIdIncremental = 65;

// Bits xor-ed into the track uuid of a thread track to make the track uuid of
// a thread time track. These bits are chosen from the
// upper end of the uint64_t bytes, because the tid of the thread is hashed
// into the least significant 32 bits of the uuid.
constexpr uint64_t kThreadTimeTrackUuidBit = static_cast<uint64_t>(1u) << 32;
constexpr uint64_t kAbsoluteThreadTimeTrackUuidBit = static_cast<uint64_t>(1u)
                                                     << 33;

void AddConvertableToTraceFormat(
    base::trace_event::ConvertableToTraceFormat* value,
    perfetto::protos::pbzero::DebugAnnotation* annotation) {
  PerfettoProtoAppender proto_appender(annotation);
  if (value->AppendToProto(&proto_appender)) {
    return;
  }

  std::string json;
  value->AppendAsTraceFormat(&json);
  annotation->set_legacy_json_value(json.c_str());
}

void WriteDebugAnnotationValue(base::trace_event::TraceEvent* trace_event,
                               size_t arg_index,
                               DebugAnnotation* annotation) {
  auto type = trace_event->arg_type(arg_index);

  if (type == TRACE_VALUE_TYPE_CONVERTABLE) {
    AddConvertableToTraceFormat(trace_event->arg_convertible_value(arg_index),
                                annotation);
    return;
  }

  auto& value = trace_event->arg_value(arg_index);
  switch (type) {
    case TRACE_VALUE_TYPE_BOOL:
      annotation->set_bool_value(value.as_bool);
      break;
    case TRACE_VALUE_TYPE_UINT:
      annotation->set_uint_value(value.as_uint);
      break;
    case TRACE_VALUE_TYPE_INT:
      annotation->set_int_value(value.as_int);
      break;
    case TRACE_VALUE_TYPE_DOUBLE:
      annotation->set_double_value(value.as_double);
      break;
    case TRACE_VALUE_TYPE_POINTER:
      annotation->set_pointer_value(
          static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value.as_pointer)));
      break;
    case TRACE_VALUE_TYPE_STRING:
    case TRACE_VALUE_TYPE_COPY_STRING:
      annotation->set_string_value(value.as_string ? value.as_string : "NULL");
      break;
    case TRACE_VALUE_TYPE_PROTO: {
      auto data = value.as_proto->SerializeAsArray();
      annotation->AppendRawProtoBytes(data.data(), data.size());
    } break;

    default:
      NOTREACHED_IN_MIGRATION() << "Don't know how to serialize this value";
      break;
  }
}

// Lazily sets |legacy_event| on the |track_event|. Note that you should not set
// any other fields of |track_event| (outside the LegacyEvent) between any calls
// to GetOrCreate(), as the protozero serialization requires the writes to a
// message's fields to be consecutive.
class LazyLegacyEventInitializer {
 public:
  explicit LazyLegacyEventInitializer(TrackEvent* track_event)
      : track_event_(track_event) {}

  TrackEvent::LegacyEvent* GetOrCreate() {
    if (!legacy_event_) {
      legacy_event_ = track_event_->set_legacy_event();
    }
    return legacy_event_;
  }

 private:
  // `track_event_` and `legacy_event_` are not a raw_ptr<...> for performance
  // reasons (based on analysis of sampling profiler data and
  // tab_search:top100:2020).
  RAW_PTR_EXCLUSION TrackEvent* track_event_;
  RAW_PTR_EXCLUSION TrackEvent::LegacyEvent* legacy_event_ = nullptr;
};

}  // namespace

// static
constexpr size_t TrackEventThreadLocalEventSink::kMaxCompleteEventDepth;

// static
std::atomic<uint32_t>
    TrackEventThreadLocalEventSink::incremental_state_reset_id_{0};

TrackEventThreadLocalEventSink::TrackEventThreadLocalEventSink(
    std::unique_ptr<perfetto::TraceWriter> trace_writer,
    uint32_t session_id,
    bool disable_interning,
    bool proto_writer_filtering_enabled)
    : process_id_(TraceLog::GetInstance()->process_id()),
      thread_id_(base::PlatformThread::CurrentId()),
      privacy_filtering_enabled_(proto_writer_filtering_enabled),
      trace_writer_(std::move(trace_writer)),
      session_id_(session_id),
      disable_interning_(disable_interning) {
  static std::atomic<uint32_t> g_sink_id_counter{0};
  sink_id_ = ++g_sink_id_counter;
  base::ThreadIdNameManager::GetInstance()->AddObserver(this);
}

TrackEventThreadLocalEventSink::~TrackEventThreadLocalEventSink() {
  base::ThreadIdNameManager::GetInstance()->RemoveObserver(this);

  // We've already destroyed all message handles at this point.
  TraceEventDataSource::GetInstance()->ReturnTraceWriter(
      std::move(trace_writer_));
}

// static
void TrackEventThreadLocalEventSink::ClearIncrementalState() {
  incremental_state_reset_id_.fetch_add(1u, std::memory_order_relaxed);
}

perfetto::TraceWriter::TracePacketHandle
TrackEventThreadLocalEventSink::NewTracePacket(PacketType packet_type) {
  last_packet_was_empty_ = packet_type == PacketType::kEmpty;
  return trace_writer_->NewTracePacket();
}

void TrackEventThreadLocalEventSink::AddLegacyTraceEvent(
    base::trace_event::TraceEvent* trace_event,
    base::trace_event::TraceEventHandle* handle) {
  DCHECK(!pending_trace_packet_);
  UpdateIncrementalStateIfNeeded(trace_event);

  auto trace_packet = NewTracePacket();
  PrepareTrackEvent(trace_event, handle, &trace_packet);

  WriteInternedDataIntoTracePacket(trace_packet.get());
}

base::trace_event::TrackEventHandle
TrackEventThreadLocalEventSink::AddTypedTraceEvent(
    base::trace_event::TraceEvent* trace_event) {
  DCHECK(!*base::tracing::GetThreadIsInTraceEvent());
  // Cleared in OnTrackEventCompleted().
  *base::tracing::GetThreadIsInTraceEvent() = true;

  DCHECK(!pending_trace_packet_);
  UpdateIncrementalStateIfNeeded(trace_event);

  pending_trace_packet_ = NewTracePacket();

  // Note: Since |track_event| is a protozero message under |trace_packet|, we
  // can't modify |trace_packet| further until we're done with |track_event|.
  // Thus, incremental state is buffered until the TrackEventHandle we return
  // here is destroyed.
  base::trace_event::TraceEventHandle base_handle{0, 0, 0};
  auto* track_event =
      PrepareTrackEvent(trace_event, &base_handle, &pending_trace_packet_);

  // |pending_trace_packet_| will be finalized in OnTrackEventCompleted() after
  // the code in //base ran the typed trace point's argument function.
  return base::trace_event::TrackEventHandle(track_event, &incremental_state_,
                                             this, privacy_filtering_enabled_);
}

void TrackEventThreadLocalEventSink::WriteInternedDataIntoTracePacket(
    TracePacket* packet) {
  auto& serialized_interned_data = incremental_state_.serialized_interned_data;

  // When the track event is finalized (i.e., the context is destroyed), we
  // should flush any newly seen interned data to the trace. The data has
  // earlier been written to a heap allocated protobuf message
  // (|serialized_interned_data|). Here we just need to flush it to the main
  // trace.
  if (!serialized_interned_data.empty()) {
    auto ranges = serialized_interned_data.GetRanges();
    packet->AppendScatteredBytes(
        perfetto::protos::pbzero::TracePacket::kInternedDataFieldNumber,
        &ranges[0], ranges.size());

    // Reset the message but keep one buffer allocated for future use.
    serialized_interned_data.Reset();
  }
}

void TrackEventThreadLocalEventSink::OnTrackEventCompleted() {
  DCHECK(pending_trace_packet_);

  WriteInternedDataIntoTracePacket(pending_trace_packet_.get());
  pending_trace_packet_ = perfetto::TraceWriter::TracePacketHandle();

  DCHECK(*base::tracing::GetThreadIsInTraceEvent());
  *base::tracing::GetThreadIsInTraceEvent() = false;
}

base::trace_event::TracePacketHandle
TrackEventThreadLocalEventSink::AddTracePacket() {
  DCHECK(!*base::tracing::GetThreadIsInTraceEvent());
  // Cleared in OnTracePacketCompleted().
  *base::tracing::GetThreadIsInTraceEvent() = true;

  DCHECK(!pending_trace_packet_);

  perfetto::TraceWriter::TracePacketHandle packet = NewTracePacket();
  // base doesn't require accurate timestamps in these packets, so we just emit
  // the packet with the last timestamp we used.
  SetPacketTimestamp(&packet, last_timestamp_);

  return base::trace_event::TracePacketHandle(std::move(packet), this);
}

void TrackEventThreadLocalEventSink::AddEmptyPacket() {
  // Only add a new empty packet if there's at least one non-empty packet in the
  // current chunk. Otherwise, there's nothing to flush, so adding more empty
  // packets serves no purpose.
  if (last_packet_was_empty_)
    return;

  const base::AutoReset<bool> resetter(base::tracing::GetThreadIsInTraceEvent(),
                                       true, false);
  DCHECK(!pending_trace_packet_);
  NewTracePacket(PacketType::kEmpty);
}

void TrackEventThreadLocalEventSink::OnTracePacketCompleted() {
  DCHECK(*base::tracing::GetThreadIsInTraceEvent());
  *base::tracing::GetThreadIsInTraceEvent() = false;
}

void TrackEventThreadLocalEventSink::UpdateIncrementalStateIfNeeded(
    base::trace_event::TraceEvent* trace_event) {
  bool explicit_timestamp =
      trace_event->flags() & TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP;
  bool is_for_different_process =
      (trace_event->flags() & TRACE_EVENT_FLAG_HAS_PROCESS_ID) &&
      trace_event->process_id() != base::kNullProcessId;
  bool is_for_different_thread = thread_id_ != trace_event->thread_id();

  // We access |incremental_state_reset_id_| atomically but racily. It's OK if
  // we don't notice the reset request immediately, as long as we will notice
  // and service it eventually.
  auto reset_id = incremental_state_reset_id_.load(std::memory_order_relaxed);
  if (reset_id != last_incremental_state_reset_id_) {
    reset_incremental_state_ = true;
    last_incremental_state_reset_id_ = reset_id;
  }

  if (reset_incremental_state_) {
    DoResetIncrementalState(trace_event, explicit_timestamp);
  }

  // Ensure that track descriptors for another thread that we reference are
  // emitted. The other thread may not emit its own descriptors, e.g. if it's an
  // early java thread that already died. We can only do this for threads in the
  // same process. Some metadata events also set thread_id to 0, which doesn't
  // correspond to a valid thread, so we skip these here.
  if (!is_for_different_process && is_for_different_thread &&
      trace_event->thread_id() != 0) {
    // If we haven't yet, emit thread track descriptor and mark it as emitted.
    // This descriptor is compatible with the thread track descriptor that may
    // be emitted by the other thread itself, but doesn't set thread details
    // (e.g. thread name or type).
    perfetto::ThreadTrack thread_track =
        perfetto::ThreadTrack::ForThread(trace_event->thread_id());
    if (!base::Contains(extra_emitted_track_descriptor_uuids_,
                        thread_track.uuid)) {
      auto packet = NewTracePacket();
      SetPacketTimestamp(&packet, last_timestamp_);
      TrackDescriptor* track_descriptor = packet->set_track_descriptor();
      // TODO(eseckler): Call thread_track.Serialize() here instead once the
      // gets the correct pid from Chrome.
      track_descriptor->set_uuid(thread_track.uuid);
      DCHECK(thread_track.parent_uuid);
      track_descriptor->set_parent_uuid(thread_track.parent_uuid);
      // Instructs Trace Processor not to merge track events and system events
      // track for this thread.
      // TODO(kraskevich): Figure out how to do this for the Perfetto SDK
      // version.
      track_descriptor->set_disallow_merging_with_system_tracks(true);
      ThreadDescriptor* thread = track_descriptor->set_thread();
      thread->set_pid(process_id_);
      thread->set_tid(trace_event->thread_id());

      extra_emitted_track_descriptor_uuids_.push_back(thread_track.uuid);
    }

    bool has_thread_time = !trace_event->thread_timestamp().is_null();
    if (has_thread_time) {
      // If we haven't yet, emit an absolute thread time counter track
      // descriptor and mark it as emitted. We can't use the thread's own thread
      // time counter track, because its delta encoding is not valid on this
      // trace writer sequence.
      uint64_t thread_time_track_uuid =
          thread_track.uuid ^ kAbsoluteThreadTimeTrackUuidBit;
      if (!base::Contains(extra_emitted_track_descriptor_uuids_,
                          thread_time_track_uuid)) {
        auto packet = NewTracePacket();
        SetPacketTimestamp(&packet, last_timestamp_);
        TrackDescriptor* track_descriptor = packet->set_track_descriptor();
        // TODO(eseckler): Switch to client library support for CounterTrack
        // uuid calculation once available.
        track_descriptor->set_uuid(thread_time_track_uuid);
        track_descriptor->set_parent_uuid(thread_track.uuid);
        CounterDescriptor* counter = track_descriptor->set_counter();
        counter->set_type(CounterDescriptor::COUNTER_THREAD_TIME_NS);
        // Absolute values, but in microseconds.
        counter->set_unit_multiplier(1000u);

        extra_emitted_track_descriptor_uuids_.push_back(thread_time_track_uuid);
      }
    }
  }
}

TrackEvent* TrackEventThreadLocalEventSink::PrepareTrackEvent(
    base::trace_event::TraceEvent* trace_event,
    base::trace_event::TraceEventHandle* handle,
    protozero::MessageHandle<TracePacket>* trace_packet) {
  // Each event's updates to InternedData are flushed at the end of
  // AddTraceEvent().
  DCHECK(incremental_state_.serialized_interned_data.empty());

  char phase = trace_event->phase();

  // Split COMPLETE events into BEGIN/END pairs. We write the BEGIN here, and
  // the END in UpdateDuration().
  if (phase == TRACE_EVENT_PHASE_COMPLETE) {
    phase = TRACE_EVENT_PHASE_BEGIN;
  }

  bool is_sync_end = phase == TRACE_EVENT_PHASE_END;
  bool is_nestable_async_end = phase == TRACE_EVENT_PHASE_NESTABLE_ASYNC_END;

  uint32_t flags = trace_event->flags();
  bool copy_strings = flags & TRACE_EVENT_FLAG_COPY;
  bool is_java_event = flags & TRACE_EVENT_FLAG_JAVA_STRING_LITERALS;
  bool explicit_timestamp = flags & TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP;

  // Delta encoded timestamps and interned data require incremental state.
  (*trace_packet)->set_sequence_flags(TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);

  // Events for different processes/threads always use an absolute timestamp.
  bool is_for_different_process =
      (flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID) &&
      trace_event->process_id() != base::kNullProcessId;
  bool is_for_different_thread = thread_id_ != trace_event->thread_id();
  bool force_absolute_timestamp =
      is_for_different_process || is_for_different_thread || explicit_timestamp;

  SetPacketTimestamp(trace_packet, trace_event->timestamp(),
                     force_absolute_timestamp);

  TrackEvent* track_event = (*trace_packet)->set_track_event();
  perfetto::EventContext event_context(track_event, &incremental_state_,
                                       privacy_filtering_enabled_);

  // TODO(eseckler): Split comma-separated category strings.
  const char* category_name =
      TraceLog::GetCategoryGroupName(trace_event->category_group_enabled());
  // No need to write the category for sync end events. Trace processor will
  // match them without, provided event nesting is correct. For async end
  // events, event ID matching includes the category, so we have to emit the
  // category for the time being.
  if (!is_sync_end) {
    track_event->add_category_iids(
        perfetto::internal::InternedEventCategory::Get(
            &event_context, category_name, strlen(category_name)));
  }
  // No need to write the event name for end events (sync or nestable async).
  // Trace processor will match them without, provided event nesting is correct.
  // Legacy async events (TRACE_EVENT_ASYNC*) are only pass-through in trace
  // processor, so we still have to emit names for these.
  const char* trace_event_name = trace_event->name();
  if (!is_sync_end && !is_nestable_async_end && trace_event_name != nullptr) {
    bool filter_name =
        copy_strings && !is_java_event && privacy_filtering_enabled_;
    if (filter_name)
      trace_event_name = kPrivacyFiltered;
    if (copy_strings) {
      track_event->set_name(trace_event_name);
    } else {
      track_event->set_name_iid(perfetto::internal::InternedEventName::Get(
          &event_context, trace_event->name()));
    }
  }

  if (!privacy_filtering_enabled_) {
    for (size_t i = 0; i < trace_event->arg_size() && trace_event->arg_name(i);
         ++i) {
      auto* debug_annotation = track_event->add_debug_annotations();
      if (copy_strings) {
        debug_annotation->set_name(trace_event->arg_name(i));
      } else {
        debug_annotation->set_name_iid(
            perfetto::internal::InternedDebugAnnotationName::Get(
                &event_context, trace_event->arg_name(i)));
      }
      WriteDebugAnnotationValue(trace_event, i, debug_annotation);
    }
  }

  if (!trace_event->thread_timestamp().is_null()) {
    if (is_for_different_thread) {
      // EarlyJava events are emitted on the main thread but may actually be for
      // different threads and specify their thread time.

      // EarlyJava events are always for the same process.
      DCHECK(!is_for_different_process);

      // Emit a value onto the absolute thread time track for the other thread.
      // We emit a descriptor for this in UpdateIncrementalStateIfNeeded().
      uint64_t track_uuid =
          perfetto::ThreadTrack::ForThread(trace_event->thread_id()).uuid ^
          kAbsoluteThreadTimeTrackUuidBit;
      DCHECK(base::Contains(extra_emitted_track_descriptor_uuids_, track_uuid));

      track_event->add_extra_counter_values(
          (trace_event->thread_timestamp() - base::ThreadTicks())
              .InMicroseconds());
      track_event->add_extra_counter_track_uuids(track_uuid);
    } else {
      // Thread timestamps for the current thread are never user-provided.
      // While OSes don't guarantee they are monotonic, the discrepancies are
      // usually quite rare and quite small.
      track_event->add_extra_counter_values(
          (trace_event->thread_timestamp() - last_thread_time_)
              .InMicroseconds());
      last_thread_time_ = trace_event->thread_timestamp();
    }
  }

  // Only set the |legacy_event| field of the TrackEvent if we need to emit any
  // of the legacy fields. BEWARE: Do not set any other TrackEvent fields in
  // between calls to |legacy_event.GetOrCreate()|.
  LazyLegacyEventInitializer legacy_event(track_event);

  // TODO(eseckler): Also convert async / flow events to corresponding native
  // TrackEvent types. Instants & asyncs require using track descriptors rather
  // than instant event scopes / async event IDs.
  TrackEvent::Type track_event_type = TrackEvent::TYPE_UNSPECIFIED;
  switch (phase) {
    case TRACE_EVENT_PHASE_BEGIN:
      track_event_type = TrackEvent::TYPE_SLICE_BEGIN;
      break;
    case TRACE_EVENT_PHASE_END:
      track_event_type = TrackEvent::TYPE_SLICE_END;
      break;
    case TRACE_EVENT_PHASE_INSTANT:
      track_event_type = TrackEvent::TYPE_INSTANT;
      break;
    default:
      break;
  }

  if (track_event_type != TrackEvent::TYPE_UNSPECIFIED) {
    // We emit these events using TrackDescriptors, and we cannot emit events on
    // behalf of other processes using the TrackDescriptor format. Chrome
    // currently only emits PHASE_MEMORY_DUMP events with an explicit process
    // id, so we should be fine here.
    // TODO(eseckler): Get rid of events with explicit process ids entirely.
    DCHECK(!(flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID));

    track_event->set_type(track_event_type);

    if (track_event_type == TrackEvent::TYPE_INSTANT) {
      switch (flags & TRACE_EVENT_FLAG_SCOPE_MASK) {
        case TRACE_EVENT_SCOPE_GLOBAL: {
          track_event->set_track_uuid(0);  // Global track.
          break;
        }
        case TRACE_EVENT_SCOPE_PROCESS: {
          track_event->set_track_uuid(perfetto::ProcessTrack::Current().uuid);
          break;
        }
        case TRACE_EVENT_SCOPE_THREAD: {
          if (thread_id_ != trace_event->thread_id()) {
            uint64_t track_uuid =
                perfetto::ThreadTrack::ForThread(trace_event->thread_id()).uuid;
            DCHECK(base::Contains(extra_emitted_track_descriptor_uuids_,
                                  track_uuid));
            track_event->set_track_uuid(track_uuid);
          } else {
            // Default to the thread track.
          }
          break;
        }
      }
    } else {
      if (thread_id_ != trace_event->thread_id()) {
        uint64_t track_uuid =
            perfetto::ThreadTrack::ForThread(trace_event->thread_id()).uuid;
        DCHECK(
            base::Contains(extra_emitted_track_descriptor_uuids_, track_uuid));
        track_event->set_track_uuid(track_uuid);
      } else {
        // Default to the thread track.
      }
    }
  } else {
    // Explicitly clear the track, so that the event is not associated with the
    // default track, but instead uses the legacy mechanism based on the phase
    // and pid/tid override.
    track_event->set_track_uuid(0);

    legacy_event.GetOrCreate()->set_phase(phase);

    if ((flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID) &&
        trace_event->process_id() != base::kNullProcessId) {
      legacy_event.GetOrCreate()->set_pid_override(trace_event->process_id());
      legacy_event.GetOrCreate()->set_tid_override(-1);
    } else if (thread_id_ != trace_event->thread_id()) {
      // Some metadata events set thread_id to 0. We avoid setting tid_override
      // to 0 to avoid clashes with the swapper thread in system traces
      // (b/215725684).
      if (trace_event->thread_id() != 0)
        legacy_event.GetOrCreate()->set_tid_override(trace_event->thread_id());
    }
  }

  uint32_t id_flags =
      flags & (TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_HAS_LOCAL_ID |
               TRACE_EVENT_FLAG_HAS_GLOBAL_ID);
  uint32_t flow_flags =
      flags & (TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN);

  uint64_t id = trace_event->id();
  if (id_flags && trace_event->scope() != trace_event_internal::kGlobalScope) {
    // The scope string might be privacy filtered, so also hash it with the
    // id.
    id = base::HashInts(base::FastHash(trace_event->scope()), id);
  }

  // Legacy flow events use bind_id as their (unscoped) identifier. There's no
  // need to also emit id in that case.
  if (!flow_flags) {
    switch (id_flags) {
      case TRACE_EVENT_FLAG_HAS_ID:
        legacy_event.GetOrCreate()->set_unscoped_id(id);
        break;
      case TRACE_EVENT_FLAG_HAS_LOCAL_ID:
        legacy_event.GetOrCreate()->set_local_id(id);
        break;
      case TRACE_EVENT_FLAG_HAS_GLOBAL_ID:
        legacy_event.GetOrCreate()->set_global_id(id);
        break;
      default:
        break;
    }
  }

  if (flags & TRACE_EVENT_FLAG_ASYNC_TTS) {
    legacy_event.GetOrCreate()->set_use_async_tts(true);
  }

  switch (flow_flags) {
    case TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN:
      legacy_event.GetOrCreate()->set_flow_direction(
          TrackEvent::LegacyEvent::FLOW_INOUT);
      break;
    case TRACE_EVENT_FLAG_FLOW_OUT:
      legacy_event.GetOrCreate()->set_flow_direction(
          TrackEvent::LegacyEvent::FLOW_OUT);
      break;
    case TRACE_EVENT_FLAG_FLOW_IN:
      legacy_event.GetOrCreate()->set_flow_direction(
          TrackEvent::LegacyEvent::FLOW_IN);
      break;
    default:
      break;
  }

  if (flow_flags) {
    legacy_event.GetOrCreate()->set_bind_id(trace_event->bind_id());
  }

  if (flags & TRACE_EVENT_FLAG_BIND_TO_ENCLOSING) {
    legacy_event.GetOrCreate()->set_bind_to_enclosing(true);
  }

  if (disable_interning_) {
    incremental_state_.interned_data_indices = {};
  }

  return track_event;
}

void TrackEventThreadLocalEventSink::UpdateDuration(
    const unsigned char* category_group_enabled,
    const char* name,
    base::trace_event::TraceEventHandle handle,
    int thread_id,
    bool explicit_timestamps,
    const base::TimeTicks& now,
    const base::ThreadTicks& thread_now) {
  base::trace_event::TraceEvent new_trace_event(
      thread_id, now, thread_now, TRACE_EVENT_PHASE_END, category_group_enabled,
      name, trace_event_internal::kGlobalScope,
      trace_event_internal::kNoId /* id */,
      trace_event_internal::kNoId /* bind_id */, nullptr,
      explicit_timestamps ? TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP
                          : TRACE_EVENT_FLAG_NONE);
  AddLegacyTraceEvent(&new_trace_event, nullptr);
}

void TrackEventThreadLocalEventSink::Flush() {
  trace_writer_->Flush();
}

void TrackEventThreadLocalEventSink::OnThreadNameChanged(const char* name) {
  if (thread_id_ != base::PlatformThread::CurrentId())
    return;
  EmitThreadTrackDescriptor(nullptr, TRACE_TIME_TICKS_NOW(), name);
}

void TrackEventThreadLocalEventSink::EmitThreadTrackDescriptor(
    base::trace_event::TraceEvent* trace_event,
    base::TimeTicks timestamp,
    const char* maybe_new_name) {
  auto packet = NewTracePacket();
  SetPacketTimestamp(&packet, timestamp);

  TrackDescriptor* track_descriptor = packet->set_track_descriptor();
  // TODO(eseckler): Call ThreadTrack::Current() instead once the client lib
  // supports Chrome's tids.
  auto thread_track = perfetto::ThreadTrack::ForThread(thread_id_);

  // TODO(eseckler): Call thread_track.Serialize() here instead once the client
  // lib also fills in the ThreadDescriptor's thread_name, gets the correct pid
  // from Chrome, and supports pivacy filtering, and we moved off reference_*
  // fields in ThreadDescriptor.
  track_descriptor->set_uuid(thread_track.uuid);
  DCHECK(thread_track.parent_uuid);
  track_descriptor->set_parent_uuid(thread_track.parent_uuid);
  // Instructs Trace Processor not to merge track events and system events
  // track for this thread.
  // TODO(kraskevich): Figure out how to do this for the Perfetto SDK version.
  track_descriptor->set_disallow_merging_with_system_tracks(true);

  ThreadDescriptor* thread = track_descriptor->set_thread();
  thread->set_pid(process_id_);
  thread->set_tid(thread_id_);

  if (!maybe_new_name) {
    maybe_new_name =
        base::ThreadIdNameManager::GetInstance()->GetNameForCurrentThread();
  }
  if (maybe_new_name && *maybe_new_name &&
      std::string_view(thread_name_) != maybe_new_name) {
    thread_name_ = maybe_new_name;
    thread_type_ = GetThreadType(maybe_new_name);
  }
  if (!privacy_filtering_enabled_) {
    thread->set_thread_name(thread_name_);
  }

  ChromeThreadDescriptor* chrome_thread = track_descriptor->set_chrome_thread();
  chrome_thread->set_thread_type(thread_type_);

  // TODO(eseckler): Fill in remaining fields in ChromeThreadDescriptor.
}

void TrackEventThreadLocalEventSink::EmitCounterTrackDescriptor(
    base::TimeTicks timestamp,
    uint64_t thread_track_uuid,
    uint64_t counter_track_uuid_bit,
    CounterDescriptor::BuiltinCounterType counter_type,
    uint64_t unit_multiplier) {
  auto packet = NewTracePacket();
  SetPacketTimestamp(&packet, timestamp);

  TrackDescriptor* track_descriptor = packet->set_track_descriptor();

  // TODO(eseckler): Switch to client library support for CounterTrack uuid
  // calculation once available.
  uint64_t track_uuid = thread_track_uuid ^ counter_track_uuid_bit;
  track_descriptor->set_uuid(track_uuid);
  track_descriptor->set_parent_uuid(thread_track_uuid);

  CounterDescriptor* counter = track_descriptor->set_counter();
  if (counter_type != CounterDescriptor::COUNTER_UNSPECIFIED) {
    counter->set_type(counter_type);
  }
  if (unit_multiplier) {
    counter->set_unit_multiplier(unit_multiplier);
  }
  counter->set_is_incremental(true);
}

void TrackEventThreadLocalEventSink::DoResetIncrementalState(
    base::trace_event::TraceEvent* trace_event,
    bool explicit_timestamp) {
  extra_emitted_track_descriptor_uuids_.clear();
  incremental_state_.interned_data_indices = {};
  incremental_state_.seen_tracks.clear();

  // Reset the reference timestamp.
  base::TimeTicks timestamp;
  if (explicit_timestamp || !trace_event) {
    timestamp = TRACE_TIME_TICKS_NOW();
  } else {
    timestamp = trace_event->timestamp();
  }
  last_timestamp_ = timestamp;

  // TODO(eseckler): Call ThreadTrack::Current() instead once the client lib
  // supports Chrome's tids.
  uint64_t thread_track_uuid =
      perfetto::ThreadTrack::ForThread(thread_id_).uuid;

  {
    // Emit a new clock snapshot with this timestamp, and also set the
    // |incremental_state_cleared| flag and defaults.
    auto packet = NewTracePacket();
    packet->set_sequence_flags(TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);

    TracePacketDefaults* tp_defaults = packet->set_trace_packet_defaults();
    tp_defaults->set_timestamp_clock_id(kClockIdIncremental);
    TrackEventDefaults* te_defaults = tp_defaults->set_track_event_defaults();

    // Default to thread track, with counter values for thread time.
    te_defaults->set_track_uuid(thread_track_uuid);
    te_defaults->add_extra_counter_track_uuids(thread_track_uuid ^
                                               kThreadTimeTrackUuidBit);

    ClockSnapshot* clocks = packet->set_clock_snapshot();
    // Reference clock is in nanoseconds.
    ClockSnapshot::Clock* clock_reference = clocks->add_clocks();
    clock_reference->set_clock_id(base::tracing::kTraceClockId);
    clock_reference->set_timestamp(timestamp.since_origin().InNanoseconds());
    // Absolute clock in micros.
    ClockSnapshot::Clock* clock_absolute = clocks->add_clocks();
    clock_absolute->set_clock_id(kClockIdAbsolute);
    clock_absolute->set_timestamp(timestamp.since_origin().InMicroseconds());
    clock_absolute->set_unit_multiplier_ns(1000u);
    // Delta-encoded incremental clock in micros.
    ClockSnapshot::Clock* clock_incremental = clocks->add_clocks();
    clock_incremental->set_clock_id(kClockIdIncremental);
    clock_incremental->set_timestamp(timestamp.since_origin().InMicroseconds());
    clock_incremental->set_unit_multiplier_ns(1000u);
    clock_incremental->set_is_incremental(true);
  }

  // Emit new track descriptors for the thread and its counters in separate
  // packets.
  EmitThreadTrackDescriptor(trace_event, timestamp);
  if (base::ThreadTicks::IsSupported()) {
    EmitCounterTrackDescriptor(
        timestamp, thread_track_uuid, kThreadTimeTrackUuidBit,
        CounterDescriptor::COUNTER_THREAD_TIME_NS, 1000u);
  }

  // The first set of counter values should be absolute.
  last_thread_time_ = base::ThreadTicks();

  reset_incremental_state_ = false;
}

void TrackEventThreadLocalEventSink::SetPacketTimestamp(
    protozero::MessageHandle<TracePacket>* trace_packet,
    base::TimeTicks timestamp,
    bool force_absolute_timestamp) {
  if (force_absolute_timestamp || last_timestamp_ > timestamp) {
    (*trace_packet)->set_timestamp(timestamp.since_origin().InMicroseconds());
    (*trace_packet)->set_timestamp_clock_id(kClockIdAbsolute);
    return;
  }

  // Use default timestamp_clock_id (kClockIdIncremental).
  (*trace_packet)
      ->set_timestamp((timestamp - last_timestamp_).InMicroseconds());
  last_timestamp_ = timestamp;
}

}  // namespace tracing
