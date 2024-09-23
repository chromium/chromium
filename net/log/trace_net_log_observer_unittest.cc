// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/trace_net_log_observer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_impl.h"
#include "base/values.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::trace_event::TraceLog;

namespace net {

namespace {

// TraceLog category for NetLog events.
const char kNetLogTracingCategory[] = "netlog";

struct TraceEntryInfo {
  std::string category;
  // The netlog source id formatted as a hexadecimal string.
  std::string id;
  std::string phase;
  std::string name;
  std::string source_type;
};

TraceEntryInfo GetTraceEntryInfoFromValue(const base::Value::Dict& value) {
  TraceEntryInfo info;
  if (const std::string* cat = value.FindString("cat")) {
    info.category = *cat;
  } else {
    ADD_FAILURE() << "Missing 'cat'";
  }
  if (const std::string* id = value.FindString("id")) {
    info.id = *id;
  } else {
    ADD_FAILURE() << "Missing 'id'";
  }
  if (const std::string* ph = value.FindString("ph")) {
    info.phase = *ph;
  } else {
    ADD_FAILURE() << "Missing 'ph'";
  }
  if (const std::string* name = value.FindString("name")) {
    info.name = *name;
  } else {
    ADD_FAILURE() << "Missing 'name'";
  }
  if (const std::string* type =
          value.FindStringByDottedPath("args.source_type")) {
    info.source_type = *type;
  } else {
    EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_END), info.phase);
  }

  return info;
}

void EnableTraceLog(std::string_view category) {
  TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig(category, ""), TraceLog::RECORDING_MODE);
  // AsyncEnabledStateObserver will receive enabled notification one message
  // loop iteration later.
  base::RunLoop().RunUntilIdle();
}

void DisableTraceLog() {
  TraceLog::GetInstance()->SetDisabled();
  // AsyncEnabledStateObserver will receive disabled notification one message
  // loop iteration later.
  base::RunLoop().RunUntilIdle();
}

void EnableTraceLogWithNetLog() {
  EnableTraceLog(kNetLogTracingCategory);
}

void EnableTraceLogWithoutNetLog() {
  std::string disabled_netlog_category =
      std::string("-") + kNetLogTracingCategory;
  EnableTraceLog(disabled_netlog_category);
}

class TraceNetLogObserverTest : public TestWithTaskEnvironment {
 public:
  TraceNetLogObserverTest() {
    TraceLog* tracelog = TraceLog::GetInstance();
    DCHECK(tracelog);
    DCHECK(!tracelog->IsEnabled());
    trace_buffer_.SetOutputCallback(json_output_.GetCallback());
    trace_net_log_observer_ = std::make_unique<TraceNetLogObserver>();
  }

  ~TraceNetLogObserverTest() override {
    DCHECK(!TraceLog::GetInstance()->IsEnabled());
  }

  void OnTraceDataCollected(
      base::RunLoop* run_loop,
      const scoped_refptr<base::RefCountedString>& events_str,
      bool has_more_events) {
    DCHECK(trace_events_.empty());
    trace_buffer_.Start();
    trace_buffer_.AddFragment(events_str->as_string());
    trace_buffer_.Finish();

    std::optional<base::Value> trace_value;
    trace_value =
        base::JSONReader::Read(json_output_.json_output, base::JSON_PARSE_RFC);

    ASSERT_TRUE(trace_value) << json_output_.json_output;
    ASSERT_TRUE(trace_value->is_list());

    trace_events_ = FilterNetLogTraceEvents(trace_value->GetList());

    if (!has_more_events)
      run_loop->Quit();
  }

  void EndTraceAndFlush() {
    DisableTraceLog();
    base::RunLoop run_loop;
    TraceLog::GetInstance()->Flush(base::BindRepeating(
        &TraceNetLogObserverTest::OnTraceDataCollected, base::Unretained(this),
        base::Unretained(&run_loop)));
    run_loop.Run();
  }

  void set_trace_net_log_observer(
      std::unique_ptr<TraceNetLogObserver> trace_net_log_observer) {
    trace_net_log_observer_ = std::move(trace_net_log_observer);
  }

  static base::Value::List FilterNetLogTraceEvents(
      const base::Value::List& trace_events) {
    base::Value::List filtered_trace_events;

    for (const auto& event : trace_events) {
      if (!event.is_dict()) {
        ADD_FAILURE() << "Unexpected non-dictionary event in trace_events";
        continue;
      }
      const std::string* category =
          event.GetDict().FindStringByDottedPath("cat");
      if (!category) {
        ADD_FAILURE()
            << "Unexpected item without a category field in trace_events";
        continue;
      }
      if (*category != kNetLogTracingCategory)
        continue;
      filtered_trace_events.Append(event.Clone());
    }
    return filtered_trace_events;
  }

  const base::Value::List& trace_events() const { return trace_events_; }

  void clear_trace_events() {
    trace_events_.clear();
    json_output_.json_output.clear();
  }

  size_t trace_events_size() const { return trace_events_.size(); }

  RecordingNetLogObserver* net_log_observer() { return &net_log_observer_; }

  TraceNetLogObserver* trace_net_log_observer() const {
    return trace_net_log_observer_.get();
  }

 private:
  base::Value::List trace_events_;
  base::trace_event::TraceResultBuffer trace_buffer_;
  base::trace_event::TraceResultBuffer::SimpleOutput json_output_;
  RecordingNetLogObserver net_log_observer_;
  std::unique_ptr<TraceNetLogObserver> trace_net_log_observer_;
};

TEST_F(TraceNetLogObserverTest, TracingNotEnabled) {
  trace_net_log_observer()->WatchForTraceStart(NetLog::Get());
  NetLog::Get()->AddGlobalEntry(NetLogEventType::REQUEST_ALIVE);

  EndTraceAndFlush();
  trace_net_log_observer()->StopWatchForTraceStart();

  EXPECT_EQ(0u, trace_events_size());
}

TEST_F(TraceNetLogObserverTest, TraceEventCaptured) {
  auto entries = net_log_observer()->GetEntries();
  EXPECT_TRUE(entries.empty());

  trace_net_log_observer()->WatchForTraceStart(NetLog::Get());
  EnableTraceLogWithNetLog();
  NetLogWithSource net_log_with_source =
      NetLogWithSource::Make(NetLog::Get(), net::NetLogSourceType::NONE);
  NetLog::Get()->AddGlobalEntry(NetLogEventType::CANCELLED);
  net_log_with_source.BeginEvent(NetLogEventType::URL_REQUEST_START_JOB);
  net_log_with_source.EndEvent(NetLogEventType::URL_REQUEST_START_JOB);

  entries = net_log_observer()->GetEntries();
  EXPECT_EQ(3u, entries.size());
  EndTraceAndFlush();
  trace_net_log_observer()->StopWatchForTraceStart();

  EXPECT_EQ(3u, trace_events_size());

  const base::Value* item1 = &trace_events()[0];
  ASSERT_TRUE(item1->is_dict());
  TraceEntryInfo actual_item1 = GetTraceEntryInfoFromValue(item1->GetDict());
  EXPECT_EQ(kNetLogTracingCategory, actual_item1.category);
  EXPECT_EQ(base::StringPrintf("0x%x", entries[0].source.id), actual_item1.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT),
            actual_item1.phase);
  EXPECT_EQ(NetLogEventTypeToString(NetLogEventType::CANCELLED),
            actual_item1.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[0].source.type),
            actual_item1.source_type);

  const base::Value* item2 = &trace_events()[1];
  ASSERT_TRUE(item2->is_dict());
  TraceEntryInfo actual_item2 = GetTraceEntryInfoFromValue(item2->GetDict());
  EXPECT_EQ(kNetLogTracingCategory, actual_item2.category);
  EXPECT_EQ(base::StringPrintf("0x%x", entries[1].source.id), actual_item2.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN),
            actual_item2.phase);
  EXPECT_EQ(NetLogEventTypeToString(NetLogEventType::URL_REQUEST_START_JOB),
            actual_item2.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[1].source.type),
            actual_item2.source_type);

  const base::Value* item3 = &trace_events()[2];
  ASSERT_TRUE(item3->is_dict());
  TraceEntryInfo actual_item3 = GetTraceEntryInfoFromValue(item3->GetDict());
  EXPECT_EQ(kNetLogTracingCategory, actual_item3.category);
  EXPECT_EQ(base::StringPrintf("0x%x", entries[2].source.id), actual_item3.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_END),
            actual_item3.phase);
  EXPECT_EQ(NetLogEventTypeToString(NetLogEventType::URL_REQUEST_START_JOB),
            actual_item3.name);
}

TEST_F(TraceNetLogObserverTest, EnableAndDisableTracing) {
  trace_net_log_observer()->WatchForTraceStart(NetLog::Get());
  EnableTraceLogWithNetLog();
  NetLog::Get()->AddGlobalEntry(NetLogEventType::CANCELLED);
  EndTraceAndFlush();

  auto entries = net_log_observer()->GetEntries();
  EXPECT_EQ(1u, entries.size());
  EXPECT_EQ(1u, trace_events_size());
  const base::Value* item1 = &trace_events()[0];
  ASSERT_TRUE(item1->is_dict());
  TraceEntryInfo actual_item1 = GetTraceEntryInfoFromValue(item1->GetDict());
  EXPECT_EQ(kNetLogTracingCategory, actual_item1.category);
  EXPECT_EQ(base::StringPrintf("0x%x", entries[0].source.id), actual_item1.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT),
            actual_item1.phase);
  EXPECT_EQ(NetLogEventTypeToString(NetLogEventType::CANCELLED),
            actual_item1.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[0].source.type),
            actual_item1.source_type);

  clear_trace_events();

  // This entry is emitted while tracing is off.
  NetLog::Get()->AddGlobalEntry(NetLogEventType::REQUEST_ALIVE);

  EnableTraceLogWithNetLog();
  NetLog::Get()->AddGlobalEntry(NetLogEventType::URL_REQUEST_START_JOB);
  EndTraceAndFlush();
  trace_net_log_observer()->StopWatchForTraceStart();

  entries = net_log_observer()->GetEntries();
  EXPECT_EQ(3u, entries.size());
  EXPECT_EQ(1u, trace_events_size());
  const base::Value* item2 = &trace_events()[0];
  ASSERT_TRUE(item2->is_dict());
  TraceEntryInfo actual_item2 = GetTraceEntryInfoFromValue(item2->GetDict());
  EXPECT_EQ(kNetLogTracingCategory, actual_item2.category);
  EXPECT_EQ(base::StringPrintf("0x%x", entries[2].source.id), actual_item2.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT),
            actual_item2.phase);
  EXPECT_EQ(NetLogEventTypeToString(NetLogEventType::URL_REQUEST_START_JOB),
            actual_item2.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[2].source.type),
            actual_item2.source_type);
}

TEST_F(TraceNetLogObserverTest, DestroyObserverWhileTracing) {
  trace_net_log_observer()->WatchForTraceStart(NetLog::Get());
  EnableTraceLogWithNetLog();
  NetLog::Get()->AddGlobalEntry(NetLogEventType::CANCELLED);
  trace_net_log_observer()->StopWatchForTraceStart();
  set_trace_net_log_observer(nullptr);
  NetLog::Get()->AddGlobalEntry(NetLogEventType::REQUEST_ALIVE);

  EndTraceAndFlush();

  auto entries = net_log_observer()->GetEntries();
  EXPECT_EQ(2u, entries.size());
  EXPECT_EQ(1u, trace_events_size());

  const base::Value* item1 = &trace_events()[0];
  ASSERT_TRUE(item1->is_dict());

  TraceEntryInfo actual_item1 = GetTraceEntryInfoFromValue(item1->GetDict());
  EXPECT_EQ(kNetLogTracingCategory, actual_item1.category);
  EXPECT_EQ(base::StringPrintf("0x%x", entries[0].source.id), actual_item1.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT),
            actual_item1.phase);
  EXPECT_EQ(NetLogEventTypeToString(NetLogEventType::CANCELLED),
            actual_item1.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[0].source.type),
            actual_item1.source_type);
}

TEST_F(TraceNetLogObserverTest, DestroyObserverWhileNotTracing) {
  trace_net_log_observer()->WatchForTraceStart(NetLog::Get());
  NetLog::Get()->AddGlobalEntry(NetLogEventType::CANCELLED);
  trace_net_log_observer()->StopWatchForTraceStart();
  set_trace_net_log_observer(nullptr);
  NetLog::Get()->AddGlobalEntry(NetLogEventType::REQUEST_ALIVE);
  NetLog::Get()->AddGlobalEntry(NetLogEventType::URL_REQUEST_START_JOB);

  EndTraceAndFlush();

  auto entries = net_log_observer()->GetEntries();
  EXPECT_EQ(3u, entries.size());
  EXPECT_EQ(0u, trace_events_size());
}

TEST_F(TraceNetLogObserverTest, CreateObserverAfterTracingStarts) {
  set_trace_net_log_observer(nullptr);
  EnableTraceLogWithNetLog();
  set_trace_net_log_observer(std::make_unique<TraceNetLogObserver>());
  trace_net_log_observer()->WatchForTraceStart(NetLog::Get());
  NetLog::Get()->AddGlobalEntry(NetLogEventType::CANCELLED);
  trace_net_log_observer()->StopWatchForTraceStart();
  NetLog::Get()->AddGlobalEntry(NetLogEventType::REQUEST_ALIVE);
  NetLog::Get()->AddGlobalEntry(NetLogEventType::URL_REQUEST_START_JOB);

  EndTraceAndFlush();

  auto entries = net_log_observer()->GetEntries();
  EXPECT_EQ(3u, entries.size());
  EXPECT_EQ(1u, trace_events_size());
}

TEST_F(TraceNetLogObserverTest,
       CreateObserverAfterTracingStartsDisabledCategory) {
  set_trace_net_log_observer(nullptr);

  EnableTraceLogWithoutNetLog();

  set_trace_net_log_observer(std::make_unique<TraceNetLogObserver>());
  trace_net_log_observer()->WatchForTraceStart(NetLog::Get());
  NetLog::Get()->AddGlobalEntry(NetLogEventType::CANCELLED);
  trace_net_log_observer()->StopWatchForTraceStart();
  NetLog::Get()->AddGlobalEntry(NetLogEventType::REQUEST_ALIVE);
  NetLog::Get()->AddGlobalEntry(NetLogEventType::URL_REQUEST_START_JOB);

  EndTraceAndFlush();

  auto entries = net_log_observer()->GetEntries();
  EXPECT_EQ(3u, entries.size());
  EXPECT_EQ(0u, trace_events_size());
}

TEST_F(TraceNetLogObserverTest, EventsWithAndWithoutParameters) {
  trace_net_log_observer()->WatchForTraceStart(NetLog::Get());
  EnableTraceLogWithNetLog();

  NetLog::Get()->AddGlobalEntryWithStringParams(NetLogEventType::CANCELLED,
                                                "foo", "bar");
  NetLog::Get()->AddGlobalEntry(NetLogEventType::REQUEST_ALIVE);

  EndTraceAndFlush();
  trace_net_log_observer()->StopWatchForTraceStart();

  auto entries = net_log_observer()->GetEntries();
  EXPECT_EQ(2u, entries.size());
  EXPECT_EQ(2u, trace_events_size());
  const base::Value* item1 = &trace_events()[0];
  ASSERT_TRUE(item1->is_dict());
  const base::Value* item2 = &trace_events()[1];
  ASSERT_TRUE(item2->is_dict());

  TraceEntryInfo actual_item1 = GetTraceEntryInfoFromValue(item1->GetDict());
  TraceEntryInfo actual_item2 = GetTraceEntryInfoFromValue(item2->GetDict());

  EXPECT_EQ(kNetLogTracingCategory, actual_item1.category);
  EXPECT_EQ(base::StringPrintf("0x%x", entries[0].source.id), actual_item1.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT),
            actual_item1.phase);
  EXPECT_EQ(NetLogEventTypeToString(NetLogEventType::CANCELLED),
            actual_item1.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[0].source.type),
            actual_item1.source_type);

  EXPECT_EQ(kNetLogTracingCategory, actual_item2.category);
  EXPECT_EQ(base::StringPrintf("0x%x", entries[1].source.id), actual_item2.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT),
            actual_item2.phase);
  EXPECT_EQ(NetLogEventTypeToString(NetLogEventType::REQUEST_ALIVE),
            actual_item2.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[1].source.type),
            actual_item2.source_type);

  const std::string* item1_params =
      item1->GetDict().FindStringByDottedPath("args.params.foo");
  ASSERT_TRUE(item1_params);
  EXPECT_EQ("bar", *item1_params);

  // Perfetto tracing backend skips empty args.
  const base::Value::Dict* item2_args =
      item2->GetDict().FindDictByDottedPath("args");
  EXPECT_FALSE(item2_args->contains("params"));
}

TEST(TraceNetLogObserverCategoryTest, DisabledCategory) {
  base::test::TaskEnvironment task_environment;
  TraceNetLogObserver observer;
  observer.WatchForTraceStart(NetLog::Get());

  EXPECT_FALSE(NetLog::Get()->IsCapturing());

  EnableTraceLogWithoutNetLog();

  EXPECT_FALSE(NetLog::Get()->IsCapturing());
  observer.StopWatchForTraceStart();
  EXPECT_FALSE(NetLog::Get()->IsCapturing());

  DisableTraceLog();
}

TEST(TraceNetLogObserverCategoryTest, EnabledCategory) {
  base::test::TaskEnvironment task_environment;
  TraceNetLogObserver observer;
  observer.WatchForTraceStart(NetLog::Get());

  EXPECT_FALSE(NetLog::Get()->IsCapturing());

  EnableTraceLogWithNetLog();

  EXPECT_TRUE(NetLog::Get()->IsCapturing());
  observer.StopWatchForTraceStart();
  EXPECT_FALSE(NetLog::Get()->IsCapturing());

  DisableTraceLog();
}

}  // namespace

}  // namespace net
