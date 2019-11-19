// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/track_event_json_exporter.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/trace_event_analyzer.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

namespace tracing {
namespace {

using ::perfetto::protos::ProcessDescriptor;
using ::perfetto::protos::ThreadDescriptor;
using ::perfetto::protos::TrackEvent;
using ::trace_analyzer::Query;

constexpr int64_t kPid = 4;
constexpr int64_t kTid = 5;
constexpr int64_t kReferenceTimeUs = 1;
constexpr int64_t kReferenceThreadTimeUs = 2;
constexpr char kThreadName[] = "kThreadName";

// Defaults for LegacyEvents.
constexpr int32_t kLegacyPhase = TRACE_EVENT_PHASE_MEMORY_DUMP;
constexpr uint32_t kLegacyFlags = TRACE_EVENT_FLAG_HAS_ID;
constexpr int64_t kLegacyDuration = 100;
constexpr int64_t kLegacyThreadDuration = 101;
constexpr uint64_t kLegacyId = 102;
constexpr char kLegacyIdStr[] = "0x66";
constexpr char kLegacyScope[] = "legacy_scope";
constexpr uint64_t kLegacyBindId = 103;
constexpr char kLegacyBindIdStr[] = "0x67";

class TrackEventJsonExporterTest : public testing::Test {
 public:
  void SetUp() override {}

  void TearDown() override { json_trace_exporter_.reset(); }

  void OnTraceEventJson(std::string* json,
                        base::DictionaryValue* metadata,
                        bool has_more) {
    CHECK(!has_more);

    unparsed_trace_data_.swap(*json);
    parsed_trace_data_ = base::DictionaryValue::From(
        base::JSONReader::ReadDeprecated(unparsed_trace_data_));
    ASSERT_TRUE(parsed_trace_data_) << "Couldn't parse json: \n"
                                    << unparsed_trace_data_;

    // The TraceAnalyzer expects the raw trace output, without the
    // wrapping root-node.
    std::string raw_events;
    auto* events_value = parsed_trace_data_->FindKey("traceEvents");
    ASSERT_TRUE(events_value);
    base::JSONWriter::Write(*events_value, &raw_events);

    trace_analyzer_.reset(trace_analyzer::TraceAnalyzer::Create(raw_events));
    EXPECT_TRUE(trace_analyzer_);
  }

  void FinalizePackets(
      const std::vector<perfetto::protos::TracePacket>& trace_packet_protos) {
    json_trace_exporter_.reset(new TrackEventJSONExporter(
        JSONTraceExporter::ArgumentFilterPredicate(),
        JSONTraceExporter::MetadataFilterPredicate(),
        base::BindRepeating(&TrackEventJsonExporterTest::OnTraceEventJson,
                            base::Unretained(this))));

    std::vector<std::string> serialized_packets;
    // We need stable addressing of the string pointers so ensure we won't
    // reallocate the backing vector.
    serialized_packets.reserve(trace_packet_protos.size());
    std::vector<perfetto::TracePacket> packets;
    for (const auto& proto_packet : trace_packet_protos) {
      // Since we use AddSlice which does not copy the data the string has to
      // live as long as we want to use it. So it needs to survive until the
      // call to OnTraceData finishes below.
      serialized_packets.push_back(proto_packet.SerializeAsString());
      const std::string& ser_buf = serialized_packets.back();
      // Now we construct the inmemory TracePacket and store it.
      perfetto::TracePacket trace_packet;
      trace_packet.AddSlice(&ser_buf[0], ser_buf.size());
      packets.emplace_back(std::move(trace_packet));
    }

    json_trace_exporter_->OnTraceData(std::move(packets), false);
  }

 protected:
  // This class makes it easier to construct TrackEvents because NestedValues in
  // DebugAnnotations can be quite complex to set up.
  class NestedValue {
   public:
    NestedValue(const NestedValue& copy) : result_(copy.result_) {}

    explicit NestedValue(int64_t val) { result_.set_int_value(val); }
    explicit NestedValue(double val) { result_.set_double_value(val); }
    explicit NestedValue(bool val) { result_.set_bool_value(val); }
    explicit NestedValue(const std::string& val) {
      result_.set_string_value(val);
    }
    NestedValue(const std::vector<std::pair<std::string, NestedValue>>& vals) {
      result_.set_nested_type(
          perfetto::protos::DebugAnnotation::NestedValue::DICT);
      for (const auto& val : vals) {
        *result_.add_dict_keys() = val.first;
        *result_.add_dict_values() = val.second;
      }
    }

    explicit NestedValue(const std::vector<NestedValue>& vals) {
      result_.set_nested_type(
          perfetto::protos::DebugAnnotation::NestedValue::ARRAY);
      for (const auto& val : vals) {
        *result_.add_array_values() = val;
      }
    }

    // Intentionally implicit to allow assigning directly to the proto field.
    operator perfetto::protos::DebugAnnotation::NestedValue() const {
      return result_;
    }

   private:
    perfetto::protos::DebugAnnotation::NestedValue result_;
  };

  trace_analyzer::TraceAnalyzer* trace_analyzer() {
    return trace_analyzer_.get();
  }

  const base::DictionaryValue* parsed_trace_data() const {
    return parsed_trace_data_.get();
  }

  void AddProcessDescriptorPacket(
      std::vector<std::string> cmds,
      base::Optional<int32_t> sort_index,
      ProcessDescriptor::ChromeProcessType type,
      std::vector<perfetto::protos::TracePacket>* output,
      int32_t pid = kPid) {
    output->emplace_back();
    auto* result = output->back().mutable_process_descriptor();
    result->set_pid(pid);
    for (const auto& cmd : cmds) {
      *result->add_cmdline() = cmd;
    }
    if (sort_index) {
      result->set_legacy_sort_index(*sort_index);
    }
    result->set_chrome_process_type(type);
    // ProcessDescriptors don't require previous state.
    output->back().set_incremental_state_cleared(true);
  }

  void AddThreadDescriptorPacket(
      const base::Optional<int32_t>& sort_index,
      ThreadDescriptor::ChromeThreadType type,
      const base::Optional<std::string>& thread_name,
      int64_t reference_time_us,
      int64_t reference_thread_time_us,
      std::vector<perfetto::protos::TracePacket>* output,
      int32_t pid = kPid,
      int32_t tid = kTid) {
    output->emplace_back();
    auto* result = output->back().mutable_thread_descriptor();
    result->set_pid(pid);
    result->set_tid(tid);
    if (sort_index) {
      result->set_legacy_sort_index(*sort_index);
    }
    result->set_chrome_thread_type(type);
    if (thread_name) {
      result->set_thread_name(*thread_name);
    }
    result->set_reference_timestamp_us(reference_time_us);
    result->set_reference_thread_time_us(reference_thread_time_us);
    // ThreadDescriptors don't require previous state.
    output->back().set_incremental_state_cleared(true);
  }

  template <typename T>
  T CreateInternedName(uint32_t iid, const std::string& value) {
    T result;
    result.set_iid(iid);
    result.set_name(value);
    return result;
  }

  perfetto::protos::InternedString CreateInternedString(
      uint32_t iid,
      const std::string& value) {
    perfetto::protos::InternedString result;
    result.set_iid(iid);
    result.set_str(value);
    return result;
  }

  void AddInternedEventCategory(
      uint32_t iid,
      const std::string& value,
      std::vector<perfetto::protos::TracePacket>* output) {
    output->emplace_back();
    *output->back().mutable_interned_data()->add_event_categories() =
        CreateInternedName<perfetto::protos::EventCategory>(iid, value);
  }

  void AddInternedEventName(
      uint32_t iid,
      const std::string& value,
      std::vector<perfetto::protos::TracePacket>* output) {
    output->emplace_back();
    *output->back().mutable_interned_data()->add_event_names() =
        CreateInternedName<perfetto::protos::EventName>(iid, value);
  }

  void AddInternedDebugAnnotationName(
      uint32_t iid,
      const std::string& value,
      std::vector<perfetto::protos::TracePacket>* output) {
    output->emplace_back();
    *output->back().mutable_interned_data()->add_debug_annotation_names() =
        CreateInternedName<perfetto::protos::DebugAnnotationName>(iid, value);
  }

  void AddInternedSourceLocation(
      uint32_t iid,
      const std::string& file,
      const std::string& function,
      std::vector<perfetto::protos::TracePacket>* output) {
    output->emplace_back();
    auto* source =
        output->back().mutable_interned_data()->add_source_locations();
    source->set_iid(iid);
    source->set_file_name(file);
    source->set_function_name(function);
  }

  void AddInternedBuildID(uint32_t iid,
                          const std::string& value,
                          std::vector<perfetto::protos::TracePacket>* output) {
    output->emplace_back();
    *output->back().mutable_interned_data()->add_build_ids() =
        CreateInternedString(iid, value);
  }

  void AddInternedMappingPath(
      uint32_t iid,
      const std::string& value,
      std::vector<perfetto::protos::TracePacket>* output) {
    output->emplace_back();
    *output->back().mutable_interned_data()->add_mapping_paths() =
        CreateInternedString(iid, value);
  }

  void AddInternedFunctionName(
      uint32_t iid,
      const std::string& value,
      std::vector<perfetto::protos::TracePacket>* output) {
    output->emplace_back();
    *output->back().mutable_interned_data()->add_function_names() =
        CreateInternedString(iid, value);
  }

  void AddInternedMapping(uint32_t iid,
                          uint32_t build_iid,
                          uint32_t path_iid,
                          std::vector<perfetto::protos::TracePacket>* output) {
    output->emplace_back();
    auto* mapping = output->back().mutable_interned_data()->add_mappings();
    mapping->set_iid(iid);
    mapping->set_build_id(build_iid);
    mapping->add_path_string_ids(path_iid);
  }

  void AddInternedFrame(uint32_t iid,
                        bool set_rel_pc,
                        uint64_t rel_pc,
                        uint32_t function_name_iid,
                        uint32_t mapping_iid,
                        std::vector<perfetto::protos::TracePacket>* output) {
    output->emplace_back();
    auto* mapping = output->back().mutable_interned_data()->add_frames();
    mapping->set_iid(iid);
    mapping->set_mapping_id(mapping_iid);
    if (set_rel_pc) {
      mapping->set_rel_pc(rel_pc);
    } else {
      mapping->set_function_name_id(function_name_iid);
    }
  }

  void AddInternedCallstack(
      uint32_t iid,
      const std::vector<uint32_t> frame_iids,
      std::vector<perfetto::protos::TracePacket>* output) {
    output->emplace_back();
    auto* mapping = output->back().mutable_interned_data()->add_callstacks();
    mapping->set_iid(iid);
    for (const auto& frame_iid : frame_iids) {
      mapping->add_frame_ids(frame_iid);
    }
  }

  perfetto::protos::TrackEvent::LegacyEvent CreateLegacyEvent(
      uint32_t name_iid,  // interned EventName.
      uint32_t flags,
      int32_t phase,
      int32_t pid = kPid,
      int32_t tid = kTid) {
    perfetto::protos::TrackEvent::LegacyEvent event;
    event.set_name_iid(name_iid);
    event.set_phase(phase);
    if (pid != kPid) {
      event.set_pid_override(pid);
    }
    if (tid != kTid) {
      event.set_tid_override(tid);
    }

    // Use |flags| to set legacy fields correctly.
    if (flags & TRACE_EVENT_FLAG_HAS_ID) {
      event.set_unscoped_id(kLegacyId);
    } else if (flags & TRACE_EVENT_FLAG_HAS_LOCAL_ID) {
      event.set_local_id(kLegacyId);
    } else if (flags & TRACE_EVENT_FLAG_HAS_GLOBAL_ID) {
      event.set_global_id(kLegacyId);
    }

    if (event.id_case() !=
        perfetto::protos::TrackEvent::LegacyEvent::ID_NOT_SET) {
      event.set_id_scope(kLegacyScope);
    }

    if (flags & TRACE_EVENT_FLAG_ASYNC_TTS) {
      event.set_use_async_tts(true);
    }

    if (flags & TRACE_EVENT_FLAG_BIND_TO_ENCLOSING) {
      event.set_bind_to_enclosing(true);
    }

    uint32_t flow =
        flags & (TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
    switch (flow) {
      case (TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT):
        event.set_flow_direction(
            perfetto::protos::TrackEvent::LegacyEvent::FLOW_INOUT);
        break;
      case TRACE_EVENT_FLAG_FLOW_IN:
        event.set_flow_direction(
            perfetto::protos::TrackEvent::LegacyEvent::FLOW_IN);
        break;
      case TRACE_EVENT_FLAG_FLOW_OUT:
        event.set_flow_direction(
            perfetto::protos::TrackEvent::LegacyEvent::FLOW_OUT);
        break;
    }

    // The rest of the fields are set to default/sane values.
    if (phase == TRACE_EVENT_PHASE_COMPLETE) {
      event.set_duration_us(kLegacyDuration);
      event.set_thread_duration_us(kLegacyThreadDuration);
    } else if (phase == TRACE_EVENT_PHASE_INSTANT) {
      switch (flags & TRACE_EVENT_FLAG_SCOPE_MASK) {
        case TRACE_EVENT_SCOPE_GLOBAL:
          event.set_instant_event_scope(TrackEvent::LegacyEvent::SCOPE_GLOBAL);
          break;
        case TRACE_EVENT_SCOPE_PROCESS:
          event.set_instant_event_scope(TrackEvent::LegacyEvent::SCOPE_PROCESS);
          break;
        case TRACE_EVENT_SCOPE_THREAD:
          event.set_instant_event_scope(TrackEvent::LegacyEvent::SCOPE_THREAD);
          break;
      }
    }
    event.set_bind_id(kLegacyBindId);

    return event;
  }

  std::string unparsed_trace_data_;

 private:
  std::unique_ptr<TrackEventJSONExporter> json_trace_exporter_;
  std::unique_ptr<trace_analyzer::TraceAnalyzer> trace_analyzer_;
  std::unique_ptr<base::DictionaryValue> parsed_trace_data_;
};

TEST_F(TrackEventJsonExporterTest, EmptyProcessDescriptor) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;
  AddProcessDescriptorPacket(
      /* cmds = */ {}, /* sort_index = */ base::nullopt,
      ProcessDescriptor::PROCESS_UNSPECIFIED, &trace_packet_protos);
  FinalizePackets(trace_packet_protos);
  // No traceEvents or data was emitted but a process descriptor without extra
  // data should just be an empty array and not cause crashes.
  EXPECT_THAT(unparsed_trace_data_,
              testing::StartsWith("{\"traceEvents\":[],"));
}

TEST_F(TrackEventJsonExporterTest, SortIndexProcessDescriptor) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;
  AddProcessDescriptorPacket(/* cmds = */ {}, /* sort_index = */ 2,
                             ProcessDescriptor::PROCESS_UNSPECIFIED,
                             &trace_packet_protos);
  FinalizePackets(trace_packet_protos);
  ASSERT_EQ(1u,
            trace_analyzer()->FindEvents(
                Query(Query::EVENT_NAME) == Query::String("process_sort_index"),
                &events));
  EXPECT_EQ("process_sort_index", events[0]->name);
  EXPECT_EQ("__metadata", events[0]->category);
  EXPECT_EQ('M', events[0]->phase);
  EXPECT_EQ(0, events[0]->timestamp);
  ASSERT_TRUE(events[0]->HasArg("sort_index"));
  EXPECT_EQ(2, events[0]->GetKnownArgAsInt("sort_index"));
}

TEST_F(TrackEventJsonExporterTest, ProcessTypeProcessDescriptor) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;
  AddProcessDescriptorPacket(
      /* cmds = */ {}, /* sort_index = */ base::nullopt,
      ProcessDescriptor::PROCESS_UNSPECIFIED, &trace_packet_protos);
  FinalizePackets(trace_packet_protos);

  // UNSPECIFIED does not add a process_name metadata event.
  EXPECT_EQ(0u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("process_name"),
                    &events));

  for (int process_type = static_cast<int>(ProcessDescriptor::PROCESS_BROWSER);
       process_type <
       static_cast<int>(ProcessDescriptor::ChromeProcessType_ARRAYSIZE);
       ++process_type) {
    ASSERT_TRUE(ProcessDescriptor::ChromeProcessType_IsValid(process_type));
    events.clear();
    trace_packet_protos[0]
        .mutable_process_descriptor()
        ->set_chrome_process_type(
            static_cast<ProcessDescriptor::ChromeProcessType>(process_type));
    FinalizePackets(trace_packet_protos);
    EXPECT_EQ(1u, trace_analyzer()->FindEvents(
                      Query(Query::EVENT_NAME) == Query::String("process_name"),
                      &events));
    EXPECT_EQ("process_name", events[0]->name);
    EXPECT_EQ("__metadata", events[0]->category);
    EXPECT_EQ('M', events[0]->phase);
    EXPECT_EQ(0, events[0]->timestamp);
    ASSERT_TRUE(events[0]->HasArg("name"));
    switch (static_cast<ProcessDescriptor::ChromeProcessType>(process_type)) {
      case ProcessDescriptor::PROCESS_UNSPECIFIED:
        ADD_FAILURE() << "Unspecified triggered a process_name event when it "
                         "shouldn't have. Name \""
                      << events[0]->GetKnownArgAsString("name") << "\"";
        break;
      case ProcessDescriptor::PROCESS_BROWSER:
        EXPECT_EQ("BROWSER", events[0]->GetKnownArgAsString("name"));
        break;
      case ProcessDescriptor::PROCESS_RENDERER:
        EXPECT_EQ("RENDERER", events[0]->GetKnownArgAsString("name"));
        break;
      case ProcessDescriptor::PROCESS_UTILITY:
        EXPECT_EQ("UTILITY", events[0]->GetKnownArgAsString("name"));
        break;
      case ProcessDescriptor::PROCESS_ZYGOTE:
        EXPECT_EQ("ZYGOTE", events[0]->GetKnownArgAsString("name"));
        break;
      case ProcessDescriptor::PROCESS_SANDBOX_HELPER:
        EXPECT_EQ("SANDBOX_HELPER", events[0]->GetKnownArgAsString("name"));
        break;
      case ProcessDescriptor::PROCESS_GPU:
        EXPECT_EQ("GPU", events[0]->GetKnownArgAsString("name"));
        break;
      case ProcessDescriptor::PROCESS_PPAPI_PLUGIN:
        EXPECT_EQ("PPAPI_PLUGIN", events[0]->GetKnownArgAsString("name"));
        break;
      case ProcessDescriptor::PROCESS_PPAPI_BROKER:
        EXPECT_EQ("PPAPI_BROKER", events[0]->GetKnownArgAsString("name"));
        break;
    }
  }
}

TEST_F(TrackEventJsonExporterTest, MultipleProcessDescriptors) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;
  // Sort index 2 + name BROWSER.
  AddProcessDescriptorPacket(/* cmds = */ {}, /* sort_index = */ 2,
                             ProcessDescriptor::PROCESS_BROWSER,
                             &trace_packet_protos);
  // We've already emitted the process metadata so all future ProcessDescriptors
  // will not cause new events until a new sequence is hit (even on resets).
  AddProcessDescriptorPacket(/* cmds = */ {}, /* sort_index = */ 3,
                             ProcessDescriptor::PROCESS_RENDERER,
                             &trace_packet_protos);
  trace_packet_protos.back().set_incremental_state_cleared(true);
  // Empty.
  AddProcessDescriptorPacket(
      /* cmds = */ {}, /* sort_index = */ base::nullopt,
      ProcessDescriptor::PROCESS_UNSPECIFIED, &trace_packet_protos);
  FinalizePackets(trace_packet_protos);
  ASSERT_EQ(2u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_CATEGORY) == Query::String("__metadata"),
                    &events));
  for (size_t i = 0; i < events.size(); ++i) {
    const auto* event = events[i];
    if (event->name == "process_name") {
      EXPECT_EQ("BROWSER", event->GetKnownArgAsString("name"));
    } else if (event->name == "process_sort_index") {
      EXPECT_EQ(2, event->GetKnownArgAsInt("sort_index"));
    } else {
      ADD_FAILURE() << "Unexpected event name: " << event->name;
    }
  }

  // However if we put it on a new sequence then the metadata will be emitted
  // again.
  trace_packet_protos[1].set_trusted_packet_sequence_id(333);
  FinalizePackets(trace_packet_protos);

  FinalizePackets(trace_packet_protos);
  ASSERT_EQ(4u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_CATEGORY) == Query::String("__metadata"),
                    &events));
  for (size_t i = 0; i < 2; ++i) {
    const auto* event = events[i];
    if (event->name == "process_name") {
      EXPECT_EQ("BROWSER", event->GetKnownArgAsString("name"));
    } else if (event->name == "process_sort_index") {
      EXPECT_EQ(2, event->GetKnownArgAsInt("sort_index"));
    } else {
      ADD_FAILURE() << "Unexpected event name: " << event->name;
    }
  }
  for (size_t i = 2; i < events.size(); ++i) {
    const auto* event = events[i];
    if (event->name == "process_name") {
      EXPECT_EQ("RENDERER", event->GetKnownArgAsString("name"));
    } else if (event->name == "process_sort_index") {
      EXPECT_EQ(3, event->GetKnownArgAsInt("sort_index"));
    } else {
      ADD_FAILURE() << "Unexpected event name: " << event->name;
    }
  }
}

TEST_F(TrackEventJsonExporterTest, EmptyThreadDescriptor) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;
  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  FinalizePackets(trace_packet_protos);
  // No traceEvents or data was emitted but a thread descriptor should be an
  // empty array and not cause crashes.
  EXPECT_THAT(unparsed_trace_data_,
              testing::StartsWith("{\"traceEvents\":[],"));
}

TEST_F(TrackEventJsonExporterTest, SortIndexThreadDescriptor) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;
  AddThreadDescriptorPacket(
      /* sort_index = */ 2, ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
      base::nullopt, kReferenceTimeUs, kReferenceThreadTimeUs,
      &trace_packet_protos);
  FinalizePackets(trace_packet_protos);
  ASSERT_EQ(1u,
            trace_analyzer()->FindEvents(
                Query(Query::EVENT_NAME) == Query::String("thread_sort_index"),
                &events));
  EXPECT_EQ("thread_sort_index", events[0]->name);
  EXPECT_EQ("__metadata", events[0]->category);
  EXPECT_EQ('M', events[0]->phase);
  EXPECT_EQ(0, events[0]->timestamp);
  ASSERT_TRUE(events[0]->HasArg("sort_index"));
  EXPECT_EQ(2, events[0]->GetKnownArgAsInt("sort_index"));
}

TEST_F(TrackEventJsonExporterTest, ThreadNameThreadDescriptor) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;
  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            kThreadName, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  FinalizePackets(trace_packet_protos);
  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("thread_name"),
                    &events));
  EXPECT_EQ("thread_name", events[0]->name);
  EXPECT_EQ("__metadata", events[0]->category);
  EXPECT_EQ('M', events[0]->phase);
  EXPECT_EQ(0, events[0]->timestamp);
  ASSERT_TRUE(events[0]->HasArg("name"));
  EXPECT_EQ(kThreadName, events[0]->GetKnownArgAsString("name"));
}

TEST_F(TrackEventJsonExporterTest, MainThreadNameThreadDescriptor) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;
  AddThreadDescriptorPacket(
      /* sort_index = */ base::nullopt, ThreadDescriptor::CHROME_THREAD_MAIN,
      base::nullopt, kReferenceTimeUs, kReferenceThreadTimeUs,
      &trace_packet_protos);
  FinalizePackets(trace_packet_protos);
  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("thread_name"),
                    &events));
  EXPECT_EQ("thread_name", events[0]->name);
  EXPECT_EQ("__metadata", events[0]->category);
  EXPECT_EQ('M', events[0]->phase);
  EXPECT_EQ(0, events[0]->timestamp);
  ASSERT_TRUE(events[0]->HasArg("name"));
  EXPECT_EQ("CrProcessMain", events[0]->GetKnownArgAsString("name"));
}

TEST_F(TrackEventJsonExporterTest, MultipleThreadDescriptors) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;
  // Thread can have no name in the first packet.
  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddThreadDescriptorPacket(
      /* sort_index = */ 2, ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
      "different_thread_name", kReferenceTimeUs, kReferenceThreadTimeUs,
      &trace_packet_protos);
  // This packet will be ignored because we've already emitted the sort_index of
  // 2 and thread_name kThreadName so we suppress this metadata because it
  // isn't supposed to have changed (even if reset).
  ASSERT_NE("different_thread_name", kThreadName);
  AddThreadDescriptorPacket(
      /* sort_index = */ 3, ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
      kThreadName, kReferenceTimeUs, kReferenceThreadTimeUs,
      &trace_packet_protos);
  trace_packet_protos.back().set_incremental_state_cleared(true);
  FinalizePackets(trace_packet_protos);
  ASSERT_EQ(2u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_CATEGORY) == Query::String("__metadata"),
                    &events));
  for (size_t i = 0; i < events.size(); ++i) {
    const auto* event = events[i];
    if (event->name == "thread_name") {
      EXPECT_EQ(kThreadName, event->GetKnownArgAsString("name"));
    } else if (event->name == "thread_sort_index") {
      EXPECT_EQ(3, event->GetKnownArgAsInt("sort_index"));
    } else {
      ADD_FAILURE() << "Unexpected event name: " << event->name;
    }
  }

  // However if we put it on a new sequence then the metadata will be emitted
  // again.
  trace_packet_protos[1].set_trusted_packet_sequence_id(333);
  FinalizePackets(trace_packet_protos);

  FinalizePackets(trace_packet_protos);
  ASSERT_EQ(4u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_CATEGORY) == Query::String("__metadata"),
                    &events));
  for (size_t i = 0; i < 2; ++i) {
    const auto* event = events[i];
    if (event->name == "thread_name") {
      EXPECT_EQ("different_thread_name", event->GetKnownArgAsString("name"));
    } else if (event->name == "thread_sort_index") {
      EXPECT_EQ(2, event->GetKnownArgAsInt("sort_index"));
    } else {
      ADD_FAILURE() << "Unexpected event name: " << event->name;
    }
  }
  for (size_t i = 2; i < events.size(); ++i) {
    const auto* event = events[i];
    if (event->name == "thread_name") {
      EXPECT_EQ(kThreadName, event->GetKnownArgAsString("name"));
    } else if (event->name == "thread_sort_index") {
      EXPECT_EQ(3, event->GetKnownArgAsInt("sort_index"));
    } else {
      ADD_FAILURE() << "Unexpected event name: " << event->name;
    }
  }
}

TEST_F(TrackEventJsonExporterTest, JustInternedData) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  AddInternedEventName(2, "event_name", &trace_packet_protos);
  AddInternedEventCategory(2, "event_category", &trace_packet_protos);
  trace_packet_protos.back().set_incremental_state_cleared(true);
  AddInternedEventCategory(3, "event_category", &trace_packet_protos);
  AddInternedDebugAnnotationName(4, "debug_annotation_name",
                                 &trace_packet_protos);
  AddInternedSourceLocation(1, "file_name", "function_name",
                            &trace_packet_protos);
  FinalizePackets(trace_packet_protos);
  // Interned data by itself does not call any trace events to be emitted.
  EXPECT_THAT(unparsed_trace_data_,
              testing::StartsWith("{\"traceEvents\":[],"));
}

TEST_F(TrackEventJsonExporterTest, LegacyEventBasicTest) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  // Add the required intern state plus one extra unused interned string.
  // Ensuring we reset the state with the first packet.
  AddInternedEventName(2, "event_name_2", &trace_packet_protos);
  trace_packet_protos.back().set_incremental_state_cleared(true);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);
  AddInternedEventCategory(2, "event_category_2", &trace_packet_protos);
  AddInternedEventCategory(4, "event_category_4", &trace_packet_protos);

  // Now we construct a Legacy event that overrides everything and only relies
  // on interned data as needed.
  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->set_timestamp_absolute_us(4);
  track_event->set_thread_time_absolute_us(5);
  track_event->add_category_iids(3);
  track_event->add_category_iids(4);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase,
                        /* pid = */ 7, /* tid = */ 8);

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_EQ("event_name_3", events[0]->name);
  EXPECT_EQ("event_category_3,event_category_4", events[0]->category);
  EXPECT_EQ(4, events[0]->timestamp);
  EXPECT_EQ(5, events[0]->thread_timestamp);
  EXPECT_EQ(0, events[0]->duration);
  EXPECT_EQ(0, events[0]->thread_duration);
  EXPECT_EQ(kLegacyPhase, events[0]->phase);
  EXPECT_EQ(kLegacyIdStr, events[0]->id);
  EXPECT_EQ(kLegacyScope, events[0]->scope);
  EXPECT_EQ(kLegacyBindIdStr, events[0]->bind_id);
  EXPECT_FALSE(events[0]->flow_out);
  EXPECT_FALSE(events[0]->flow_in);
  EXPECT_TRUE(events[0]->local_id2.empty());
  EXPECT_TRUE(events[0]->global_id2.empty());
  EXPECT_EQ(7, events[0]->thread.process_id);
  EXPECT_EQ(8, events[0]->thread.thread_id);
}

TEST_F(TrackEventJsonExporterTest, LegacyEventFilledInState) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  // Now we construct a Legacy event however it relies on the ThreadDescriptor
  // to provide the extra data needed to make a complete event.
  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(3);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(6);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase);

  FinalizePackets(trace_packet_protos);

  // Since there was no ThreadDescriptor or interned data this event should be
  // completely dropped, but to enable potentially partial trace exports we
  // shouldn't expect a crash.
  ASSERT_EQ(0u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));

  // This provides the pid & tid, as well as the timestamps reference points.
  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  // To correctly use the state the thread descriptor has to come
  // before so we swap the last two packets. However since we don't have the
  // interned names yet this will DCHECK.
  std::swap(trace_packet_protos[0], trace_packet_protos[1]);
  EXPECT_DCHECK_DEATH(FinalizePackets(trace_packet_protos));

  // Now with the interned data.
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);
  // Again swap the TrackEvent to the end.
  std::swap(trace_packet_protos[1], trace_packet_protos[3]);
  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_EQ("event_name_3", events[0]->name);
  EXPECT_EQ("event_category_3", events[0]->category);
  EXPECT_EQ(kReferenceTimeUs + 4, events[0]->timestamp);
  EXPECT_EQ(kReferenceThreadTimeUs + 6, events[0]->thread_timestamp);
  EXPECT_EQ(kPid, events[0]->thread.process_id);
  EXPECT_EQ(kTid, events[0]->thread.thread_id);
}

TEST_F(TrackEventJsonExporterTest, LegacyEventTimestampDelta) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);

  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(3);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(6);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase);
  trace_packet_protos.push_back(trace_packet_protos.back());
  trace_packet_protos.push_back(trace_packet_protos.back());
  trace_packet_protos.back().mutable_track_event()->set_timestamp_absolute_us(
      333);
  trace_packet_protos.back().mutable_track_event()->set_thread_time_absolute_us(
      666);
  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(3u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_EQ(kReferenceTimeUs + 4, events[0]->timestamp);
  EXPECT_EQ(kReferenceThreadTimeUs + 6, events[0]->thread_timestamp);
  EXPECT_EQ(kReferenceTimeUs + 4 * 2, events[1]->timestamp);
  EXPECT_EQ(kReferenceThreadTimeUs + 6 * 2, events[1]->thread_timestamp);
  EXPECT_EQ(333, events[2]->timestamp);
  EXPECT_EQ(666, events[2]->thread_timestamp);
}

TEST_F(TrackEventJsonExporterTest, LegacyEventPhase) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);

  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(3);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(6);

  *track_event->mutable_legacy_event() = CreateLegacyEvent(
      /* name_iid = */ 3, kLegacyFlags, TRACE_EVENT_PHASE_COMPLETE);
  trace_packet_protos.push_back(trace_packet_protos.back());
  trace_packet_protos.back()
      .mutable_track_event()
      ->mutable_legacy_event()
      ->set_phase(TRACE_EVENT_PHASE_INSTANT);

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(2u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_EQ(TRACE_EVENT_PHASE_COMPLETE, events[0]->phase);
  EXPECT_EQ(TRACE_EVENT_PHASE_INSTANT, events[1]->phase);
}

TEST_F(TrackEventJsonExporterTest, LegacyEventDuration) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);

  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(3);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(6);

  *track_event->mutable_legacy_event() = CreateLegacyEvent(
      /* name_iid = */ 3, kLegacyFlags, TRACE_EVENT_PHASE_COMPLETE);

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_EQ(kLegacyDuration, events[0]->duration);
  EXPECT_EQ(kLegacyThreadDuration, events[0]->thread_duration);
}

TEST_F(TrackEventJsonExporterTest, LegacyEventId) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);

  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(3);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(6);

  *track_event->mutable_legacy_event() = CreateLegacyEvent(
      /* name_iid = */ 3, TRACE_EVENT_FLAG_HAS_ID,
      TRACE_EVENT_PHASE_MEMORY_DUMP);

  trace_packet_protos.push_back(trace_packet_protos.back());
  *trace_packet_protos.back().mutable_track_event()->mutable_legacy_event() =
      CreateLegacyEvent(
          /* name_iid = */ 3, TRACE_EVENT_FLAG_HAS_LOCAL_ID,
          TRACE_EVENT_PHASE_MARK);

  trace_packet_protos.push_back(trace_packet_protos.back());
  *trace_packet_protos.back().mutable_track_event()->mutable_legacy_event() =
      CreateLegacyEvent(
          /* name_iid = */ 3, TRACE_EVENT_FLAG_HAS_GLOBAL_ID,
          TRACE_EVENT_PHASE_MARK);

  trace_packet_protos.push_back(trace_packet_protos.back());
  *trace_packet_protos.back().mutable_track_event()->mutable_legacy_event() =
      CreateLegacyEvent(
          /* name_iid = */ 3, 0, TRACE_EVENT_PHASE_MARK);
  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(4u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_EQ(kLegacyIdStr, events[0]->id);
  EXPECT_TRUE(events[0]->local_id2.empty());
  EXPECT_TRUE(events[0]->global_id2.empty());
  EXPECT_TRUE(events[1]->id.empty());
  EXPECT_EQ(kLegacyIdStr, events[1]->local_id2);
  EXPECT_TRUE(events[1]->global_id2.empty());
  EXPECT_TRUE(events[2]->id.empty());
  EXPECT_TRUE(events[2]->local_id2.empty());
  EXPECT_EQ(kLegacyIdStr, events[2]->global_id2);
  EXPECT_TRUE(events[3]->id.empty());
  EXPECT_TRUE(events[3]->local_id2.empty());
  EXPECT_TRUE(events[3]->global_id2.empty());

  // Because trace_analyzer only parses the respective ID fields when a
  // corresponding flag is set we also do a regex based on each line to ensure
  // it only has the correct type.
  //
  // First line shouldn't have id2.
  EXPECT_THAT(unparsed_trace_data_,
              ::testing::Not(::testing::ContainsRegex("^.*\"id2\".*\n")));
  // Second line should not have id or global id2.
  EXPECT_THAT(unparsed_trace_data_,
              ::testing::Not(::testing::ContainsRegex("^.*\n.*\"id\".*\n")));
  EXPECT_THAT(unparsed_trace_data_, ::testing::Not(::testing::ContainsRegex(
                                        "^.*\n.*\"id2\":\\{\"global\".*\n")));
  // Third line should not have id or local id2.
  EXPECT_THAT(
      unparsed_trace_data_,
      ::testing::Not(::testing::ContainsRegex("^.*\n.*\n.*\"id\".*\n")));
  EXPECT_THAT(unparsed_trace_data_,
              ::testing::Not(::testing::ContainsRegex(
                  "^.*\n.*\n.*\"id2\":\\{\"local\".*\n")));
  // Fourth line should not have id or id2.
  EXPECT_THAT(
      unparsed_trace_data_,
      ::testing::Not(::testing::ContainsRegex("^.*\n.*\n.*\n.*\"id.*")));
}

TEST_F(TrackEventJsonExporterTest, LegacyEventIdScope) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);

  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(3);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(6);

  *track_event->mutable_legacy_event() = CreateLegacyEvent(
      /* name_iid = */ 3, kLegacyFlags, kLegacyPhase);
  track_event->mutable_legacy_event()->clear_id_scope();

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_TRUE(events[0]->scope.empty());

  track_event->mutable_legacy_event()->set_id_scope(kLegacyScope);
  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_EQ(kLegacyScope, events[0]->scope);
}

TEST_F(TrackEventJsonExporterTest, LegacyEventAsyncTts) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);

  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(3);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(6);

  *track_event->mutable_legacy_event() = CreateLegacyEvent(
      /* name_iid = */ 3, kLegacyFlags, kLegacyPhase);

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_THAT(
      unparsed_trace_data_,
      ::testing::Not(::testing::ContainsRegex(".*\"use_async_tts\":.*")));

  // Now rewrite the legacy event with the TRACE_EVENT_FLAG_ASYNC_TTS
  // flag.
  *track_event->mutable_legacy_event() = CreateLegacyEvent(
      /* name_iid = */ 3, kLegacyFlags | TRACE_EVENT_FLAG_ASYNC_TTS,
      kLegacyPhase);

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_THAT(unparsed_trace_data_,
              ::testing::ContainsRegex(".*\"use_async_tts\":1.*"));
}

TEST_F(TrackEventJsonExporterTest, LegacyEventBindId) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);

  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(3);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(6);

  *track_event->mutable_legacy_event() = CreateLegacyEvent(
      /* name_iid = */ 3, kLegacyFlags, kLegacyPhase);
  track_event->mutable_legacy_event()->clear_bind_id();

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_TRUE(events[0]->bind_id.empty());

  track_event->mutable_legacy_event()->set_bind_id(kLegacyBindId);
  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_EQ(kLegacyBindIdStr, events[0]->bind_id);
}

TEST_F(TrackEventJsonExporterTest, LegacyEventBindToEnclosing) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);

  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(3);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(6);

  *track_event->mutable_legacy_event() = CreateLegacyEvent(
      /* name_iid = */ 3, kLegacyFlags, kLegacyPhase);

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_THAT(unparsed_trace_data_,
              ::testing::Not(::testing::ContainsRegex(".*\"bp\":.*")));

  // Now rewrite the legacy event with the TRACE_EVENT_FLAG_BIND_TO_ENCLOSING
  // flag.
  *track_event->mutable_legacy_event() = CreateLegacyEvent(
      /* name_iid = */ 3, kLegacyFlags | TRACE_EVENT_FLAG_BIND_TO_ENCLOSING,
      kLegacyPhase);

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_THAT(unparsed_trace_data_,
              ::testing::ContainsRegex(".*\"bp\":\"e\".*"));
}

TEST_F(TrackEventJsonExporterTest, LegacyEventFlowEvents) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);

  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(3);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(6);

  *track_event->mutable_legacy_event() = CreateLegacyEvent(
      /* name_iid = */ 3,
      kLegacyFlags | TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
      kLegacyPhase);

  trace_packet_protos.push_back(trace_packet_protos.back());
  *trace_packet_protos.back().mutable_track_event()->mutable_legacy_event() =
      CreateLegacyEvent(
          /* name_iid = */ 3, kLegacyFlags | TRACE_EVENT_FLAG_FLOW_IN,
          kLegacyPhase);

  trace_packet_protos.push_back(trace_packet_protos.back());
  *trace_packet_protos.back().mutable_track_event()->mutable_legacy_event() =
      CreateLegacyEvent(
          /* name_iid = */ 3, kLegacyFlags | TRACE_EVENT_FLAG_FLOW_OUT,
          kLegacyPhase);
  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(3u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  EXPECT_TRUE(events[0]->flow_in);
  EXPECT_TRUE(events[0]->flow_out);
  EXPECT_TRUE(events[1]->flow_in);
  EXPECT_FALSE(events[1]->flow_out);
  EXPECT_FALSE(events[2]->flow_in);
  EXPECT_TRUE(events[2]->flow_out);
}

TEST_F(TrackEventJsonExporterTest, LegacyEventInstantEventScope) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);

  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(3);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(6);

  *track_event->mutable_legacy_event() = CreateLegacyEvent(
      /* name_iid = */ 3, TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_SCOPE_GLOBAL,
      TRACE_EVENT_PHASE_INSTANT);

  trace_packet_protos.push_back(trace_packet_protos.back());
  *trace_packet_protos.back().mutable_track_event()->mutable_legacy_event() =
      CreateLegacyEvent(
          /* name_iid = */ 3,
          TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_SCOPE_PROCESS,
          TRACE_EVENT_PHASE_INSTANT);

  trace_packet_protos.push_back(trace_packet_protos.back());
  *trace_packet_protos.back().mutable_track_event()->mutable_legacy_event() =
      CreateLegacyEvent(
          /* name_iid = */ 3,
          TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_SCOPE_THREAD,
          TRACE_EVENT_PHASE_INSTANT);
  trace_packet_protos.push_back(trace_packet_protos.back());
  trace_packet_protos.back()
      .mutable_track_event()
      ->mutable_legacy_event()
      ->clear_instant_event_scope();
  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(4u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  // For whatever reason trace_analyzer() does not extract the instant event
  // scope so look for it in the unparsed data in the order we added them.
  EXPECT_THAT(unparsed_trace_data_,
              ::testing::ContainsRegex(base::StringPrintf(
                  ".*\"s\":\"%c\".*\n"
                  ".*\"s\":\"%c\".*\n"
                  ".*\"s\":\"%c\".*\n"
                  ".*\"s\":\"g\".*",  // UNSPECIFIED defaults to global.
                  TRACE_EVENT_SCOPE_NAME_GLOBAL, TRACE_EVENT_SCOPE_NAME_PROCESS,
                  TRACE_EVENT_SCOPE_NAME_THREAD)));
}

TEST_F(TrackEventJsonExporterTest, TaskExecutionAddedAsArgs) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);
  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->set_timestamp_absolute_us(4);
  track_event->add_category_iids(3);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase);

  trace_packet_protos.back()
      .mutable_track_event()
      ->mutable_task_execution()
      ->set_posted_from_iid(4);

  EXPECT_DCHECK_DEATH(FinalizePackets(trace_packet_protos));

  AddInternedSourceLocation(4, "file_name", "function_name",
                            &trace_packet_protos);
  std::swap(trace_packet_protos[3], trace_packet_protos[4]);

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  ASSERT_TRUE(events[0]->HasArg("src_file"));
  ASSERT_TRUE(events[0]->HasArg("src_func"));
  EXPECT_EQ("file_name", events[0]->GetKnownArgAsString("src_file"));
  EXPECT_EQ("function_name", events[0]->GetKnownArgAsString("src_func"));

  // A source location without function name converts into a single "src" arg.
  trace_packet_protos[3]
      .mutable_interned_data()
      ->mutable_source_locations(0)
      ->clear_function_name();
  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  ASSERT_TRUE(events[0]->HasArg("src"));
  EXPECT_EQ("file_name", events[0]->GetKnownArgAsString("src"));
}

TEST_F(TrackEventJsonExporterTest, DebugAnnotationRequiresName) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);
  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->set_timestamp_absolute_us(4);
  track_event->add_category_iids(3);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase,
                        /* pid = */ 7, /* tid = */ 8);

  auto* annotation =
      trace_packet_protos.back().mutable_track_event()->add_debug_annotations();
  annotation->set_name_iid(2);
  annotation->set_bool_value(true);

  EXPECT_DCHECK_DEATH(FinalizePackets(trace_packet_protos));
}

TEST_F(TrackEventJsonExporterTest, DebugAnnotationBoolValue) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);
  AddInternedDebugAnnotationName(2, "bool_2", &trace_packet_protos);
  AddInternedDebugAnnotationName(3, "bool_3", &trace_packet_protos);
  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->set_timestamp_absolute_us(4);
  track_event->add_category_iids(3);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase,
                        /* pid = */ 7, /* tid = */ 8);

  auto* annotation =
      trace_packet_protos.back().mutable_track_event()->add_debug_annotations();
  annotation->set_name_iid(2);
  annotation->set_bool_value(true);
  annotation =
      trace_packet_protos.back().mutable_track_event()->add_debug_annotations();
  annotation->set_name_iid(3);
  annotation->set_bool_value(false);

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  ASSERT_TRUE(events[0]->HasArg("bool_2"));
  EXPECT_TRUE(events[0]->GetKnownArgAsBool("bool_2"));
  ASSERT_TRUE(events[0]->HasArg("bool_3"));
  EXPECT_FALSE(events[0]->GetKnownArgAsBool("bool_3"));
}

TEST_F(TrackEventJsonExporterTest, DebugAnnotationUintValue) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);
  AddInternedDebugAnnotationName(2, "uint", &trace_packet_protos);
  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->set_timestamp_absolute_us(4);
  track_event->add_category_iids(3);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase,
                        /* pid = */ 7, /* tid = */ 8);

  auto* annotation =
      trace_packet_protos.back().mutable_track_event()->add_debug_annotations();
  annotation->set_name_iid(2);
  annotation->set_uint_value(std::numeric_limits<uint64_t>::max());

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  ASSERT_TRUE(events[0]->HasArg("uint"));
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(),
            events[0]->GetKnownArgAsDouble("uint"));
}

TEST_F(TrackEventJsonExporterTest, DebugAnnotationIntValue) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);
  AddInternedDebugAnnotationName(2, "int", &trace_packet_protos);
  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->set_timestamp_absolute_us(4);
  track_event->add_category_iids(3);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase,
                        /* pid = */ 7, /* tid = */ 8);

  auto* annotation =
      trace_packet_protos.back().mutable_track_event()->add_debug_annotations();
  annotation->set_name_iid(2);
  annotation->set_int_value(std::numeric_limits<int64_t>::max());

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  ASSERT_TRUE(events[0]->HasArg("int"));
  EXPECT_EQ(std::numeric_limits<int64_t>::max(),
            events[0]->GetKnownArgAsDouble("int"));
}

TEST_F(TrackEventJsonExporterTest, DebugAnnotationDoubleValue) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);
  AddInternedDebugAnnotationName(1, "double_min", &trace_packet_protos);
  AddInternedDebugAnnotationName(3, "double_max", &trace_packet_protos);
  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->set_timestamp_absolute_us(4);
  track_event->add_category_iids(3);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase,
                        /* pid = */ 7, /* tid = */ 8);

  auto* annotation =
      trace_packet_protos.back().mutable_track_event()->add_debug_annotations();
  annotation->set_name_iid(1);
  annotation->set_double_value(std::numeric_limits<double>::min());

  annotation =
      trace_packet_protos.back().mutable_track_event()->add_debug_annotations();
  annotation->set_name_iid(3);
  annotation->set_double_value(std::numeric_limits<double>::max());

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));

  // NaN and +/- Inf aren't allowed by json.
  ASSERT_TRUE(events[0]->HasArg("double_min"));
  EXPECT_EQ(std::numeric_limits<double>::min(),
            events[0]->GetKnownArgAsDouble("double_min"));
  ASSERT_TRUE(events[0]->HasArg("double_max"));
  EXPECT_EQ(std::numeric_limits<double>::max(),
            events[0]->GetKnownArgAsDouble("double_max"));
}

TEST_F(TrackEventJsonExporterTest, DebugAnnotationStringValue) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);
  AddInternedDebugAnnotationName(2, "string", &trace_packet_protos);
  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->set_timestamp_absolute_us(4);
  track_event->add_category_iids(3);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase,
                        /* pid = */ 7, /* tid = */ 8);

  auto* annotation =
      trace_packet_protos.back().mutable_track_event()->add_debug_annotations();
  annotation->set_name_iid(2);
  annotation->set_string_value("foo");

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  ASSERT_TRUE(events[0]->HasArg("string"));
  EXPECT_EQ("foo", events[0]->GetKnownArgAsString("string"));
}

TEST_F(TrackEventJsonExporterTest, DebugAnnotationPointerValue) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);
  AddInternedDebugAnnotationName(2, "pointer", &trace_packet_protos);
  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->set_timestamp_absolute_us(4);
  track_event->add_category_iids(3);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase,
                        /* pid = */ 7, /* tid = */ 8);

  auto* annotation =
      trace_packet_protos.back().mutable_track_event()->add_debug_annotations();
  annotation->set_name_iid(2);
  annotation->set_pointer_value(1);

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  ASSERT_TRUE(events[0]->HasArg("pointer"));
  EXPECT_EQ("0x1", events[0]->GetKnownArgAsString("pointer"));
}

TEST_F(TrackEventJsonExporterTest, DebugAnnotationComplexNestedValue) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);
  AddInternedDebugAnnotationName(2, "top_level_dict", &trace_packet_protos);
  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->set_timestamp_absolute_us(4);
  track_event->add_category_iids(3);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase,
                        /* pid = */ 7, /* tid = */ 8);

  auto* annotation =
      trace_packet_protos.back().mutable_track_event()->add_debug_annotations();
  annotation->set_name_iid(2);
  *annotation->mutable_nested_value() = NestedValue(
      {{"arr0", NestedValue({NestedValue(int64_t(1)), NestedValue(bool(true)),
                             NestedValue({{"i2", NestedValue(int64_t(3))}})})},
       {"b0", NestedValue(bool(true))},
       {"d0", NestedValue(double(6.0))},
       {"dict0",
        NestedValue({{"dict1", NestedValue({{"b2", NestedValue(bool(false))}})},
                     {"i1", NestedValue(int64_t(2014))},
                     {"s1", NestedValue(std::string("foo"))}})},
       {"i0", NestedValue(int64_t(2014))},
       {"s0", NestedValue(std::string("foo"))}});

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  ASSERT_TRUE(events[0]->HasArg("top_level_dict"));
  auto value = events[0]->GetKnownArgAsValue("top_level_dict");
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_dict());
  EXPECT_EQ(6.0, value->FindKey("d0")->GetDouble());
  EXPECT_EQ(2014, value->FindKey("i0")->GetInt());
  EXPECT_EQ("foo", value->FindKey("s0")->GetString());

  auto* arr0 = value->FindKey("arr0");
  ASSERT_TRUE(arr0);
  ASSERT_TRUE(arr0->is_list());
  const auto& list0 = arr0->GetList();
  ASSERT_EQ(3u, list0.size());
  EXPECT_EQ(1, list0[0].GetInt());
  EXPECT_TRUE(list0[1].GetBool());
  const auto& arr0_dict1 = list0[2];
  ASSERT_TRUE(arr0_dict1.is_dict());
  EXPECT_EQ(3, arr0_dict1.FindKey("i2")->GetInt());

  auto* dict0 = value->FindKey("dict0");
  ASSERT_TRUE(dict0);
  EXPECT_EQ(2014, dict0->FindKey("i1")->GetInt());
  EXPECT_EQ("foo", dict0->FindKey("s1")->GetString());
  auto* dict0_dict1 = dict0->FindKey("dict1");
  ASSERT_TRUE(dict0_dict1);
  EXPECT_FALSE(dict0_dict1->FindKey("b2")->GetBool());
}

TEST_F(TrackEventJsonExporterTest, DebugAnnotationLegacyJsonValue) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "event_name_3", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);
  AddInternedDebugAnnotationName(2, "legacy_json", &trace_packet_protos);
  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->set_timestamp_absolute_us(4);
  track_event->add_category_iids(3);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase,
                        /* pid = */ 7, /* tid = */ 8);

  auto* annotation =
      trace_packet_protos.back().mutable_track_event()->add_debug_annotations();
  annotation->set_name_iid(2);
  annotation->set_legacy_json_value("{\"fooName\":\"fooValue\"}");

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("event_name_3"),
                    &events));
  ASSERT_TRUE(events[0]->HasArg("legacy_json"));
  auto value = events[0]->GetKnownArgAsValue("legacy_json");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->is_dict());
  auto* sub_value = value->FindKey("fooName");
  ASSERT_TRUE(sub_value);
  EXPECT_EQ("fooValue", sub_value->GetString());
}

TEST_F(TrackEventJsonExporterTest, TestMetadata) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;

  trace_packet_protos.emplace_back();
  {
    auto* new_metadata =
        trace_packet_protos.back().mutable_chrome_events()->add_metadata();
    new_metadata->set_name("int_metadata");
    new_metadata->set_int_value(42);
  }

  {
    auto* new_metadata =
        trace_packet_protos.back().mutable_chrome_events()->add_metadata();
    new_metadata->set_name("string_metadata");
    new_metadata->set_string_value("met_val");
  }

  {
    auto* new_metadata =
        trace_packet_protos.back().mutable_chrome_events()->add_metadata();
    new_metadata->set_name("bool_metadata");
    new_metadata->set_bool_value(true);
  }

  {
    auto* new_metadata =
        trace_packet_protos.back().mutable_chrome_events()->add_metadata();
    new_metadata->set_name("dict_metadata");
    new_metadata->set_json_value("{\"child_dict\": \"foo\"}");
  }

  FinalizePackets(trace_packet_protos);

  auto* metadata = parsed_trace_data()->FindKey("metadata");
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->FindKey("int_metadata")->GetInt(), 42);
  EXPECT_EQ(metadata->FindKey("string_metadata")->GetString(), "met_val");
  EXPECT_EQ(metadata->FindKey("bool_metadata")->GetBool(), true);
  EXPECT_EQ(
      metadata->FindKey("dict_metadata")->FindKey("child_dict")->GetString(),
      "foo");
}

TEST_F(TrackEventJsonExporterTest, TestLegacySystemFtrace) {
  std::string ftrace = "#dummy data";

  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_packet_protos.emplace_back();
  trace_packet_protos.back().mutable_chrome_events()->add_legacy_ftrace_output(
      ftrace);
  FinalizePackets(trace_packet_protos);

  auto* sys_trace = parsed_trace_data()->FindKey("systemTraceEvents");
  EXPECT_TRUE(sys_trace);
  EXPECT_EQ(sys_trace->GetString(), ftrace);
}

TEST_F(TrackEventJsonExporterTest, TestLegacyUserTrace) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;

  trace_packet_protos.emplace_back();
  auto* json_trace = trace_packet_protos.back()
                         .mutable_chrome_events()
                         ->add_legacy_json_trace();
  json_trace->set_type(perfetto::protos::ChromeLegacyJsonTrace::USER_TRACE);
  json_trace->set_data(
      "{\"pid\":10,\"tid\":11,\"ts\":23,\"ph\":\"I\""
      ",\"cat\":\"cat_name2\",\"name\":\"bar_name\""
      ",\"id2\":{\"global\":\"0x5\"},\"args\":{}}");

  FinalizePackets(trace_packet_protos);

  const trace_analyzer::TraceEvent* trace_event = trace_analyzer()->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("bar_name"));
  EXPECT_TRUE(trace_event);

  EXPECT_EQ(10, trace_event->thread.process_id);
  EXPECT_EQ(11, trace_event->thread.thread_id);
  EXPECT_EQ(23, trace_event->timestamp);
  EXPECT_EQ('I', trace_event->phase);
  EXPECT_EQ("bar_name", trace_event->name);
  EXPECT_EQ("cat_name2", trace_event->category);
  EXPECT_EQ("0x5", trace_event->global_id2);
}

TEST_F(TrackEventJsonExporterTest, TestTraceStats) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;

  trace_packet_protos.emplace_back();
  // We are just checking that we correctly called the function provided by the
  // base class. See json_trace_exporter_unittest for a more complete test.
  trace_packet_protos.back().mutable_trace_stats();
  FinalizePackets(trace_packet_protos);

  EXPECT_THAT(unparsed_trace_data_, testing::StartsWith("{\"traceEvents\":[],"
                                                        "\"metadata\":{"));
  EXPECT_THAT(unparsed_trace_data_,
              testing::HasSubstr("\"perfetto_trace_stats\":{\""));
}

TEST_F(TrackEventJsonExporterTest, ComplexLongSequenceWithDroppedPackets) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;
  size_t idx = 0;

  // Sequence 10 is complete with no data loss and 3 events. 2 with delta
  // timestamps and one with an absolute timestamps 1 us further than the delta
  // events.
  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(1, "sequence_10", &trace_packet_protos);
  AddInternedEventCategory(1, "event_category_1", &trace_packet_protos);
  trace_packet_protos.emplace_back();
  auto* track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(1);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(3);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 1, kLegacyFlags, kLegacyPhase);
  trace_packet_protos.push_back(trace_packet_protos.back());
  trace_packet_protos.push_back(trace_packet_protos.back());
  trace_packet_protos.back().mutable_track_event()->set_timestamp_absolute_us(
      333);
  trace_packet_protos.back().mutable_track_event()->set_thread_time_absolute_us(
      666);
  for (; idx < trace_packet_protos.size(); ++idx) {
    trace_packet_protos[idx].set_trusted_packet_sequence_id(10);
  }

  // Sequence 20 alternates between emitting an event dropping packets and
  // clearing incremental state.
  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(2, "sequence_20", &trace_packet_protos);
  AddInternedEventCategory(2, "event_category_2", &trace_packet_protos);

  trace_packet_protos.emplace_back();
  track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(2);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(3);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 2, kLegacyFlags, kLegacyPhase);

  // This event will be dropped.
  trace_packet_protos.push_back(trace_packet_protos.back());
  trace_packet_protos.back().set_previous_packet_dropped(true);

  // Add a TraceStats packet, which shouldn't be dropped even though the
  // incremental state was not cleared yet.
  trace_packet_protos.emplace_back();
  trace_packet_protos.back().mutable_trace_stats();

  // Reset the state.
  AddThreadDescriptorPacket(
      /* sort_index = */ base::nullopt,
      ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
      /* thread_name = */ base::nullopt, kReferenceTimeUs + 4 * 3,
      kReferenceThreadTimeUs + 3 * 3, &trace_packet_protos);
  trace_packet_protos.back().set_incremental_state_cleared(true);
  AddInternedEventName(2, "sequence_20", &trace_packet_protos);
  AddInternedEventCategory(2, "event_category_2", &trace_packet_protos);

  trace_packet_protos.emplace_back();
  *trace_packet_protos.back().mutable_track_event() = *track_event;
  // This event will be dropped.
  trace_packet_protos.push_back(trace_packet_protos.back());
  trace_packet_protos.back().set_previous_packet_dropped(true);
  for (; idx < trace_packet_protos.size(); ++idx) {
    trace_packet_protos[idx].set_trusted_packet_sequence_id(2);
  }

  // Sequence 30 emits a single event to ensure that sequence 2 doesn't prevent
  // these events from being emitted.
  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);
  AddInternedEventName(3, "sequence_30", &trace_packet_protos);
  AddInternedEventCategory(3, "event_category_3", &trace_packet_protos);
  trace_packet_protos.emplace_back();
  track_event = trace_packet_protos.back().mutable_track_event();
  track_event->add_category_iids(3);
  track_event->set_timestamp_delta_us(4);
  track_event->set_thread_time_delta_us(3);
  *track_event->mutable_legacy_event() =
      CreateLegacyEvent(/* name_iid = */ 3, kLegacyFlags, kLegacyPhase);
  for (; idx < trace_packet_protos.size(); ++idx) {
    trace_packet_protos[idx].set_trusted_packet_sequence_id(30);
  }

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(3u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("sequence_10"),
                    &events));
  EXPECT_EQ(kReferenceTimeUs + 4, events[0]->timestamp);
  EXPECT_EQ(kReferenceThreadTimeUs + 3, events[0]->thread_timestamp);
  EXPECT_EQ(kReferenceTimeUs + 4 * 2, events[1]->timestamp);
  EXPECT_EQ(kReferenceThreadTimeUs + 3 * 2, events[1]->thread_timestamp);
  EXPECT_EQ(333, events[2]->timestamp);
  EXPECT_EQ(666, events[2]->thread_timestamp);

  ASSERT_EQ(2u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("sequence_20"),
                    &events));
  EXPECT_EQ(kReferenceTimeUs + 4, events[0]->timestamp);
  EXPECT_EQ(kReferenceThreadTimeUs + 3, events[0]->thread_timestamp);
  // When we reset we resent as if 2 packets had occurred in the middle.
  EXPECT_EQ(kReferenceTimeUs + 4 * 4, events[1]->timestamp);
  EXPECT_EQ(kReferenceThreadTimeUs + 3 * 4, events[1]->thread_timestamp);

  ASSERT_EQ(1u, trace_analyzer()->FindEvents(
                    Query(Query::EVENT_NAME) == Query::String("sequence_30"),
                    &events));
  EXPECT_EQ(kReferenceTimeUs + 4, events[0]->timestamp);
  EXPECT_EQ(kReferenceThreadTimeUs + 3, events[0]->thread_timestamp);

  // We only verify that the TraceStats packet was not dropped.
  EXPECT_THAT(unparsed_trace_data_,
              testing::HasSubstr("\"perfetto_trace_stats\""));
}

TEST_F(TrackEventJsonExporterTest, SamplingProfilePacket) {
  std::vector<perfetto::protos::TracePacket> trace_packet_protos;
  trace_analyzer::TraceEventVector events;

  AddThreadDescriptorPacket(/* sort_index = */ base::nullopt,
                            ThreadDescriptor::CHROME_THREAD_UNSPECIFIED,
                            /* thread_name = */ base::nullopt, kReferenceTimeUs,
                            kReferenceThreadTimeUs, &trace_packet_protos);

  AddInternedMappingPath(1, "my_module_1", &trace_packet_protos);
  AddInternedBuildID(11, "AAAAAAAAA", &trace_packet_protos);
  AddInternedMapping(1, /*build_iid=*/11, /*mapping_path_iid=*/1,
                     &trace_packet_protos);
  AddInternedMappingPath(2, "my_module_2", &trace_packet_protos);
  AddInternedBuildID(22, "BBBBBBB", &trace_packet_protos);
  AddInternedMapping(2, /*build_iid=*/22, /*mapping_path_iid=*/2,
                     &trace_packet_protos);

  AddInternedFunctionName(1, "strlen", &trace_packet_protos);
  AddInternedFunctionName(2, "RunMainLoop", &trace_packet_protos);

  AddInternedFrame(1, /*set_rel_pc=*/false, /*rel_pc=*/0,
                   /*function_name_iid=*/1, /*mapping_iid=*/1,
                   &trace_packet_protos);
  AddInternedFrame(2, /*set_rel_pc=*/true, /*rel_pc=*/42,
                   /*function_name_iid=*/0, /*mapping_iid=*/1,
                   &trace_packet_protos);
  AddInternedFrame(3, /*set_rel_pc=*/true, /*rel_pc=*/424242,
                   /*function_name_iid=*/0, /*mapping_iid=*/2,
                   &trace_packet_protos);
  AddInternedFrame(4, /*set_rel_pc=*/false, /*rel_pc=*/0,
                   /*function_name_iid=*/2, /*mapping_iid=*/2,
                   &trace_packet_protos);
  AddInternedFrame(5, /*set_rel_pc=*/true, /*rel_pc=*/0,
                   /*function_name_iid=*/0, /*mapping_iid=*/2,
                   &trace_packet_protos);

  AddInternedCallstack(1, {1, 2, 3, 4, 5}, &trace_packet_protos);

  trace_packet_protos.emplace_back();
  auto* profile_packet =
      trace_packet_protos.back().mutable_streaming_profile_packet();
  profile_packet->add_timestamp_delta_us(4);
  profile_packet->add_callstack_iid(1);

  FinalizePackets(trace_packet_protos);

  ASSERT_EQ(1u,
            trace_analyzer()->FindEvents(
                Query(Query::EVENT_NAME) == Query::String("StackCpuSampling"),
                &events));
  ASSERT_TRUE(events[0]->HasArg("frames"));
  auto value = events[0]->GetKnownArgAsValue("frames");
  ASSERT_TRUE(value);
  EXPECT_EQ(
      "strlen - my_module_1 [AAAAAAAAA]\n"
      "off:0x2a - my_module_1 [AAAAAAAAA]\n"
      "off:0x67932 - my_module_2 [BBBBBBB]\n"
      "RunMainLoop - my_module_2 [BBBBBBB]\n"
      "off:0x0 - my_module_2 [BBBBBBB]\n",
      value->GetString());
}

}  // namespace
}  // namespace tracing
