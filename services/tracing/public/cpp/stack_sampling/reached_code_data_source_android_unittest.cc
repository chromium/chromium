// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/reached_code_data_source_android.h"

#include <stdlib.h>

#include <limits>
#include <memory>
#include <utility>

#include "base/android/reached_code_profiler.h"
#include "base/base_switches.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "services/tracing/public/cpp/perfetto/producer_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/core/forward_decls.h"

namespace tracing {

namespace {

double BusyLoopFor(base::TimeDelta duration) {
  // Do some floating point arithmetic, since uninterruptible waits cannot be
  // profiled.
  base::TimeTicks end = base::TimeTicks::Now() + duration;
  double number = 1;
  while (base::TimeTicks::Now() < end) {
    for (int i = 0; i < 10000; ++i) {
      number *= rand() / static_cast<double>(RAND_MAX) * 2;
    }
  }
  return number;
}

class ReachedCodeDataSourceTest : public testing::Test {
 public:
  void SetUp() override {
    PerfettoTracedProcess::ResetTaskRunnerForTesting();
    PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner();

    auto perfetto_wrapper = std::make_unique<PerfettoTaskRunner>(
        task_environment_.GetMainThreadTaskRunner());

    producer_ =
        std::make_unique<TestProducerClient>(std::move(perfetto_wrapper));
  }

  void TearDown() override {
    // Be sure there is no pending/running tasks.
    task_environment_.RunUntilIdle();
  }

  void BeginTrace() {
    ReachedCodeDataSource::Get()->StartTracingWithID(
        /*data_source_id=*/1, producer_.get(), perfetto::DataSourceConfig());
  }

  void EndTracing() {
    base::RunLoop wait_for_end;
    ReachedCodeDataSource::Get()->StopTracing(wait_for_end.QuitClosure());
    wait_for_end.Run();
  }

  TestProducerClient* producer() const { return producer_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestProducerClient> producer_;
};

}  // namespace

TEST_F(ReachedCodeDataSourceTest, ProfilerDisabled) {
  BeginTrace();
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(200));
  EndTracing();
  EXPECT_EQ(producer()->GetFinalizedPacketCount(), 0u);
}

// TODO(https://crbug.com/1100216): Test crashes on android-asan.
// TODO(https://crbug.com/1122186): Test is flaky on android.
TEST_F(ReachedCodeDataSourceTest, DISABLED_ProfilerOutput) {
  if (!base::android::IsReachedCodeProfilerSupported())
    return;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableReachedCodeProfiler);
  base::android::InitReachedCodeProfilerAtStartup(
      base::android::PROCESS_BROWSER);
  ASSERT_TRUE(base::android::IsReachedCodeProfilerEnabled());
  BeginTrace();
  BusyLoopFor(base::TimeDelta::FromSeconds(2));
  EndTracing();
  EXPECT_EQ(producer()->GetFinalizedPacketCount(), 1u);
  const auto* packet = producer()->GetFinalizedPacket();
  EXPECT_TRUE(packet->has_streaming_profile_packet());
  // TODO: the profiler doesn't work in test because of ordering.
  EXPECT_GT(packet->streaming_profile_packet().callstack_iid_size(), 0);
}

}  // namespace tracing
