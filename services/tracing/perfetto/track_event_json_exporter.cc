// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/track_event_json_exporter.h"

#include <cinttypes>
#include <memory>

#include "base/json/string_escape.h"
#include "base/strings/string_util.h"
#include "base/trace_event/common/trace_event_common.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/basic_types.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/sliced_protobuf_input_stream.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_packet.pb.h"

namespace tracing {
namespace {

using ::perfetto::protos::ChromeTracePacket;
using ::perfetto::protos::ThreadDescriptor;
using ::perfetto::protos::TrackEvent;

const std::string& GetInternedName(
    uint32_t iid,
    const std::unordered_map<uint32_t, std::string>& interned) {
  DCHECK(iid);
  auto iter = interned.find(iid);
  DCHECK(iter != interned.end()) << "Missing interned ID: " << iid;
  return iter->second;
}

const char* ThreadTypeToName(ThreadDescriptor::ChromeThreadType type) {
  switch (type) {
    case ThreadDescriptor::CHROME_THREAD_MAIN:
      return "CrProcessMain";
    case ThreadDescriptor::CHROME_THREAD_IO:
      return "ChromeIOThread";
    case ThreadDescriptor::CHROME_THREAD_POOL_FG_WORKER:
      return "ThreadPoolForegroundWorker&";
    case ThreadDescriptor::CHROME_THREAD_POOL_BG_WORKER:
      return "ThreadPoolBackgroundWorker&";
    case ThreadDescriptor::CHROME_THREAD_POOL_FB_BLOCKING:
      return "ThreadPoolSingleThreadForegroundBlocking&";
    case ThreadDescriptor::CHROME_THREAD_POOL_BG_BLOCKING:
      return "ThreadPoolSingleThreadBackgroundBlocking&";
    case ThreadDescriptor::CHROME_THREAD_POOL_SERVICE:
      return "ThreadPoolService";
    case ThreadDescriptor::CHROME_THREAD_COMPOSITOR_WORKER:
      return "CompositorTileWorker&";
    case ThreadDescriptor::CHROME_THREAD_COMPOSITOR:
      return "Compositor";
    case ThreadDescriptor::CHROME_THREAD_VIZ_COMPOSITOR:
      return "VizCompositorThread";
    case ThreadDescriptor::CHROME_THREAD_SERVICE_WORKER:
      return "ServiceWorkerThread&";
    case ThreadDescriptor::CHROME_THREAD_MEMORY_INFRA:
      return "MemoryInfra";
    case ThreadDescriptor::CHROME_THREAD_SAMPLING_PROFILER:
      return "StackSamplingProfiler";

    case ThreadDescriptor::CHROME_THREAD_UNSPECIFIED:
      return nullptr;
  }
}

}  // namespace

TrackEventJSONExporter::TrackEventJSONExporter(
    JSONTraceExporter::ArgumentFilterPredicate argument_filter_predicate,
    JSONTraceExporter::MetadataFilterPredicate metadata_filter_predicate,
    JSONTraceExporter::OnTraceEventJSONCallback callback)
    : JSONTraceExporter(std::move(argument_filter_predicate),
                        std::move(metadata_filter_predicate),
                        std::move(callback)),
      current_state_(std::make_unique<ProducerWriterState>(0)) {}

TrackEventJSONExporter::~TrackEventJSONExporter() {
  DCHECK(!current_state_ || !current_state_->last_seen_thread_descriptor);
}

void TrackEventJSONExporter::ProcessPackets(
    const std::vector<perfetto::TracePacket>& packets,
    bool has_more) {
  for (auto& encoded_packet : packets) {
    // These are perfetto::TracePackets, but ChromeTracePacket is a mirror that
    // reduces binary bloat and only has the fields we are interested in. So
    // Decode the serialized proto as a ChromeTracePacket.
    perfetto::protos::ChromeTracePacket packet;
    ::perfetto::SlicedProtobufInputStream stream(&encoded_packet.slices());
    bool decoded = packet.ParseFromZeroCopyStream(&stream);
    DCHECK(decoded);

    // If this is a different packet_sequence_id we have to reset all our state
    // and wait for the first state_clear before emitting anything. However, we
    // shouldn't do this for packets emitted by the service since they may
    // appear anywhere in the stream.
    if (packet.trusted_packet_sequence_id() !=
        perfetto::kServicePacketSequenceID) {
      if (packet.trusted_packet_sequence_id() !=
          current_state_->trusted_packet_sequence_id) {
        stats_.sequences_seen++;
        StartNewState(packet.trusted_packet_sequence_id(),
                      packet.incremental_state_cleared());
      } else if (packet.incremental_state_cleared()) {
        stats_.incremental_state_resets++;
        ResetIncrementalState();
      } else if (packet.previous_packet_dropped()) {
        // If we've lost packets we can no longer trust any timestamp data and
        // other state which might have been dropped. We will keep skipping
        // events until we start a new sequence.
        stats_.packets_with_previous_packet_dropped++;
        current_state_->incomplete = true;
      }
    } else {
      // We assume the service doesn't use incremental state or lose packets.
      DCHECK(!packet.incremental_state_cleared());
      DCHECK(!packet.previous_packet_dropped());
    }

    // Now we process the data from the packet. First by getting the interned
    // strings out and processed.
    if (packet.has_interned_data()) {
      HandleInternedData(packet);
    }

    // These are all oneof fields below so only one should be true.
    if (packet.has_track_event()) {
      HandleTrackEvent(packet);
    } else if (packet.has_chrome_events()) {
      HandleChromeEvents(packet);
    } else if (packet.has_thread_descriptor()) {
      HandleThreadDescriptor(packet);
    } else if (packet.has_process_descriptor()) {
      HandleProcessDescriptor(packet);
    } else if (packet.has_trace_stats()) {
      SetTraceStatsMetadata(packet.trace_stats());
    } else if (packet.has_streaming_profile_packet()) {
      HandleStreamingProfilePacket(packet.streaming_profile_packet());
    } else if (packet.has_profiled_frame_symbols()) {
      HandleProfiledFrameSymbols(packet.profiled_frame_symbols());
    } else {
      // If none of the above matched, this packet was emitted by the service
      // and has no equivalent in the old trace format. We thus ignore it.
    }
  }
  if (!has_more) {
    EmitThreadDescriptorIfNeeded();
    EmitStats();
  }
}

TrackEventJSONExporter::ProducerWriterState::ProducerWriterState(
    uint32_t sequence_id)
    : ProducerWriterState(sequence_id, false, nullptr, true) {}

TrackEventJSONExporter::ProducerWriterState::ProducerWriterState(
    uint32_t sequence_id,
    bool emitted_process,
    std::unique_ptr<ThreadDescriptor> last_seen_thread_descriptor,
    bool incomplete)
    : trusted_packet_sequence_id(sequence_id),
      emitted_process_metadata(emitted_process),
      last_seen_thread_descriptor(std::move(last_seen_thread_descriptor)),
      incomplete(incomplete) {}

TrackEventJSONExporter::ProducerWriterState::~ProducerWriterState() = default;

TrackEventJSONExporter::UnorderedProducerWriterState::
    UnorderedProducerWriterState() = default;

TrackEventJSONExporter::UnorderedProducerWriterState::
    ~UnorderedProducerWriterState() = default;

void TrackEventJSONExporter::StartNewState(uint32_t trusted_packet_sequence_id,
                                           bool state_cleared) {
  EmitThreadDescriptorIfNeeded();
  current_state_ = std::make_unique<ProducerWriterState>(
      trusted_packet_sequence_id, /* emitted_process = */ false,
      /* last_seen_thread_descriptor = */ nullptr,
      /* incomplete = */ !state_cleared);
}

void TrackEventJSONExporter::ResetIncrementalState() {
  current_state_ = std::make_unique<ProducerWriterState>(
      current_state_->trusted_packet_sequence_id,
      current_state_->emitted_process_metadata,
      std::move(current_state_->last_seen_thread_descriptor),
      /* incomplete = */ false);
}

int64_t TrackEventJSONExporter::ComputeTimeUs(const TrackEvent& event) {
  switch (event.timestamp_case()) {
    case TrackEvent::kTimestampAbsoluteUs:
      return event.timestamp_absolute_us();
    case TrackEvent::kTimestampDeltaUs:
      DCHECK_NE(current_state_->time_us, -1);
      current_state_->time_us += event.timestamp_delta_us();
      return current_state_->time_us;
    case TrackEvent::TIMESTAMP_NOT_SET:
      DLOG(FATAL) << "Event has no timestamp this shouldn't be possible";
      return -1;
  }
}

base::Optional<int64_t> TrackEventJSONExporter::ComputeThreadTimeUs(
    const TrackEvent& event) {
  switch (event.thread_time_case()) {
    case TrackEvent::kThreadTimeAbsoluteUs:
      return event.thread_time_absolute_us();
    case TrackEvent::kThreadTimeDeltaUs:
      DCHECK_NE(current_state_->thread_time_us, -1);
      current_state_->thread_time_us += event.thread_time_delta_us();
      return current_state_->thread_time_us;
    case TrackEvent::THREAD_TIME_NOT_SET:
      return base::nullopt;
  }
}

base::Optional<int64_t> TrackEventJSONExporter::ComputeThreadInstructionCount(
    const TrackEvent& event) {
  switch (event.thread_instruction_count_case()) {
    case TrackEvent::kThreadInstructionCountAbsolute:
      return event.thread_instruction_count_absolute();
    case TrackEvent::kThreadInstructionCountDelta:
      DCHECK_NE(current_state_->thread_instruction_count, -1);
      current_state_->thread_instruction_count +=
          event.thread_instruction_count_delta();
      return current_state_->thread_instruction_count;
    case TrackEvent::THREAD_INSTRUCTION_COUNT_NOT_SET:
      return base::nullopt;
  }
}

void TrackEventJSONExporter::HandleInternedData(
    const ChromeTracePacket& packet) {
  DCHECK(packet.has_interned_data());

  // InternedData is only emitted on sequences with incremental state.
  if (current_state_->incomplete) {
    stats_.packets_dropped_invalid_incremental_state++;
    return;
  }

  const auto& data = packet.interned_data();
  // Even if the interned data was reset we should not change the values in the
  // interned data.
  for (const auto& event_cat : data.event_categories()) {
    auto iter = current_state_->interned_event_categories_.insert(
        std::make_pair(event_cat.iid(), event_cat.name()));
    DCHECK(iter.second || iter.first->second == event_cat.name());
  }
  for (const auto& event_name : data.event_names()) {
    auto iter = current_state_->interned_event_names_.insert(
        std::make_pair(event_name.iid(), event_name.name()));
    DCHECK(iter.second || iter.first->second == event_name.name());
  }
  for (const auto& debug_name : data.debug_annotation_names()) {
    auto iter = current_state_->interned_debug_annotation_names_.insert(
        std::make_pair(debug_name.iid(), debug_name.name()));
    DCHECK(iter.second || iter.first->second == debug_name.name());
  }
  for (const auto& src_loc : data.source_locations()) {
    auto iter = current_state_->interned_source_locations_.insert(
        std::make_pair(src_loc.iid(), std::make_pair(src_loc.file_name(),
                                                     src_loc.function_name())));
    DCHECK(iter.second ||
           (iter.first->second.first == src_loc.file_name() &&
            iter.first->second.second == src_loc.function_name()));
  }
  for (const auto& frame : data.frames()) {
    auto iter = current_state_->interned_frames_.emplace(
        frame.iid(), ProducerWriterState::Frame{
                         frame.has_rel_pc(), frame.rel_pc(),
                         frame.function_name_id(), frame.mapping_id()});
    DCHECK(iter.second || iter.first->second.rel_pc == frame.rel_pc());
  }
  for (const auto& module_name : data.mapping_paths()) {
    auto iter = current_state_->interned_module_names_.insert(
        std::make_pair(module_name.iid(), module_name.str()));
    DCHECK(iter.second || iter.first->second == module_name.str());
  }
  for (const auto& mapping_id : data.build_ids()) {
    auto iter = current_state_->interned_module_ids_.insert(
        std::make_pair(mapping_id.iid(), mapping_id.str()));
    DCHECK(iter.second || iter.first->second == mapping_id.str());
  }
  for (const auto& mapping : data.mappings()) {
    DCHECK_EQ(mapping.path_string_ids_size(), 1);
    auto iter = current_state_->interned_mappings_.emplace(
        mapping.iid(), ProducerWriterState::Mapping{
                           mapping.build_id(), mapping.path_string_ids(0)});
    DCHECK(iter.second || iter.first->second.build_id == mapping.build_id());
  }
  for (const auto& callstack : data.callstacks()) {
    std::vector<uint32_t> frame_ids;
    for (const auto& frame_id : callstack.frame_ids())
      frame_ids.push_back(frame_id);

    current_state_->interned_callstacks_.emplace(callstack.iid(),
                                                 std::move(frame_ids));
  }

  // Unordered data which may be required at any time during export.
  for (const auto& function_name : data.function_names()) {
    auto iter =
        unordered_state_data_[current_state_->trusted_packet_sequence_id]
            .interned_frame_names_.insert(
                std::make_pair(function_name.iid(), function_name.str()));
    DCHECK(iter.second || iter.first->second == function_name.str());
  }
}

void TrackEventJSONExporter::HandleProcessDescriptor(
    const ChromeTracePacket& packet) {
  DCHECK(packet.has_process_descriptor());
  const auto& process = packet.process_descriptor();
  // Save the current state we need for future packets.
  current_state_->pid = process.pid();
  if (process.has_process_priority()) {
    current_state_->process_priority = process.process_priority();
  }

  // ProcessDescriptor is only emitted on sequences with incremental state.
  if (current_state_->incomplete) {
    stats_.packets_dropped_invalid_incremental_state++;
    return;
  }

  // If we aren't outputting traceEvents then we don't need to look at the
  // metadata that might need to be emitted.
  if (!ShouldOutputTraceEvents()) {
    return;
  }

  // Prevent duplicates by only emitting the metadata once.
  if (current_state_->emitted_process_metadata) {
    return;
  }
  current_state_->emitted_process_metadata = true;

  if (!process.cmdline().empty()) {
    NOTIMPLEMENTED();
  }

  if (process.has_legacy_sort_index()) {
    auto event_builder =
        AddTraceEvent("process_sort_index", "__metadata", 'M', 0,
                      current_state_->pid, current_state_->pid);
    auto args_builder = event_builder.BuildArgs();
    auto* add_arg = args_builder->MaybeAddArg("sort_index");
    if (add_arg) {
      add_arg->AppendF("%" PRId32, process.legacy_sort_index());
    }
  }

  const auto emit_process_name = [this](const char* name) {
    auto event_builder =
        AddTraceEvent("process_name", "__metadata", 'M', 0, current_state_->pid,
                      current_state_->pid);
    auto args_builder = event_builder.BuildArgs();
    auto* add_arg = args_builder->MaybeAddArg("name");
    if (add_arg) {
      add_arg->AppendF("\"%s\"", name);
    }
  };
  switch (process.chrome_process_type()) {
    case perfetto::protos::ProcessDescriptor::PROCESS_UNSPECIFIED:
      // This process does not have a name.
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_BROWSER:
      emit_process_name("BROWSER");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_RENDERER:
      emit_process_name("RENDERER");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_UTILITY:
      emit_process_name("UTILITY");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_ZYGOTE:
      emit_process_name("ZYGOTE");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_SANDBOX_HELPER:
      emit_process_name("SANDBOX_HELPER");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_GPU:
      emit_process_name("GPU");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_PPAPI_PLUGIN:
      emit_process_name("PPAPI_PLUGIN");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_PPAPI_BROKER:
      emit_process_name("PPAPI_BROKER");
      break;
  }
}

void TrackEventJSONExporter::HandleThreadDescriptor(
    const ChromeTracePacket& packet) {
  DCHECK(packet.has_thread_descriptor());

  // ThreadDescriptor is only emitted on sequences with incremental state.
  if (current_state_->incomplete) {
    stats_.packets_dropped_invalid_incremental_state++;
    return;
  }

  const auto& thread = packet.thread_descriptor();
  // Save the current state we need for future packets.
  current_state_->pid = thread.pid();
  current_state_->tid = thread.tid();
  current_state_->time_us = thread.reference_timestamp_us();
  current_state_->thread_time_us = thread.reference_thread_time_us();
  current_state_->thread_instruction_count =
      thread.reference_thread_instruction_count();

  // If we aren't outputting traceEvents then we don't need to look at the
  // metadata that might need to be emitted.
  if (!ShouldOutputTraceEvents()) {
    return;
  }
  current_state_->last_seen_thread_descriptor =
      std::make_unique<ThreadDescriptor>();
  *current_state_->last_seen_thread_descriptor = thread;
}

void TrackEventJSONExporter::EmitThreadDescriptorIfNeeded() {
  if (!current_state_->last_seen_thread_descriptor) {
    return;
  }
  const auto& thread = *current_state_->last_seen_thread_descriptor;
  if (thread.has_legacy_sort_index()) {
    auto event_builder =
        AddTraceEvent("thread_sort_index", "__metadata", 'M', 0,
                      current_state_->pid, current_state_->tid);
    auto args_builder = event_builder.BuildArgs();
    auto* add_arg = args_builder->MaybeAddArg("sort_index");
    if (add_arg) {
      add_arg->AppendF("%" PRId32, thread.legacy_sort_index());
    }
  }

  const auto emit_thread_name = [this](const char* name) {
    auto event_builder =
        AddTraceEvent("thread_name", "__metadata", 'M', 0, current_state_->pid,
                      current_state_->tid);
    auto args_builder = event_builder.BuildArgs();
    auto* add_arg = args_builder->MaybeAddArg("name");
    if (add_arg) {
      add_arg->AppendF("\"%s\"", name);
    }
  };
  if (thread.has_thread_name()) {
    emit_thread_name(thread.thread_name().c_str());
  } else if (thread.has_chrome_thread_type()) {
    const char* name = ThreadTypeToName(thread.chrome_thread_type());
    if (name) {
      emit_thread_name(name);
    }
  }
  current_state_->last_seen_thread_descriptor.reset();
}

void TrackEventJSONExporter::HandleChromeEvents(
    const perfetto::protos::ChromeTracePacket& packet) {
  DCHECK(packet.has_chrome_events());

  const auto& chrome_events = packet.chrome_events();
  DCHECK(chrome_events.trace_events().empty())
      << "Found trace_events inside a ChromeEventBundle. This shouldn't "
      << "happen when emitting TrackEvents.";

  for (const auto& metadata : chrome_events.metadata()) {
    AddChromeMetadata(metadata);
  }
  for (const auto& legacy_ftrace : chrome_events.legacy_ftrace_output()) {
    AddLegacyFtrace(legacy_ftrace);
  }
  for (const auto& legacy_json_trace : chrome_events.legacy_json_trace()) {
    AddChromeLegacyJSONTrace(legacy_json_trace);
  }
}

void TrackEventJSONExporter::HandleTrackEvent(const ChromeTracePacket& packet) {
  DCHECK(packet.has_track_event());

  // TrackEvents need incremental state.
  if (current_state_->incomplete) {
    stats_.packets_dropped_invalid_incremental_state++;
    return;
  }

  // If we aren't outputting traceEvents nothing in a TrackEvent currently will
  // be needed so just return early.
  if (!ShouldOutputTraceEvents()) {
    return;
  }

  const auto& track = packet.track_event();

  // Get the time data out of the TrackEvent.
  int64_t timestamp_us = ComputeTimeUs(track);
  DCHECK_NE(timestamp_us, -1);
  base::Optional<int64_t> thread_time_us = ComputeThreadTimeUs(track);
  base::Optional<int64_t> thread_instruction_count =
      ComputeThreadInstructionCount(track);

  std::vector<base::StringPiece> all_categories;
  all_categories.reserve(track.category_iids().size());
  for (const auto& cat_iid : track.category_iids()) {
    const std::string& name =
        GetInternedName(cat_iid, current_state_->interned_event_categories_);
    all_categories.push_back(name);
  }
  const std::string joined_categories = base::JoinString(all_categories, ",");

  DCHECK(track.has_legacy_event()) << "required field legacy_event missing";
  auto builder =
      HandleLegacyEvent(track.legacy_event(), joined_categories, timestamp_us);

  if (thread_time_us) {
    builder.AddThreadTimestamp(*thread_time_us);
  }

  if (thread_instruction_count) {
    builder.AddThreadInstructionCount(*thread_instruction_count);
  }

  // Now we add args from both |task_execution| and |debug_annotations|. Recall
  // that |args_builder| must run its deconstructer before any other fields in
  // traceEvents are added. Therefore do not do anything below this comment but
  // add args.
  auto args_builder = builder.BuildArgs();

  for (const auto& debug_annotation : track.debug_annotations()) {
    HandleDebugAnnotation(debug_annotation, args_builder.get());
  }

  if (track.has_task_execution()) {
    HandleTaskExecution(track.task_execution(), args_builder.get());
  }
}

void TrackEventJSONExporter::HandleStreamingProfilePacket(
    const perfetto::protos::StreamingProfilePacket& profile_packet) {
  if (current_state_->incomplete) {
    stats_.packets_dropped_invalid_incremental_state++;
    return;
  }

  if (!ShouldOutputTraceEvents()) {
    return;
  }

  // Insert an event with the frames rendered as a string with the following
  // formats:
  //   offset - module [debugid]
  //  [OR]
  //   symbol - module []
  // The offset is difference between the load module address and the
  // frame address.
  //
  // Example:
  //
  //   "malloc             - libc.so    []
  //    std::string::alloc - stdc++.so  []
  //    off:7ffb3f991b2d   - USER32.dll [2103C0950C7DEC7F7AAA44348EDC1DDD1]
  //    off:7ffb3d439164   - win32u.dll [B3E4BE89CA7FB42A2AC1E1C475284CA11]
  //    off:7ffaf3e26201   - chrome.dll [8767EB7E1C77DD10014E8152A34786B812]
  //    off:7ffaf3e26008   - chrome.dll [8767EB7E1C77DD10014E8152A34786B812]
  //    [...] "

  DCHECK_EQ(profile_packet.callstack_iid_size(), 1);
  DCHECK_EQ(profile_packet.timestamp_delta_us_size(), 1);

  auto callstack = current_state_->interned_callstacks_.find(
      profile_packet.callstack_iid(0));
  DCHECK(callstack != current_state_->interned_callstacks_.end());

  std::string result;
  for (const auto& frame_id : callstack->second) {
    auto frame = current_state_->interned_frames_.find(frame_id);
    if (frame == current_state_->interned_frames_.end()) {
      base::StringAppendF(&result, "MISSING FRAME REFERENCE - ???\n");
      continue;
    }

    std::string frame_name;
    std::string module_name;
    std::string module_id;
    uintptr_t rel_pc = frame->second.rel_pc;
    const auto& frame_names =
        unordered_state_data_[current_state_->trusted_packet_sequence_id]
            .interned_frame_names_;
    if (frame->second.has_rel_pc) {
      const auto& profiled_frames =
          unordered_state_data_[current_state_->trusted_packet_sequence_id]
              .interned_profiled_frame_;

      auto frame_iter = profiled_frames.find(frame_id);
      if (frame_iter != profiled_frames.end()) {
        auto frame_name_iter = frame_names.find(frame_iter->second);
        DCHECK(frame_name_iter != frame_names.end());
        frame_name = frame_name_iter->second;
      } else {
        frame_name = base::StringPrintf("off:0x%" PRIxPTR, rel_pc);
      }
    } else {
      frame_name = GetInternedName(frame->second.function_name_id, frame_names);
    }

    auto module =
        current_state_->interned_mappings_.find(frame->second.mapping_id);
    if (module != current_state_->interned_mappings_.end()) {
      module_name = GetInternedName(module->second.name_id,
                                    current_state_->interned_module_names_);
      module_id = GetInternedName(module->second.build_id,
                                  current_state_->interned_module_ids_);
    }

    base::StringAppendF(&result, "%s - %s [%s]\n", frame_name.c_str(),
                        module_name.c_str(), module_id.c_str());
  }

  if (result.empty()) {
    result = "empty";
  }

  current_state_->time_us += profile_packet.timestamp_delta_us(0);

  auto event_builder = AddTraceEvent(
      "StackCpuSampling", TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT, current_state_->time_us,
      current_state_->pid, current_state_->tid);
  // Add a dummy thread timestamp to this event to match the format of instant
  // events. Useful in the UI to view args of a selected group of samples.
  event_builder.AddThreadTimestamp(1);
  static int g_id_counter = 0;
  event_builder.AddFlags(TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_HAS_ID,
                         ++g_id_counter, "");
  auto args_builder = event_builder.BuildArgs();
  auto* add_arg = args_builder->MaybeAddArg("frames");
  if (add_arg) {
    std::string escaped_result;
    base::EscapeJSONString(result, true, &escaped_result);
    add_arg->AppendF("%s", escaped_result.c_str());
  }
  add_arg = args_builder->MaybeAddArg("process_priority");
  if (add_arg) {
    add_arg->AppendF("%" PRId32, current_state_->process_priority);
  }
  // Used for backwards compatibility with the memlog pipeline,
  // should remove once we've switched to looking directly at the tid.
  add_arg = args_builder->MaybeAddArg("thread_id");
  if (add_arg) {
    add_arg->AppendF("%" PRId32, current_state_->tid);
  }
}

void TrackEventJSONExporter::HandleProfiledFrameSymbols(
    const perfetto::protos::ProfiledFrameSymbols& frame_symbols) {
  int64_t function_name_id = 0;
  // Chrome never has more than one function for an address, so we can just
  // take the first one.
  if (!frame_symbols.function_name_id().empty()) {
    DCHECK(frame_symbols.function_name_id().size() == 1);
    function_name_id = frame_symbols.function_name_id()[0];
  }

  auto iter = unordered_state_data_[current_state_->trusted_packet_sequence_id]
                  .interned_profiled_frame_.insert(std::make_pair(
                      frame_symbols.frame_iid(), function_name_id));
  auto& frame_names =
      unordered_state_data_[current_state_->trusted_packet_sequence_id]
          .interned_frame_names_;
  DCHECK(iter.second ||
         frame_names[iter.first->second] == frame_names[function_name_id]);
}

void TrackEventJSONExporter::HandleDebugAnnotation(
    const perfetto::protos::DebugAnnotation& debug_annotation,
    ArgumentBuilder* args_builder) {
  const std::string& name =
      GetInternedName(debug_annotation.name_iid(),
                      current_state_->interned_debug_annotation_names_);

  auto* maybe_arg = args_builder->MaybeAddArg(name);
  if (!maybe_arg) {
    return;
  }
  OutputJSONFromArgumentProto(debug_annotation, maybe_arg->mutable_out());
}

void TrackEventJSONExporter::HandleTaskExecution(
    const perfetto::protos::TaskExecution& task,
    ArgumentBuilder* args_builder) {
  auto iter =
      current_state_->interned_source_locations_.find(task.posted_from_iid());
  DCHECK(iter != current_state_->interned_source_locations_.end());

  // If source locations were turned off, only the file is provided. JSON
  // expects the event to then have only an "src" attribute.
  if (iter->second.second.empty()) {
    auto* maybe_arg = args_builder->MaybeAddArg("src");
    if (maybe_arg) {
      base::EscapeJSONString(iter->second.first, true,
                             maybe_arg->mutable_out());
    }
    return;
  }

  auto* maybe_arg = args_builder->MaybeAddArg("src_file");
  if (maybe_arg) {
    base::EscapeJSONString(iter->second.first, true, maybe_arg->mutable_out());
  }
  maybe_arg = args_builder->MaybeAddArg("src_func");
  if (maybe_arg) {
    base::EscapeJSONString(iter->second.second, true, maybe_arg->mutable_out());
  }
}

JSONTraceExporter::ScopedJSONTraceEventAppender
TrackEventJSONExporter::HandleLegacyEvent(const TrackEvent::LegacyEvent& event,
                                          const std::string& categories,
                                          int64_t timestamp_us) {
  DCHECK(event.name_iid());
  DCHECK(event.phase());

  // Determine which pid and tid to use.
  int32_t pid =
      event.pid_override() == 0 ? current_state_->pid : event.pid_override();
  int32_t tid =
      event.tid_override() == 0 ? current_state_->tid : event.tid_override();

  const std::string& name =
      GetInternedName(event.name_iid(), current_state_->interned_event_names_);

  // Build the actual json output, if we are missing the interned name we just
  // use the interned ID.
  auto builder = AddTraceEvent(name.c_str(), categories.c_str(), event.phase(),
                               timestamp_us, pid, tid);

  if (event.has_bind_id()) {
    builder.AddBindId(event.bind_id());
  }
  if (event.has_duration_us()) {
    builder.AddDuration(event.duration_us());
  }
  if (event.has_thread_duration_us()) {
    builder.AddThreadDuration(event.thread_duration_us());
  }
  if (event.has_thread_instruction_delta()) {
    builder.AddThreadInstructionDelta(event.thread_instruction_delta());
  }

  // For flags and ID we need to determine all possible flag bits and set them
  // correctly.
  uint32_t flags = 0;
  base::Optional<uint64_t> id;
  switch (event.id_case()) {
    case TrackEvent::LegacyEvent::kUnscopedId:
      flags |= TRACE_EVENT_FLAG_HAS_ID;
      id = event.unscoped_id();
      break;
    case TrackEvent::LegacyEvent::kLocalId:
      flags |= TRACE_EVENT_FLAG_HAS_LOCAL_ID;
      id = event.local_id();
      break;
    case TrackEvent::LegacyEvent::kGlobalId:
      flags |= TRACE_EVENT_FLAG_HAS_GLOBAL_ID;
      id = event.global_id();
      break;
    case TrackEvent::LegacyEvent::ID_NOT_SET:
      break;
  }
  if (event.use_async_tts()) {
    flags |= TRACE_EVENT_FLAG_ASYNC_TTS;
  }
  if (event.bind_to_enclosing()) {
    flags |= TRACE_EVENT_FLAG_BIND_TO_ENCLOSING;
  }
  switch (event.flow_direction()) {
    case TrackEvent::LegacyEvent::FLOW_IN:
      flags |= TRACE_EVENT_FLAG_FLOW_IN;
      break;
    case TrackEvent::LegacyEvent::FLOW_OUT:
      flags |= TRACE_EVENT_FLAG_FLOW_OUT;
      break;
    case TrackEvent::LegacyEvent::FLOW_INOUT:
      flags |= TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT;
      break;
    case TrackEvent::LegacyEvent::FLOW_UNSPECIFIED:
      break;
  }
  switch (event.instant_event_scope()) {
    case TrackEvent::LegacyEvent::SCOPE_GLOBAL:
      flags |= TRACE_EVENT_SCOPE_GLOBAL;
      break;
    case TrackEvent::LegacyEvent::SCOPE_PROCESS:
      flags |= TRACE_EVENT_SCOPE_PROCESS;
      break;
    case TrackEvent::LegacyEvent::SCOPE_THREAD:
      flags |= TRACE_EVENT_SCOPE_THREAD;
      break;
    case TrackEvent::LegacyEvent::SCOPE_UNSPECIFIED:
      break;
  }
  // Even if |flags==0|, we need to call AddFlags to output instant event scope.
  builder.AddFlags(flags, id, event.id_scope());
  return builder;
}

void TrackEventJSONExporter::EmitStats() {
  auto value = std::make_unique<base::DictionaryValue>();
  value->SetInteger("sequences_seen", stats_.sequences_seen);
  value->SetInteger("incremental_state_resets",
                    stats_.incremental_state_resets);
  value->SetInteger("packets_dropped_invalid_incremental_state",
                    stats_.packets_dropped_invalid_incremental_state);
  value->SetInteger("packets_with_previous_packet_dropped",
                    stats_.packets_with_previous_packet_dropped);
  AddMetadata("json_exporter_stats", std::move(value));
}

}  // namespace tracing
