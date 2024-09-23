// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/trace_event_analyzer.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {
class WorkerPerformanceTest : public testing::Test {
 protected:
  void SetUp() override {
    reporting_proxy_ = std::make_unique<MockWorkerReportingProxy>();
    security_origin_ = SecurityOrigin::Create(KURL("http://fake.url/"));
    worker_thread_ = std::make_unique<WorkerThreadForTest>(*reporting_proxy_);
  }
  void Mark() {
    worker_thread_->StartWithSourceCode(security_origin_.get(),
                                        "performance.mark('test_trace')");
    worker_thread_->WaitForInit();

    worker_thread_->Terminate();

    worker_thread_->WaitForShutdownForTesting();
  }
  test::TaskEnvironment task_environment_;
  std::unique_ptr<WorkerThreadForTest> worker_thread_;
  scoped_refptr<const SecurityOrigin> security_origin_;
  std::unique_ptr<MockWorkerReportingProxy> reporting_proxy_;
};

// The trace_analyzer does not work on platforms on which the migration of
// tracing into Perfetto has not completed.
TEST_F(WorkerPerformanceTest, Mark) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");

  Mark();

  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;

  Query q = Query::EventNameIs("test_trace");
  analyzer->FindEvents(q, &events);

  EXPECT_EQ(1u, events.size());

  EXPECT_EQ("blink.user_timing", events[0]->category);

  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::Value::Dict arg_dict = events[0]->GetKnownArgAsDict("data");

  std::optional<double> start_time = arg_dict.FindDouble("startTime");
  ASSERT_TRUE(start_time.has_value());

  // The navigationId is NOT recorded when performance.mark is executed by a
  // worker.
  std::string* navigation_id = arg_dict.FindString("navigationId");
  ASSERT_FALSE(navigation_id);
}

}  // namespace blink
