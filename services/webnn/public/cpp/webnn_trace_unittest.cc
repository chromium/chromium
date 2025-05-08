// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/webnn_trace.h"

#include <map>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace webnn {

class ScopedTraceTest : public testing::Test {
 public:
  ScopedTraceTest() = default;

  ~ScopedTraceTest() override = default;

  void SetUp() override {
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  }

  void TearDown() override { base::trace_event::TraceLog::ResetForTesting(); }

 protected:
  void StartTracing(const std::string& filter) {
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        base::trace_event::TraceConfig(filter,
                                       base::trace_event::RECORD_UNTIL_FULL));
  }

  static void TraceDataCb(
      base::OnceClosure quit_closure,
      base::trace_event::TraceResultBuffer::SimpleOutput* json_output,
      const scoped_refptr<base::RefCountedString>& json_events_str,
      bool has_more_events) {
    base::trace_event::TraceResultBuffer trace_buffer;
    trace_buffer.SetOutputCallback(json_output->GetCallback());
    trace_buffer.Start();
    trace_buffer.AddFragment(json_events_str->as_string());
    trace_buffer.Finish();
    if (!has_more_events) {
      std::move(quit_closure).Run();
    }
  }

  // End tracing, return tracing data in a map of event
  // name->(begin_event_counts, end_event_counts)
  absl::flat_hash_map<std::string, std::pair<int, int>> EndTracing() {
    absl::flat_hash_map<std::string, std::pair<int, int>> event_counts;
    base::trace_event::TraceResultBuffer::SimpleOutput json_data;
    base::trace_event::TraceLog::GetInstance()->SetDisabled();
    base::RunLoop run_loop;
    base::trace_event::TraceLog::GetInstance()->Flush(base::BindRepeating(
        &ScopedTraceTest::TraceDataCb, run_loop.QuitClosure(), &json_data));
    run_loop.Run();

    auto parsed_json =
        base::JSONReader::ReadAndReturnValueWithError(json_data.json_output);
    CHECK(parsed_json.has_value())
        << "JSON parsing failed (" << parsed_json.error().message
        << ") JSON data:\n"
        << json_data.json_output;

    CHECK(parsed_json->is_list());
    for (const base::Value& entry : parsed_json->GetList()) {
      const auto& dict = entry.GetDict();
      const std::string* name = dict.FindString("name");
      CHECK(name);
      const std::string* trace_type = dict.FindString("ph");
      CHECK(trace_type);
      // Count both the "BEGIN" and "END" traces.
      if (*trace_type == "n") {
        ((event_counts)[*name].first)++;
        ((event_counts)[*name].second)++;
      } else if (*trace_type != "E" && *trace_type != "e") {
        ((event_counts)[*name].first)++;
      } else {
        ((event_counts)[*name].second)++;
      }
    }
    return event_counts;
  }

  // The task runner we use for posting tasks.
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
};

TEST_F(ScopedTraceTest, SingleScopeWithoutStep) {
  {
    // Check the behavior without move. Both begin/end event should be seen.
    StartTracing("webnn");
    { ScopedTrace scoped_trace1("Method1"); }
    auto event_counts = EndTracing();

    auto [method_begins, method_ends] = event_counts.at("Method1");
    EXPECT_EQ(1, method_begins);
    EXPECT_EQ(1, method_ends);
  }

  {
    // Check the behavior with move assign. Both begin/end event should be seen.
    StartTracing("webnn");
    {
      ScopedTrace scoped_trace1("Method1");
      ScopedTrace scoped_trace2 = std::move(scoped_trace1);
    }
    auto event_counts = EndTracing();

    auto [method_begins, method_ends] = event_counts.at("Method1");
    EXPECT_EQ(1, method_begins);
    EXPECT_EQ(1, method_ends);
  }

  {
    // Check the behavior with move ctor, similar as move assign.
    StartTracing("webnn");
    {
      ScopedTrace scoped_trace1("Method1");
      ScopedTrace scoped_trace2(std::move(scoped_trace1));
    }
    auto event_counts = EndTracing();

    auto [method_begins, method_ends] = event_counts.at("Method1");
    EXPECT_EQ(1, method_begins);
    EXPECT_EQ(1, method_ends);
  }

  {
    // Move should not trigger an immediate end event.
    StartTracing("webnn");
    {
      ScopedTrace scoped_trace1("Method1");
      ScopedTrace scoped_trace2 = std::move(scoped_trace1);
      auto event_counts = EndTracing();
      auto [method_begins, method_ends] = event_counts.at("Method1");
      EXPECT_EQ(1, method_begins);
      EXPECT_EQ(0, method_ends);
    }
  }
}

// Both main trace and sub-trace should have pairing begin/end.
TEST_F(ScopedTraceTest, SingleScopeWithStep) {
  StartTracing("webnn");
  {
    ScopedTrace scoped_trace1("Method1");
    scoped_trace1.AddStep("Step1");
    ScopedTrace scoped_trace2 = std::move(scoped_trace1);
  }
  auto event_counts = EndTracing();

  auto [method_begins, method_ends] = event_counts.at("Method1");
  auto [step_begins, step_ends] = event_counts.at("Step1");
  EXPECT_EQ(1, method_begins);
  EXPECT_EQ(1, method_ends);
  EXPECT_EQ(1, step_begins);
  EXPECT_EQ(1, step_ends);
}

// Multiple steps should results in multiple begin/end pairs.
TEST_F(ScopedTraceTest, MultipleAddSteps) {
  StartTracing("webnn");
  {
    ScopedTrace scoped_trace1("Method1");
    scoped_trace1.AddStep("Step1");
    scoped_trace1.AddStep("Step2");
    ScopedTrace scoped_trace2(std::move(scoped_trace1));
    scoped_trace2.AddStep("Step3");
  }
  auto event_counts = EndTracing();

  auto [method1_begins, method1_ends] = event_counts.at("Method1");
  auto [step1_begins, step1_ends] = event_counts.at("Step1");
  auto [step2_begins, step2_ends] = event_counts.at("Step2");
  auto [step3_begins, step3_ends] = event_counts.at("Step3");
  EXPECT_EQ(1, method1_begins);
  EXPECT_EQ(1, method1_ends);
  EXPECT_EQ(1, step1_begins);
  EXPECT_EQ(1, step1_ends);
  EXPECT_EQ(1, step2_begins);
  EXPECT_EQ(1, step2_ends);
  EXPECT_EQ(1, step3_begins);
  EXPECT_EQ(1, step3_ends);
}

// Nesting top-level traces should have pairing begin/end.
TEST_F(ScopedTraceTest, MultipleNestedTraces) {
  StartTracing("webnn");
  {
    ScopedTrace scoped_trace1("Method1");
    { ScopedTrace scoped_trace2("Method2"); }
  }
  auto event_counts = EndTracing();

  auto [method1_begins, method1_ends] = event_counts.at("Method1");
  auto [method2_begins, method2_ends] = event_counts.at("Method2");
  EXPECT_EQ(1, method1_begins);
  EXPECT_EQ(1, method1_ends);
  EXPECT_EQ(1, method2_begins);
  EXPECT_EQ(1, method2_ends);
}

// Trace handle should be passed correct across function boundaries.
TEST_F(ScopedTraceTest, PassScopedTraceToFunc) {
  {
    // Pass to another function that does not add extra step.
    StartTracing("webnn");
    ScopedTrace scoped_trace1("Method1");
    ([](ScopedTrace trace) {})(std::move(scoped_trace1));
    auto event_counts = EndTracing();

    auto [method_begins, method_ends] = event_counts.at("Method1");
    method_ends = event_counts["Method1"].second;
    EXPECT_EQ(1, method_begins);
    EXPECT_EQ(1, method_ends);
  }

  {
    // Pass to another function call that adds extra step.
    StartTracing("webnn");
    ScopedTrace scoped_trace2("Method1");
    ([](ScopedTrace trace) { trace.AddStep("Step1"); })(
        std::move(scoped_trace2));
    auto event_counts = EndTracing();

    auto [method_begins, method_ends] = event_counts.at("Method1");
    auto [step_begins, step_ends] = event_counts.at("Step1");
    EXPECT_EQ(1, method_begins);
    EXPECT_EQ(1, method_ends);
    EXPECT_EQ(1, step_begins);
    EXPECT_EQ(1, step_ends);
  }
}

TEST_F(ScopedTraceTest, WorksWithBindOnce) {
  {
    // Invoke BindOnce without adding extra step.
    StartTracing("webnn");
    ScopedTrace scoped_trace1("Method1");
    test_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](ScopedTrace trace) {}, std::move(scoped_trace1)));
    test_task_runner_->RunUntilIdle();
    auto event_counts = EndTracing();

    auto [method_begins, method_ends] = event_counts.at("Method1");
    EXPECT_EQ(1, method_begins);
    EXPECT_EQ(1, method_ends);
  }

  {
    // Invoke BindOnce and add extra step.
    StartTracing("webnn");
    ScopedTrace scoped_trace2("Method1");
    test_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](ScopedTrace trace) { trace.AddStep("Step1"); },
                       std::move(scoped_trace2)));
    test_task_runner_->RunUntilIdle();
    auto event_counts = EndTracing();

    auto [method_begins, method_ends] = event_counts.at("Method1");
    auto [step_begins, step_ends] = event_counts.at("Step1");
    EXPECT_EQ(1, method_begins);
    EXPECT_EQ(1, method_ends);
    EXPECT_EQ(1, step_begins);
    EXPECT_EQ(1, step_ends);
  }
}

}  // namespace webnn
