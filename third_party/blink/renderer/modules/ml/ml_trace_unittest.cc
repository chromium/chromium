// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_trace.h"

#include <map>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class ScopedMLTraceTest : public testing::Test {
 public:
  ScopedMLTraceTest() = default;

  ~ScopedMLTraceTest() override = default;

  void SetUp() override {
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  }

  void TearDown() override { base::trace_event::TraceLog::ResetForTesting(); }

 protected:
  void StartTracing(const std::string& filter) {
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        base::trace_event::TraceConfig(filter,
                                       base::trace_event::RECORD_UNTIL_FULL),
        base::trace_event::TraceLog::RECORDING_MODE);
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
  std::map<std::string, std::pair<int, int>> EndTracing() {
    std::map<std::string, std::pair<int, int>> event_counts;
    base::trace_event::TraceResultBuffer::SimpleOutput json_data;
    base::trace_event::TraceLog::GetInstance()->SetDisabled();
    base::RunLoop run_loop;
    base::trace_event::TraceLog::GetInstance()->Flush(base::BindRepeating(
        &ScopedMLTraceTest::TraceDataCb, run_loop.QuitClosure(), &json_data));
    run_loop.Run();

    auto parsed_json =
        base::JSONReader::ReadAndReturnValueWithError(json_data.json_output);
    CHECK(parsed_json.has_value())
        << "JSON parsing failed (" << parsed_json.error().message
        << ") JSON data:" << std::endl
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
  test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
};

TEST_F(ScopedMLTraceTest, SingleScopeWithoutStep) {
  {
    // Check the behavior without move. Both begin/end event should be seen.
    StartTracing("webnn");
    { ScopedMLTrace scoped_trace1("Method1"); }
    auto event_counts = EndTracing();

    auto [method_begins, method_ends] = event_counts.at("Method1");
    EXPECT_EQ(1, method_begins);
    EXPECT_EQ(1, method_ends);
  }

  {
    // Check the behavior with move assign. Both begin/end event should be seen.
    StartTracing("webnn");
    {
      ScopedMLTrace scoped_trace1("Method1");
      ScopedMLTrace scoped_trace2 = std::move(scoped_trace1);
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
      ScopedMLTrace scoped_trace1("Method1");
      ScopedMLTrace scoped_trace2(std::move(scoped_trace1));
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
      ScopedMLTrace scoped_trace1("Method1");
      ScopedMLTrace scoped_trace2 = std::move(scoped_trace1);
      auto event_counts = EndTracing();
      auto [method_begins, method_ends] = event_counts.at("Method1");
      EXPECT_EQ(1, method_begins);
      EXPECT_EQ(0, method_ends);
    }
  }
}

// Both main trace and sub-trace should have pairing begin/end.
TEST_F(ScopedMLTraceTest, SingleScopeWithStep) {
  StartTracing("webnn");
  {
    ScopedMLTrace scoped_trace1("Method1");
    scoped_trace1.AddStep("Step1");
    ScopedMLTrace scoped_trace2 = std::move(scoped_trace1);
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
TEST_F(ScopedMLTraceTest, MultipleAddSteps) {
  StartTracing("webnn");
  {
    ScopedMLTrace scoped_trace1("Method1");
    scoped_trace1.AddStep("Step1");
    scoped_trace1.AddStep("Step2");
    ScopedMLTrace scoped_trace2(std::move(scoped_trace1));
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
TEST_F(ScopedMLTraceTest, MultipleNestedTraces) {
  StartTracing("webnn");
  {
    ScopedMLTrace scoped_trace1("Method1");
    { ScopedMLTrace scoped_trace2("Method2"); }
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
TEST_F(ScopedMLTraceTest, PassScopedTraceToFunc) {
  {
    // Pass to another function that does not add extra step.
    StartTracing("webnn");
    ScopedMLTrace scoped_trace1("Method1");
    ([](ScopedMLTrace trace) {})(std::move(scoped_trace1));
    auto event_counts = EndTracing();

    auto [method_begins, method_ends] = event_counts.at("Method1");
    method_ends = event_counts["Method1"].second;
    EXPECT_EQ(1, method_begins);
    EXPECT_EQ(1, method_ends);
  }

  {
    // Pass to another function call that adds extra step.
    StartTracing("webnn");
    ScopedMLTrace scoped_trace2("Method1");
    ([](ScopedMLTrace trace) { trace.AddStep("Step1"); })(
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

// Trace handle should be passed correctly by posting tasks.
TEST_F(ScopedMLTraceTest, WorksWithPostCrossThreadTask) {
  {
    // Post to another thread that does not add extra step.
    StartTracing("webnn");
    ScopedMLTrace scoped_trace1("Method1");
    PostCrossThreadTask(*test_task_runner_, FROM_HERE,
                        CrossThreadBindOnce([](ScopedMLTrace trace) {},
                                            std::move(scoped_trace1)));
    test_task_runner_->RunUntilIdle();
    auto event_counts = EndTracing();

    auto [method_begins, method_ends] = event_counts.at("Method1");
    EXPECT_EQ(1, method_begins);
    EXPECT_EQ(1, method_ends);
  }

  {
    // Post to another thread that adds extra step.
    base::trace_event::TraceLog::ResetForTesting();
    StartTracing("webnn");
    ScopedMLTrace scoped_trace2("Method1");
    PostCrossThreadTask(
        *test_task_runner_, FROM_HERE,
        CrossThreadBindOnce([](ScopedMLTrace trace) { trace.AddStep("Step1"); },
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

  {
    // Add step first, and post to another thread without adding step.
    base::trace_event::TraceLog::ResetForTesting();
    StartTracing("webnn");
    ScopedMLTrace scoped_trace3("Method1");
    scoped_trace3.AddStep("Step1");
    PostCrossThreadTask(*test_task_runner_, FROM_HERE,
                        CrossThreadBindOnce([](ScopedMLTrace trace) {},
                                            std::move(scoped_trace3)));
    test_task_runner_->RunUntilIdle();
    auto event_counts = EndTracing();

    auto [method_begins, method_ends] = event_counts.at("Method1");
    auto [step_begins, step_ends] = event_counts.at("Step1");
    EXPECT_EQ(1, method_begins);
    EXPECT_EQ(1, method_ends);
    EXPECT_EQ(1, step_begins);
    EXPECT_EQ(1, step_ends);
  }

  {
    // Add step first, and post to another thread that adds step.
    base::trace_event::TraceLog::ResetForTesting();
    StartTracing("webnn");
    ScopedMLTrace scoped_trace4("Method1");
    scoped_trace4.AddStep("Step1");
    PostCrossThreadTask(
        *test_task_runner_, FROM_HERE,
        CrossThreadBindOnce([](ScopedMLTrace trace) { trace.AddStep("Step2"); },
                            std::move(scoped_trace4)));
    test_task_runner_->RunUntilIdle();
    auto event_counts = EndTracing();

    auto [method_begins, method_ends] = event_counts.at("Method1");
    auto [step_begins, step_ends] = event_counts.at("Step1");
    auto [step_in_task_begins, step_in_task_ends] = event_counts["Step2"];
    EXPECT_EQ(1, method_begins);
    EXPECT_EQ(1, method_ends);
    EXPECT_EQ(1, step_begins);
    EXPECT_EQ(1, step_ends);
    EXPECT_EQ(1, step_in_task_begins);
    EXPECT_EQ(1, step_in_task_ends);
  }
}

TEST_F(ScopedMLTraceTest, WorksWithBindOnce) {
  {
    // Invoke BindOnce without adding extra step.
    StartTracing("webnn");
    ScopedMLTrace scoped_trace1("Method1");
    test_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](ScopedMLTrace trace) {}, std::move(scoped_trace1)));
    test_task_runner_->RunUntilIdle();
    auto event_counts = EndTracing();

    auto [method_begins, method_ends] = event_counts.at("Method1");
    EXPECT_EQ(1, method_begins);
    EXPECT_EQ(1, method_ends);
  }

  {
    // Invoke BindOnce and add extra step.
    StartTracing("webnn");
    ScopedMLTrace scoped_trace2("Method1");
    test_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](ScopedMLTrace trace) { trace.AddStep("Step1"); },
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

}  // namespace blink
