// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/json_trace_exporter.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

using base::trace_event::TraceLog;

namespace tracing {

namespace {

struct ArgValue {
  ArgValue(int64_t val) : int_val(val), type(ArgType::kInt) {}
  ArgValue(uint64_t val) : uint_val(val), type(ArgType::kUint) {}
  ArgValue(double val) : double_val(val), type(ArgType::kDouble) {}
  ArgValue(std::string val) : string_val(val), type(ArgType::kString) {}
  ArgValue(bool val) : bool_val(val), type(ArgType::kBool) {}
  ArgValue(base::Value* val) : value_val(val), type(ArgType::kValue) {}

  int64_t int_val;
  uint64_t uint_val;
  double double_val;
  std::string string_val;
  bool bool_val;
  base::Value* value_val;
  enum class ArgType { kInt, kUint, kDouble, kString, kBool, kValue };
  ArgType type;
};

struct FakeTraceInfo {
  FakeTraceInfo(const std::string& e,
                const std::string& c,
                char p,
                int64_t t,
                int32_t pi,
                int32_t ti)
      : event_name(e),
        category_group_names(c),
        phase(p),
        timestamp(t),
        pid(pi),
        tid(ti) {}

  // Required values.
  std::string event_name;
  std::string category_group_names;
  char phase;
  int64_t timestamp;
  int32_t pid;
  int32_t tid;

  // Optional values.
  base::Optional<int64_t> duration;
  base::Optional<int64_t> thread_duration;
  base::Optional<int64_t> thread_timestamp;
  base::Optional<int64_t> bind_id;
  base::Optional<int32_t> flags;
  base::Optional<int64_t> id;
  base::Optional<std::string> scope;
  std::vector<std::pair<std::string, ArgValue>> args;
};

class TestJSONTraceExporter : public JSONTraceExporter {
 public:
  TestJSONTraceExporter(ArgumentFilterPredicate argument_filter_predicate,
                        MetadataFilterPredicate metadata_filter_predicate,
                        OnTraceEventJSONCallback callback)
      : JSONTraceExporter(std::move(argument_filter_predicate),
                          std::move(metadata_filter_predicate),
                          std::move(callback)) {}
  ~TestJSONTraceExporter() override = default;

  int process_packets_calls() const { return process_packets_calls_; }

  void SetFakeTraceEvents(const std::vector<FakeTraceInfo>& values) {
    infos_ = values;
  }

  std::vector<perfetto::protos::ChromeLegacyJsonTrace> json_traces;
  std::vector<std::string> legacy_ftrace_output;
  std::vector<perfetto::protos::ChromeMetadata> metadata;
  std::vector<perfetto::protos::TraceStats> stats;

 protected:
  void ProcessPackets(const std::vector<perfetto::TracePacket>& packets,
                      bool has_more) override {
    ++process_packets_calls_;
    DCHECK(packets.size() == infos_.size())
        << " different sizes of packets versus expected behaviour test set up "
        << "error.";
    for (const auto& event : infos_) {
      auto scoped_builder = AddTraceEvent(
          event.event_name.c_str(), event.category_group_names.c_str(),
          event.phase, event.timestamp, event.pid, event.tid);
      if (event.duration) {
        scoped_builder.AddDuration(*event.duration);
      }
      if (event.thread_duration) {
        scoped_builder.AddThreadDuration(*event.thread_duration);
      }
      if (event.thread_timestamp) {
        scoped_builder.AddThreadTimestamp(*event.thread_timestamp);
      }
      if (event.bind_id) {
        scoped_builder.AddBindId(*event.bind_id);
      }
      if (event.flags) {
        scoped_builder.AddFlags(*event.flags, event.id.value_or(0),
                                event.scope.value_or(""));
      }
      if (!event.args.empty()) {
        auto args_builder = scoped_builder.BuildArgs();
        for (const auto& arg : event.args) {
          const std::string& name = std::get<0>(arg);
          const ArgValue val = std::get<1>(arg);
          auto* maybe_arg = args_builder->MaybeAddArg(name);
          std::string temp;
          if (maybe_arg) {
            switch (val.type) {
              case ArgValue::ArgType::kInt:
                *maybe_arg += std::to_string(val.int_val);
                break;
              case ArgValue::ArgType::kUint:
                *maybe_arg += std::to_string(val.uint_val);
                break;
              case ArgValue::ArgType::kDouble:
                *maybe_arg += std::to_string(val.double_val);
                break;
              case ArgValue::ArgType::kString:
                *maybe_arg += val.string_val;
                break;
              case ArgValue::ArgType::kBool:
                *maybe_arg += val.bool_val ? "true" : "false";
                break;
              case ArgValue::ArgType::kValue:
                temp.clear();
                base::JSONWriter::Write(*val.value_val, &temp);
                *maybe_arg += temp;
                break;
            }
          }
        }
      }
    }

    for (const auto& value : json_traces) {
      AddChromeLegacyJSONTrace(value);
    }
    for (const auto& value : legacy_ftrace_output) {
      AddLegacyFtrace(value);
    }
    for (const auto& value : metadata) {
      AddChromeMetadata(value);
    }
    for (const auto& value : stats) {
      SetTraceStatsMetadata(value);
    }
  }

 private:
  int process_packets_calls_ = 0;
  std::vector<FakeTraceInfo> infos_;
};

}  // namespace

class JsonTraceExporterTest : public testing::Test {
 public:
  JsonTraceExporterTest()
      : json_trace_exporter_(new TestJSONTraceExporter(
            JSONTraceExporter::ArgumentFilterPredicate(),
            JSONTraceExporter::MetadataFilterPredicate(),
            base::BindRepeating(&JsonTraceExporterTest::OnTraceEventJSON,
                                base::Unretained(this)))) {}

  void OnTraceEventJSON(std::string* json,
                        base::DictionaryValue* metadata,
                        bool has_more) {
    unparsed_trace_data_ += *json;
    unparsed_trace_data_sequence_.push_back(std::string());
    unparsed_trace_data_sequence_.back().swap(*json);
    if (has_more) {
      return;
    }
    parsed_trace_data_ = base::DictionaryValue::From(
        base::JSONReader::ReadDeprecated(unparsed_trace_data_));
    EXPECT_TRUE(parsed_trace_data_);
    if (!parsed_trace_data_) {
      LOG(ERROR) << "Couldn't parse json: \n" << unparsed_trace_data_;
    }

    // The TraceAnalyzer expects the raw trace output, without the
    // wrapping root-node.
    std::string raw_events;
    auto* events_value = parsed_trace_data_->FindKey("traceEvents");
    ASSERT_TRUE(events_value);
    base::JSONWriter::Write(*events_value, &raw_events);

    trace_analyzer_.reset(trace_analyzer::TraceAnalyzer::Create(raw_events));
    EXPECT_TRUE(trace_analyzer_);
  }

  std::string unparsed_trace_data_;
  std::vector<std::string> unparsed_trace_data_sequence_;
  std::unique_ptr<TestJSONTraceExporter> json_trace_exporter_;
  std::unique_ptr<trace_analyzer::TraceAnalyzer> trace_analyzer_;
  std::unique_ptr<base::DictionaryValue> parsed_trace_data_;
};

TEST_F(JsonTraceExporterTest, TestNoTraceEvents) {
  json_trace_exporter_->OnTraceData(std::vector<perfetto::TracePacket>(),
                                    false);
  EXPECT_EQ("{\"traceEvents\":[]}", unparsed_trace_data_);
}

TEST_F(JsonTraceExporterTest, TestBasic) {
  std::vector<FakeTraceInfo> infos = {FakeTraceInfo(
      "name", "cat", 'B', /* timestamp = */ 1, /* pid = */ 2, /* tid = */ 3)};
  json_trace_exporter_->SetFakeTraceEvents(infos);
  json_trace_exporter_->OnTraceData(
      std::vector<perfetto::TracePacket>(infos.size()), false);
  EXPECT_EQ(
      "{\"traceEvents\":[{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\","
      "\"cat\":\"cat\",\"name\":\"name\",\"args\":{}}]}",
      unparsed_trace_data_);
  const trace_analyzer::TraceEvent* trace_event = trace_analyzer_->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("name"));
  EXPECT_TRUE(trace_event);
  EXPECT_EQ(2, trace_event->thread.process_id);
  EXPECT_EQ(3, trace_event->thread.thread_id);
  EXPECT_EQ(1, trace_event->timestamp);
  EXPECT_EQ('B', trace_event->phase);
  EXPECT_EQ("name", trace_event->name);
  EXPECT_EQ("cat", trace_event->category);
}

TEST_F(JsonTraceExporterTest, TestAllTraceEventButFlagsAndArgs) {
  // Duration is only parsed if the phase is correct in this case 'X'.
  std::vector<FakeTraceInfo> infos = {FakeTraceInfo("name_all", "cat_all", 'X',
                                                    /* timestamp = */ 1,
                                                    /* pid = */ 2,
                                                    /* tid = */ 3)};
  infos[0].duration = 4;
  infos[0].thread_duration = 5;
  infos[0].thread_timestamp = 6;
  infos[0].bind_id = 7;
  json_trace_exporter_->SetFakeTraceEvents(infos);
  json_trace_exporter_->OnTraceData(
      std::vector<perfetto::TracePacket>(infos.size()), false);
  EXPECT_EQ(
      "{\"traceEvents\":[{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"X\","
      "\"cat\":\"cat_all\",\"name\":\"name_all\",\"dur\":4,\"tdur\":5,"
      "\"tts\":6,\"bind_id\":\"0x7\",\"args\":{}}]}",
      unparsed_trace_data_);
  const trace_analyzer::TraceEvent* trace_event = trace_analyzer_->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("name_all"));
  EXPECT_TRUE(trace_event);
  EXPECT_EQ(2, trace_event->thread.process_id);
  EXPECT_EQ(3, trace_event->thread.thread_id);
  EXPECT_EQ(1, trace_event->timestamp);
  EXPECT_EQ('X', trace_event->phase);
  EXPECT_EQ("name_all", trace_event->name);
  EXPECT_EQ("cat_all", trace_event->category);
  EXPECT_EQ(4, trace_event->duration);
  EXPECT_EQ(5, trace_event->thread_duration);
  EXPECT_EQ(6, trace_event->thread_timestamp);
  EXPECT_EQ("0x7", trace_event->bind_id);
}

TEST_F(JsonTraceExporterTest, TestAddFlagsAllButIdFlagsAndPhaseScope) {
  std::vector<FakeTraceInfo> infos = {FakeTraceInfo(
      "name", "cat", 'I', /* timestamp = */ 1, /* pid = */ 2, /* tid = */ 3)};
  infos[0].flags = TRACE_EVENT_FLAG_ASYNC_TTS |
                   TRACE_EVENT_FLAG_BIND_TO_ENCLOSING |
                   TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT |
                   TRACE_EVENT_SCOPE_GLOBAL;
  json_trace_exporter_->SetFakeTraceEvents(infos);
  json_trace_exporter_->OnTraceData(
      std::vector<perfetto::TracePacket>(infos.size()), false);
  EXPECT_EQ(
      "{\"traceEvents\":[{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"I\","
      "\"cat\":\"cat\",\"name\":\"name\",\"use_async_tts\":1,\"bp\":\"e\","
      "\"flow_in\":true,\"flow_out\":true,\"s\":\"g\",\"args\":{}}]}",
      unparsed_trace_data_);
  const trace_analyzer::TraceEvent* trace_event = trace_analyzer_->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("name"));
  EXPECT_TRUE(trace_event);
  EXPECT_TRUE(trace_event->flow_in);
  EXPECT_TRUE(trace_event->flow_out);
}

TEST_F(JsonTraceExporterTest, TestAddFlagsJustIdFlags) {
  std::vector<FakeTraceInfo> infos = {
      // TRACE_EVENT_FLAG_HAS_ID is only parsed if it is required by the phase
      // like 'S'.
      FakeTraceInfo("name_1", "cat", 'S', /* timestamp = */ 1, /* pid = */ 2,
                    /* tid = */ 3),
      FakeTraceInfo("name_2", "cat", 'B', /* timestamp = */ 1, /* pid = */ 2,
                    /* tid = */ 3),
      FakeTraceInfo("name_3", "cat", 'B', /* timestamp = */ 1, /* pid = */ 2,
                    /* tid = */ 3),
  };
  infos[0].flags = TRACE_EVENT_FLAG_HAS_ID;
  infos[0].id = 1;
  infos[1].flags = TRACE_EVENT_FLAG_HAS_LOCAL_ID;
  infos[1].id = 2;
  infos[1].scope = "id2";
  infos[2].flags = TRACE_EVENT_FLAG_HAS_GLOBAL_ID;
  infos[2].id = 3;
  infos[2].scope = "id3";
  json_trace_exporter_->SetFakeTraceEvents(infos);
  json_trace_exporter_->OnTraceData(
      std::vector<perfetto::TracePacket>(infos.size()), false);
  EXPECT_EQ(
      "{\"traceEvents\":[{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"S\","
      "\"cat\":\"cat\",\"name\":\"name_1\",\"id\":\"0x1\",\"args\":{}},\n"
      "{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\",\"cat\":\"cat\","
      "\"name\":\"name_2\",\"scope\":\"id2\",\"id2\":{\"local\":\"0x2\"},"
      "\"args\":{}},\n"
      "{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\",\"cat\":\"cat\","
      "\"name\":\"name_3\",\"scope\":\"id3\",\"id2\":{\"global\":\"0x3\"},"
      "\"args\":{}}]}",
      unparsed_trace_data_);
  const trace_analyzer::TraceEvent* trace_event = trace_analyzer_->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("name_1"));
  ASSERT_TRUE(trace_event);
  EXPECT_EQ("0x1", trace_event->id);
  EXPECT_EQ("", trace_event->global_id2);
  EXPECT_EQ("", trace_event->local_id2);
  EXPECT_EQ("", trace_event->scope);
  const trace_analyzer::TraceEvent* trace_event_2 =
      trace_analyzer_->FindFirstOf(
          trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
          trace_analyzer::Query::String("name_2"));
  EXPECT_EQ("", trace_event_2->id);
  EXPECT_EQ("0x2", trace_event_2->local_id2);
  EXPECT_EQ("", trace_event_2->global_id2);
  EXPECT_EQ("id2", trace_event_2->scope);
  ASSERT_TRUE(trace_event);
  const trace_analyzer::TraceEvent* trace_event_3 =
      trace_analyzer_->FindFirstOf(
          trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
          trace_analyzer::Query::String("name_3"));
  ASSERT_TRUE(trace_event);
  EXPECT_EQ("", trace_event_3->id);
  EXPECT_EQ("", trace_event_3->local_id2);
  EXPECT_EQ("0x3", trace_event_3->global_id2);
  EXPECT_EQ("id3", trace_event_3->scope);
}

TEST_F(JsonTraceExporterTest, TestAddFlagsJustPhaseScope) {
  std::vector<FakeTraceInfo> infos = {
      FakeTraceInfo("name_1", "cat", 'I', /* timestamp = */ 1, /* pid = */ 2,
                    /* tid = */ 3),
      FakeTraceInfo("name_2", "cat", 'I', /* timestamp = */ 1, /* pid = */ 2,
                    /* tid = */ 3),
      FakeTraceInfo("name_3", "cat", 'I', /* timestamp = */ 1, /* pid = */ 2,
                    /* tid = */ 3),
  };
  infos[0].flags = TRACE_EVENT_SCOPE_GLOBAL;
  infos[1].flags = TRACE_EVENT_SCOPE_PROCESS;
  infos[2].flags = TRACE_EVENT_SCOPE_THREAD;
  json_trace_exporter_->SetFakeTraceEvents(infos);
  json_trace_exporter_->OnTraceData(
      std::vector<perfetto::TracePacket>(infos.size()), false);
  EXPECT_EQ(
      "{\"traceEvents\":[{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"I\","
      "\"cat\":\"cat\",\"name\":\"name_1\",\"s\":\"g\",\"args\":{}},\n"
      "{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"I\",\"cat\":\"cat\","
      "\"name\":\"name_2\",\"s\":\"p\",\"args\":{}},\n"
      "{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"I\",\"cat\":\"cat\","
      "\"name\":\"name_3\",\"s\":\"t\",\"args\":{}}]}",
      unparsed_trace_data_);
  const trace_analyzer::TraceEvent* trace_event = trace_analyzer_->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("name_1"));
  EXPECT_TRUE(trace_event);
  EXPECT_EQ(2, trace_event->thread.process_id);
  EXPECT_EQ(3, trace_event->thread.thread_id);
  EXPECT_EQ(1, trace_event->timestamp);
  EXPECT_EQ('I', trace_event->phase);
  EXPECT_EQ("name_1", trace_event->name);
  EXPECT_EQ("cat", trace_event->category);
}

TEST_F(JsonTraceExporterTest, TestAddArgs) {
  std::vector<FakeTraceInfo> infos = {FakeTraceInfo(
      "name", "cat", 'B', /* timestamp = */ 1, /* pid = */ 2, /* tid = */ 3)};
  infos[0].args.emplace_back("bool", bool(true));
  infos[0].args.emplace_back("double", double(8.0));
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("key", base::Value("value"));
  infos[0].args.emplace_back("dict", &dict);

  // We will set up this dictionary below so that all the args are easily
  // visible together.
  infos[0].args.emplace_back("json", std::string("{\"json_dict\":\"val\"}"));

  json_trace_exporter_->SetFakeTraceEvents(infos);
  json_trace_exporter_->OnTraceData(
      std::vector<perfetto::TracePacket>(infos.size()), false);
  EXPECT_EQ(
      "{\"traceEvents\":[{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\","
      "\"cat\":\"cat\",\"name\":\"name\",\"args\":{\"bool\":true"
      ",\"double\":8.000000,\"dict\":{\"key\":\"value\"}"
      ",\"json\":{\"json_dict\":\"val\"}}}]}",
      unparsed_trace_data_);
  const trace_analyzer::TraceEvent* trace_event = trace_analyzer_->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("name"));
  EXPECT_TRUE(trace_event);
  EXPECT_EQ(true, trace_event->GetKnownArgAsBool("bool"));
  EXPECT_TRUE(trace_event);
  EXPECT_EQ(8.0, trace_event->GetKnownArgAsDouble("double"));
  auto value = trace_event->GetKnownArgAsValue("json");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->is_dict());
  EXPECT_EQ("val", value->FindKey("json_dict")->GetString());
}

TEST_F(JsonTraceExporterTest, TestAddArgsArgumentStripping) {
  std::vector<FakeTraceInfo> infos = {
      FakeTraceInfo("event1", "toplevel", 'B', /* timestamp = */ 1,
                    /* pid = */ 2,
                    /* tid = */ 3),
      FakeTraceInfo("event2", "whitewashed", 'B', /* timestamp = */ 1,
                    /* pid = */ 2, /* tid = */ 3),
      FakeTraceInfo("event3", "granular_whitelisted", 'B',
                    /* timestamp = */ 1, /* pid = */ 2, /* tid = */ 3)};
  infos[0].args.emplace_back("int_one", int64_t(1));

  infos[1].args.emplace_back("int_two", uint64_t(2));

  // Third arg only has index into the string table.
  infos[2].args.emplace_back("granular_arg_whitelisted",
                             std::string("\"whitelisted_value\""));
  infos[2].args.emplace_back("granular_arg_blacklisted",
                             std::string("\"blacklisted_value\""));

  json_trace_exporter_->SetArgumentFilterForTesting(base::BindRepeating(
      [](const char* category_group_name, const char* event_name,
         base::trace_event::ArgumentNameFilterPredicate* arg_filter) {
        if (base::MatchPattern(category_group_name, "toplevel") &&
            base::MatchPattern(event_name, "*")) {
          return true;
        }
        if (base::MatchPattern(category_group_name, "granular_whitelisted") &&
            base::MatchPattern(event_name, "event3")) {
          *arg_filter = base::BindRepeating([](const char* arg_name) {
            return base::MatchPattern(arg_name, "granular_arg_whitelisted");
          });
          return true;
        }
        return false;
      }));

  json_trace_exporter_->SetFakeTraceEvents(infos);
  json_trace_exporter_->OnTraceData(
      std::vector<perfetto::TracePacket>(infos.size()), false);
  EXPECT_EQ(
      "{\"traceEvents\":[{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\","
      "\"cat\":\"toplevel\",\"name\":\"event1\",\"args\":{\"int_one\":1}},\n"
      "{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\",\"cat\":\"whitewashed\","
      "\"name\":\"event2\",\"args\":\"__stripped__\"},\n"
      "{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\","
      "\"cat\":\"granular_whitelisted\",\"name\":\"event3\","
      "\"args\":{\"granular_arg_whitelisted\":\"whitelisted_value\","
      "\"granular_arg_blacklisted\":\"__stripped__\"}}]}",
      unparsed_trace_data_);

  const trace_analyzer::TraceEvent* trace_event = trace_analyzer_->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("event1"));
  EXPECT_TRUE(trace_event);
  EXPECT_EQ(1, trace_event->GetKnownArgAsDouble("int_one"));
  trace_event = trace_analyzer_->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("event2"));
  EXPECT_FALSE(trace_event->HasArg("int_two"));
  trace_event = trace_analyzer_->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("event3"));
  EXPECT_EQ("whitelisted_value",
            trace_event->GetKnownArgAsString(("granular_arg_whitelisted")));
  EXPECT_EQ("__stripped__",
            trace_event->GetKnownArgAsString(("granular_arg_blacklisted")));
}

TEST_F(JsonTraceExporterTest, TestFtraceLegacyOutput) {
  json_trace_exporter_->legacy_ftrace_output.push_back("legacy_trace_data1");
  json_trace_exporter_->legacy_ftrace_output.push_back(",legacy\"_trace_data2");
  json_trace_exporter_->OnTraceData(std::vector<perfetto::TracePacket>(),
                                    false);
  EXPECT_EQ(
      "{\"traceEvents\":[],"
      "\"systemTraceEvents\":\"legacy_trace_data1,legacy\\\"_trace_data2\"}",
      unparsed_trace_data_);
}

TEST_F(JsonTraceExporterTest, TestLegacyJsonTrace) {
  json_trace_exporter_->json_traces.emplace_back();
  auto& trace = json_trace_exporter_->json_traces.back();
  trace.set_type(perfetto::protos::ChromeLegacyJsonTrace::USER_TRACE);
  trace.set_data(
      "{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\","
      "\"cat\":\"cat\",\"name\":\"name_1\",\"args\":{}}");
  json_trace_exporter_->json_traces.push_back(trace);
  json_trace_exporter_->json_traces.back().set_data(
      ",\n{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\","
      "\"cat\":\"cat\",\"name\":\"name_2\",\"args\":{}}");
  json_trace_exporter_->OnTraceData(std::vector<perfetto::TracePacket>(),
                                    false);
  EXPECT_EQ(
      "{\"traceEvents\":[{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\","
      "\"cat\":\"cat\",\"name\":\"name_1\",\"args\":{}},\n{\"pid\":2,"
      "\"tid\":3,\"ts\":1,\"ph\":\"B\",\"cat\":\"cat\","
      "\"name\":\"name_2\",\"args\":{}}]}",
      unparsed_trace_data_);
  const trace_analyzer::TraceEvent* trace_event = trace_analyzer_->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("name_1"));
  EXPECT_TRUE(trace_event);
  EXPECT_EQ(2, trace_event->thread.process_id);
  EXPECT_EQ(3, trace_event->thread.thread_id);
  EXPECT_EQ(1, trace_event->timestamp);
  EXPECT_EQ('B', trace_event->phase);
  EXPECT_EQ("name_1", trace_event->name);
  EXPECT_EQ("cat", trace_event->category);
  trace_event = trace_analyzer_->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("name_2"));
  EXPECT_TRUE(trace_event);
  EXPECT_EQ("name_2", trace_event->name);
}

TEST_F(JsonTraceExporterTest, TestTraceStats) {
  // Only the last trace stats matters.
  json_trace_exporter_->stats.emplace_back();
  json_trace_exporter_->stats.back().set_producers_connected(2);
  json_trace_exporter_->stats.emplace_back();
  auto& stats = json_trace_exporter_->stats.back();
  stats.set_producers_connected(4);
  auto* buf_stats = stats.add_buffer_stats();
  buf_stats->set_buffer_size(1024);
  json_trace_exporter_->OnTraceData(std::vector<perfetto::TracePacket>(),
                                    false);

  // To avoid making a brittle test we don't check all fields are outputted in
  // the trace stats just the ones we explicitly set.
  EXPECT_EQ(std::string::npos,
            unparsed_trace_data_.find("\"producers_connected\":2"));
  EXPECT_NE(std::string::npos,
            unparsed_trace_data_.find("\"producers_connected\":4"));
  EXPECT_NE(std::string::npos,
            unparsed_trace_data_.find("\"buffer_size\":1024"));
  EXPECT_TRUE(base::StartsWith(unparsed_trace_data_,
                               "{\"traceEvents\":[],"
                               "\"metadata\":{\"perfetto_trace_stats\":{\"",
                               base::CompareCase::SENSITIVE));
  EXPECT_TRUE(
      base::EndsWith(unparsed_trace_data_, "}}", base::CompareCase::SENSITIVE));
}

TEST_F(JsonTraceExporterTest, TestMetadata) {
  json_trace_exporter_->metadata.emplace_back();
  auto& m1 = json_trace_exporter_->metadata.back();
  m1.set_name("metadata_1");
  m1.set_bool_value(true);
  json_trace_exporter_->metadata.emplace_back();
  auto& m2 = json_trace_exporter_->metadata.back();
  m2.set_name("metadata_2");
  m2.set_json_value("{\"dict\":{\"bool\":true}}");
  json_trace_exporter_->OnTraceData(std::vector<perfetto::TracePacket>(),
                                    false);
  EXPECT_EQ(unparsed_trace_data_,
            "{\"traceEvents\":[],"
            "\"metadata\":{\"metadata_1\":true,"
            "\"metadata_2\":{\"dict\":{\"bool\":true}}}}");
}

TEST_F(JsonTraceExporterTest, TestMetadataFiltering) {
  json_trace_exporter_->SetMetdataFilterPredicateForTesting(
      base::BindRepeating([](const std::string& name) -> bool {
        return name.find("2") != std::string::npos;
      }));

  json_trace_exporter_->metadata.emplace_back();
  auto& m1 = json_trace_exporter_->metadata.back();
  m1.set_name("metadata_1");
  m1.set_bool_value(true);
  json_trace_exporter_->metadata.emplace_back();
  auto& m2 = json_trace_exporter_->metadata.back();
  m2.set_name("metadata_20");
  m2.set_int_value(50);
  json_trace_exporter_->metadata.emplace_back();
  auto& m3 = json_trace_exporter_->metadata.back();
  m3.set_name("metadata_21");
  m3.set_json_value("{\"dict\":{\"bool\":true}}");
  json_trace_exporter_->OnTraceData(std::vector<perfetto::TracePacket>(),
                                    false);
  EXPECT_EQ(unparsed_trace_data_,
            "{\"traceEvents\":[],"
            "\"metadata\":{\"metadata_1\":\"__stripped__\","
            "\"metadata_20\":50,"
            "\"metadata_21\":{\"dict\":{\"bool\":true}}}}");
}

TEST_F(JsonTraceExporterTest, ComplexMultipleCallback) {
  // Allocate a string that will cause the buffer to be flushed after this
  // event.
  constexpr size_t kTraceEventBufferSizeInBytes = 100 * 1024;
  std::string big_string(kTraceEventBufferSizeInBytes, 'a');
  std::vector<FakeTraceInfo> infos = {
      FakeTraceInfo(big_string, "cat", 'B',
                    /* timestamp = */ 1, /* pid = */ 2,
                    /* tid = */ 3),
      FakeTraceInfo("name", "cat", 'B',
                    /* timestamp = */ 1, /* pid = */ 2,
                    /* tid = */ 3)};
  json_trace_exporter_->SetFakeTraceEvents(infos);
  json_trace_exporter_->OnTraceData(
      std::vector<perfetto::TracePacket>(infos.size()), true);
  json_trace_exporter_->SetFakeTraceEvents({infos[1]});
  json_trace_exporter_->OnTraceData(std::vector<perfetto::TracePacket>(1),
                                    false);
  ASSERT_EQ(static_cast<std::size_t>(3), unparsed_trace_data_sequence_.size());
  EXPECT_EQ(base::StringPrintf(
                "{\"traceEvents\":[{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\","
                "\"cat\":\"cat\",\"name\":\"%s\"",
                big_string.c_str()),
            unparsed_trace_data_sequence_[0]);
  EXPECT_EQ(
      ",\"args\":{}},\n{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\","
      "\"cat\":\"cat\",\"name\":\"name\",\"args\":{}}",
      unparsed_trace_data_sequence_[1]);
  EXPECT_EQ(
      ",\n{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\","
      "\"cat\":\"cat\",\"name\":\"name\",\"args\":{}}]}",
      unparsed_trace_data_sequence_[2]);
  EXPECT_EQ(base::StringPrintf(
                "{\"traceEvents\":[{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\","
                "\"cat\":\"cat\",\"name\":\"%s\",\"args\":{}}"
                ",\n{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\","
                "\"cat\":\"cat\",\"name\":\"name\",\"args\":{}}"
                ",\n{\"pid\":2,\"tid\":3,\"ts\":1,\"ph\":\"B\","
                "\"cat\":\"cat\",\"name\":\"name\",\"args\":{}}]}",
                big_string.c_str()),
            unparsed_trace_data_);
}
}  // namespace tracing
