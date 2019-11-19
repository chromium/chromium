// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_TRACK_EVENT_JSON_EXPORTER_H_
#define SERVICES_TRACING_PERFETTO_TRACK_EVENT_JSON_EXPORTER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "services/tracing/perfetto/json_trace_exporter.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_packet.pb.h"

namespace perfetto {
namespace protos {
class ChromeTracePacket;
class DebugAnnotation;
class TaskExecution;
class TrackEvent;
class TrackEvent_LegacyEvent;
class StreamingProfilePacket;
}  // namespace protos
}  // namespace perfetto

namespace tracing {

class TrackEventJSONExporter : public JSONTraceExporter {
 public:
  TrackEventJSONExporter(ArgumentFilterPredicate argument_filter_predicate,
                         MetadataFilterPredicate metadata_filter_predicate,
                         OnTraceEventJSONCallback callback);

  ~TrackEventJSONExporter() override;

 protected:
  void ProcessPackets(const std::vector<perfetto::TracePacket>& packets,
                      bool has_more) override;

 private:
  struct ProducerWriterState {
    explicit ProducerWriterState(uint32_t sequence_id);
    ProducerWriterState(uint32_t sequence_id,
                        bool emitted_process,
                        std::unique_ptr<perfetto::protos::ThreadDescriptor>
                            last_seen_thread_descriptor,
                        bool incomplete);
    ~ProducerWriterState();

    // 0 is an invalid sequence_id.
    uint32_t trusted_packet_sequence_id = 0;

    int32_t pid = -1;
    int32_t process_priority = -1;
    int32_t tid = -1;
    int64_t time_us = -1;
    int64_t thread_time_us = -1;
    int64_t thread_instruction_count = -1;

    // We only want to add metadata events about the process or threads once.
    // This is to prevent duplicate events in the json since the packets
    // containing this data are periodically emitted and so would occur
    // frequently if not suppressed.
    bool emitted_process_metadata = false;
    std::unique_ptr<perfetto::protos::ThreadDescriptor>
        last_seen_thread_descriptor;

    // Until we see a TracePacket that will initialize our state we will skip
    // all data besides stateful information. Once we've been reset on the same
    // sequence or started a new sequence this will become false and we will
    // start emitting events again.
    bool incomplete = true;

    std::unordered_map<uint32_t, std::string> interned_event_categories_;
    std::unordered_map<uint32_t, std::pair<std::string, std::string>>
        interned_source_locations_;
    std::unordered_map<uint32_t, std::string> interned_event_names_;
    std::unordered_map<uint32_t, std::string> interned_debug_annotation_names_;

    struct Frame {
      bool has_rel_pc;
      uint64_t rel_pc;
      uint32_t function_name_id;
      uint32_t mapping_id;
    };
    std::unordered_map<uint32_t, Frame> interned_frames_;
    std::unordered_map<uint32_t, std::string> interned_module_names_;
    std::unordered_map<uint32_t, std::string> interned_module_ids_;
    struct Mapping {
      uint32_t build_id;
      uint32_t name_id;
    };
    std::unordered_map<uint32_t, Mapping> interned_mappings_;
    std::unordered_map<uint32_t, std::vector<uint32_t>> interned_callstacks_;

    DISALLOW_COPY_AND_ASSIGN(ProducerWriterState);
  };

  // Some sequence data can appear out of order with the rest, like
  // symbolization data which is prepended to the rest of the trace data.
  // We need to keep this around even when the ProducerWriterState gets
  // swapped out.
  struct UnorderedProducerWriterState {
    UnorderedProducerWriterState();
    ~UnorderedProducerWriterState();

    std::unordered_map<uint32_t, uint32_t> interned_profiled_frame_;
    std::unordered_map<uint32_t, std::string> interned_frame_names_;

    DISALLOW_COPY_AND_ASSIGN(UnorderedProducerWriterState);
  };

  // Packet sequences are given in order so when we encounter a new one we need
  // to reset all the interned state and per sequence info.
  void StartNewState(uint32_t trusted_packet_sequence_id, bool state_cleared);
  // When we encounter a request to reset our incremental state this will clear
  // out the |current_state_| leaving only the required persistent data (like
  // |emitted_process_metadata|) the same.
  void ResetIncrementalState();

  // Given our |current_state_| and an |event| we determine the timestamp (or
  // thread timestamp) we should output to the json.
  int64_t ComputeTimeUs(const perfetto::protos::TrackEvent& event);
  base::Optional<int64_t> ComputeThreadTimeUs(
      const perfetto::protos::TrackEvent& event);
  base::Optional<int64_t> ComputeThreadInstructionCount(
      const perfetto::protos::TrackEvent& event);

  // Gather all the interned strings of different types.
  void HandleInternedData(const perfetto::protos::ChromeTracePacket& packet);

  // New typed messages that are part of the oneof in TracePacket.
  void HandleProcessDescriptor(
      const perfetto::protos::ChromeTracePacket& packet);
  void HandleThreadDescriptor(
      const perfetto::protos::ChromeTracePacket& packet);
  void EmitThreadDescriptorIfNeeded();
  void HandleChromeEvents(const perfetto::protos::ChromeTracePacket& packet);
  void HandleTrackEvent(const perfetto::protos::ChromeTracePacket& packet);
  void HandleStreamingProfilePacket(
      const perfetto::protos::StreamingProfilePacket& profile_packet);
  void HandleProfiledFrameSymbols(
      const perfetto::protos::ProfiledFrameSymbols& frame_symbols);

  // New typed args handlers go here. Used inside HandleTrackEvent to process
  // args.
  void HandleDebugAnnotation(
      const perfetto::protos::DebugAnnotation& debug_annotation,
      ArgumentBuilder* args_builder);
  void HandleTaskExecution(const perfetto::protos::TaskExecution& task,
                           ArgumentBuilder* args_builder);

  // Used to handle the LegacyEvent message found inside the TrackEvent proto.
  ScopedJSONTraceEventAppender HandleLegacyEvent(
      const perfetto::protos::TrackEvent_LegacyEvent& event,
      const std::string& categories,
      int64_t timestamp_us);

  void EmitStats();

  // Tracks all the interned state in the current sequence.
  std::unique_ptr<ProducerWriterState> current_state_;

  // Tracks out-of-order seqeuence data.
  std::map<uint32_t, UnorderedProducerWriterState> unordered_state_data_;

  struct Stats {
    int sequences_seen = 0;
    int incremental_state_resets = 0;
    int packets_dropped_invalid_incremental_state = 0;
    int packets_with_previous_packet_dropped = 0;
  };
  Stats stats_;

  DISALLOW_COPY_AND_ASSIGN(TrackEventJSONExporter);
};

}  // namespace tracing
#endif  // SERVICES_TRACING_PERFETTO_TRACK_EVENT_JSON_EXPORTER_H_
