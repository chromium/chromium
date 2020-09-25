// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"

#include <limits>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/buildflags.h"
#include "services/tracing/public/cpp/perfetto/producer_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
#include "base/test/trace_event_analyzer.h"
#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampler_win.h"
#endif

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace tracing {
namespace {

using base::trace_event::TraceLog;
using ::testing::Invoke;
using ::testing::Return;

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)

class MockLoaderLockSampler : public TracingSamplerProfiler::LoaderLockSampler {
 public:
  MockLoaderLockSampler() = default;
  ~MockLoaderLockSampler() override = default;

  MOCK_METHOD(bool, IsLoaderLockHeld, (), (const, override));
};

class LoaderLockEventAnalyzer {
 public:
  LoaderLockEventAnalyzer() {
    trace_analyzer::Start(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"));
  }

  size_t CountEvents() {
    std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer =
        trace_analyzer::Stop();
    trace_analyzer::TraceEventVector events;
    return analyzer->FindEvents(
        trace_analyzer::Query::EventName() ==
            trace_analyzer::Query::String(
                TracingSamplerProfiler::kLoaderLockHeldEventName),
        &events);
  }
};

#endif  // BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)

class TracingSampleProfilerTest : public testing::Test {
 public:
  TracingSampleProfilerTest() = default;
  ~TracingSampleProfilerTest() override = default;

  void SetUp() override {
    events_stack_received_count_ = 0u;
    PerfettoTracedProcess::ResetTaskRunnerForTesting();
    PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner();

    auto perfetto_wrapper = std::make_unique<PerfettoTaskRunner>(
        task_environment_.GetMainThreadTaskRunner());

    producer_ =
        std::make_unique<TestProducerClient>(std::move(perfetto_wrapper),
                                             /*log_only_main_thread=*/false);

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
    ON_CALL(mock_loader_lock_sampler_, IsLoaderLockHeld())
        .WillByDefault(Return(false));
    TracingSamplerProfiler::SetLoaderLockSamplerForTesting(
        &mock_loader_lock_sampler_);
#endif
  }

  void TearDown() override {
    // Be sure there is no pending/running tasks.
    task_environment_.RunUntilIdle();

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
    TracingSamplerProfiler::SetLoaderLockSamplerForTesting(nullptr);
#endif
  }

  void BeginTrace() {
    TracingSamplerProfiler::StartTracingForTesting(producer_.get());
  }

  void WaitForEvents() {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(200));
  }

  void EndTracing() {
    TracingSamplerProfiler::StopTracingForTesting();
    base::RunLoop().RunUntilIdle();

    auto& packets = producer_->finalized_packets();
    for (auto& packet : packets) {
      if (packet->has_streaming_profile_packet()) {
        events_stack_received_count_++;
      }
    }
  }

  void ValidateReceivedEvents() {
    if (TracingSamplerProfiler::IsStackUnwindingSupported()) {
      EXPECT_GT(events_stack_received_count_, 0U);
    } else {
      EXPECT_EQ(events_stack_received_count_, 0U);
    }
  }

  uint32_t FindProfilerSequenceId() {
    uint32_t profile_sequence_id = std::numeric_limits<uint32_t>::max();
    auto& packets = producer_->finalized_packets();
    for (auto& packet : packets) {
      if (packet->has_streaming_profile_packet()) {
        profile_sequence_id = packet->trusted_packet_sequence_id();
        break;
      }
    }
    EXPECT_NE(profile_sequence_id, std::numeric_limits<uint32_t>::max());
    return profile_sequence_id;
  }

  const TestProducerClient* producer() const { return producer_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_;

  // We want our singleton torn down after each test.
  base::ShadowingAtExitManager at_exit_manager_;
  base::trace_event::TraceResultBuffer trace_buffer_;

  std::unique_ptr<TestProducerClient> producer_;

  // Number of stack sampling events received.
  size_t events_stack_received_count_ = 0;

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
  MockLoaderLockSampler mock_loader_lock_sampler_;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(TracingSampleProfilerTest);
};

// Stub module for testing.
class TestModule : public base::ModuleCache::Module {
 public:
  TestModule() = default;

  TestModule(const TestModule&) = delete;
  TestModule& operator=(const TestModule&) = delete;

  void set_id(const std::string& id) { id_ = id; }
  uintptr_t GetBaseAddress() const override { return 0; }
  std::string GetId() const override { return id_; }
  base::FilePath GetDebugBasename() const override { return base::FilePath(); }
  size_t GetSize() const override { return 0; }
  bool IsNative() const override { return true; }

 private:
  std::string id_;
};

bool ShouldSkipTestForMacOS11() {
#if defined(OS_MAC)
  // The sampling profiler does not work on macOS 11 and is disabled.
  // See https://crbug.com/1101399 and https://crbug.com/1098119.
  // DCHECK here so that when the sampling profiler is re-enabled on macOS 11,
  // these tests are also re-enabled.
  if (base::mac::IsAtLeastOS11()) {
    DCHECK(!base::StackSamplingProfiler::IsSupportedForCurrentPlatform());
    return true;
  }
#endif
  return false;
}

}  // namespace

TEST_F(TracingSampleProfilerTest, OnSampleCompleted) {
  if (ShouldSkipTestForMacOS11())
    GTEST_SKIP() << "Stack sampler is not supported on macOS 11";
  auto profiler = TracingSamplerProfiler::CreateOnMainThread();
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();
  ValidateReceivedEvents();
}

TEST_F(TracingSampleProfilerTest, JoinRunningTracing) {
  if (ShouldSkipTestForMacOS11())
    GTEST_SKIP() << "Stack sampler is not supported on macOS 11";
  BeginTrace();
  auto profiler = TracingSamplerProfiler::CreateOnMainThread();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();
  ValidateReceivedEvents();
}

TEST_F(TracingSampleProfilerTest, TestStartupTracing) {
  if (ShouldSkipTestForMacOS11())
    GTEST_SKIP() << "Stack sampler is not supported on macOS 11";
  auto profiler = TracingSamplerProfiler::CreateOnMainThread();
  TracingSamplerProfiler::SetupStartupTracingForTesting();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  auto start_tracing_ts = TRACE_TIME_TICKS_NOW();
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();
  if (TracingSamplerProfiler::IsStackUnwindingSupported()) {
    uint32_t seq_id = FindProfilerSequenceId();
    auto& packets = producer()->finalized_packets();
    int64_t reference_ts = 0;
    int64_t first_profile_ts = 0;
    for (auto& packet : packets) {
      if (packet->trusted_packet_sequence_id() == seq_id) {
        if (packet->has_thread_descriptor()) {
          reference_ts = packet->thread_descriptor().reference_timestamp_us();
        } else if (packet->has_streaming_profile_packet()) {
          first_profile_ts =
              reference_ts +
              packet->streaming_profile_packet().timestamp_delta_us(0);
          break;
        }
      }
    }
    // Expect first sample before tracing started.
    EXPECT_LT(first_profile_ts,
              start_tracing_ts.since_origin().InMicroseconds());
  }
}

TEST_F(TracingSampleProfilerTest, JoinStartupTracing) {
  if (ShouldSkipTestForMacOS11())
    GTEST_SKIP() << "Stack sampler is not supported on macOS 11";
  TracingSamplerProfiler::SetupStartupTracingForTesting();
  base::RunLoop().RunUntilIdle();
  auto profiler = TracingSamplerProfiler::CreateOnMainThread();
  WaitForEvents();
  auto start_tracing_ts = TRACE_TIME_TICKS_NOW();
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();
  if (TracingSamplerProfiler::IsStackUnwindingSupported()) {
    uint32_t seq_id = FindProfilerSequenceId();
    auto& packets = producer()->finalized_packets();
    int64_t reference_ts = 0;
    int64_t first_profile_ts = 0;
    for (auto& packet : packets) {
      if (packet->trusted_packet_sequence_id() == seq_id) {
        if (packet->has_thread_descriptor()) {
          reference_ts = packet->thread_descriptor().reference_timestamp_us();
        } else if (packet->has_streaming_profile_packet()) {
          first_profile_ts =
              reference_ts +
              packet->streaming_profile_packet().timestamp_delta_us(0);
          break;
        }
      }
    }
    // Expect first sample before tracing started.
    EXPECT_LT(first_profile_ts,
              start_tracing_ts.since_origin().InMicroseconds());
  }
}

TEST_F(TracingSampleProfilerTest, SamplingChildThread) {
  if (ShouldSkipTestForMacOS11())
    GTEST_SKIP() << "Stack sampler is not supported on macOS 11";
  base::Thread sampled_thread("sampling_profiler_test");
  sampled_thread.Start();
  sampled_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TracingSamplerProfiler::CreateOnChildThread));
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  ValidateReceivedEvents();
  sampled_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TracingSamplerProfiler::DeleteOnChildThreadForTesting));
  base::RunLoop().RunUntilIdle();
}

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)

TEST_F(TracingSampleProfilerTest, SampleLoaderLockOnMainThread) {
  if (ShouldSkipTestForMacOS11())
    GTEST_SKIP() << "Stack sampler is not supported on macOS 11";
  LoaderLockEventAnalyzer event_analyzer;

  bool lock_held = false;
  size_t call_count = 0;
  EXPECT_CALL(mock_loader_lock_sampler_, IsLoaderLockHeld())
      .WillRepeatedly(Invoke([&lock_held, &call_count]() {
        ++call_count;
        lock_held = !lock_held;
        return lock_held;
      }));

  auto profiler = TracingSamplerProfiler::CreateOnMainThread();
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();

  // Since the loader lock state changed each time it was sampled an event
  // should be emitted each time.
  EXPECT_EQ(event_analyzer.CountEvents(), call_count);

  // Loader lock should have been sampled every time the stack is sampled,
  // although not every stack sample generates a stack event.
  EXPECT_GE(call_count, events_stack_received_count_);
}

TEST_F(TracingSampleProfilerTest, SampleLoaderLockAlwaysHeld) {
  if (ShouldSkipTestForMacOS11())
    GTEST_SKIP() << "Stack sampler is not supported on macOS 11";
  LoaderLockEventAnalyzer event_analyzer;

  EXPECT_CALL(mock_loader_lock_sampler_, IsLoaderLockHeld())
      .WillRepeatedly(Return(true));

  auto profiler = TracingSamplerProfiler::CreateOnMainThread();
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();

  // An event should be emitted at the first sample when the loader lock was
  // held, and then not again since the state never changed.
  EXPECT_EQ(event_analyzer.CountEvents(), 1U);
}

TEST_F(TracingSampleProfilerTest, SampleLoaderLockNeverHeld) {
  if (ShouldSkipTestForMacOS11())
    GTEST_SKIP() << "Stack sampler is not supported on macOS 11";
  LoaderLockEventAnalyzer event_analyzer;

  EXPECT_CALL(mock_loader_lock_sampler_, IsLoaderLockHeld())
      .WillRepeatedly(Return(false));

  auto profiler = TracingSamplerProfiler::CreateOnMainThread();
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();

  // No events should be emitted since the lock is never held.
  EXPECT_EQ(event_analyzer.CountEvents(), 0U);
}

TEST_F(TracingSampleProfilerTest, SampleLoaderLockOnChildThread) {
  if (ShouldSkipTestForMacOS11())
    GTEST_SKIP() << "Stack sampler is not supported on macOS 11";
  LoaderLockEventAnalyzer event_analyzer;

  // Loader lock should only be sampled on main thread.
  EXPECT_CALL(mock_loader_lock_sampler_, IsLoaderLockHeld()).Times(0);

  base::Thread sampled_thread("sampling_profiler_test");
  sampled_thread.Start();
  sampled_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TracingSamplerProfiler::CreateOnChildThread));
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  sampled_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TracingSamplerProfiler::DeleteOnChildThreadForTesting));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(event_analyzer.CountEvents(), 0U);
}

TEST_F(TracingSampleProfilerTest, SampleLoaderLockWithoutMock) {
  if (ShouldSkipTestForMacOS11())
    GTEST_SKIP() << "Stack sampler is not supported on macOS 11";
  // Use the real loader lock sampler. This tests that it is initialized
  // correctly in TracingSamplerProfiler.
  TracingSamplerProfiler::SetLoaderLockSamplerForTesting(nullptr);

  auto profiler = TracingSamplerProfiler::CreateOnMainThread();
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();

  // The loader lock may or may not be held during the test, so there's no
  // output to test. The test passes if it reaches the end without crashing.
}

#endif  // BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)

class TracingProfileBuilderTest : public testing::Test {
 public:
  void SetUp() override {
    auto perfetto_wrapper = std::make_unique<PerfettoTaskRunner>(
        task_environment_.GetMainThreadTaskRunner());
    producer_client_ = std::make_unique<TestProducerClient>(
        std::move(perfetto_wrapper), /*log_only_main_thread=*/false);
  }

  void TearDown() override { producer_client_.reset(); }

  TestProducerClient* producer() { return producer_client_.get(); }

 private:
  // Should be the first member.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestProducerClient> producer_client_;
};

TEST_F(TracingProfileBuilderTest, ValidModule) {
  TestModule module;
  TracingSamplerProfiler::TracingProfileBuilder profile_builder(
      base::PlatformThreadId(), std::make_unique<TestTraceWriter>(producer()),
      false);
  profile_builder.OnSampleCompleted({base::Frame(0x1010, &module)},
                                    base::TimeTicks());
}

TEST_F(TracingProfileBuilderTest, InvalidModule) {
  TracingSamplerProfiler::TracingProfileBuilder profile_builder(
      base::PlatformThreadId(), std::make_unique<TestTraceWriter>(producer()),
      false);
  profile_builder.OnSampleCompleted({base::Frame(0x1010, nullptr)},
                                    base::TimeTicks());
}

#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
TEST_F(TracingProfileBuilderTest, MangleELFModuleID) {
  TestModule module;
  // See explanation for the module_id mangling in
  // TracingSamplerProfiler::TracingProfileBuilder::GetCallstackIDAndMaybeEmit.
  module.set_id("7F0715C286F8B16C10E4AD349CDA3B9B56C7A773");

  TracingSamplerProfiler::TracingProfileBuilder profile_builder(
      base::PlatformThreadId(), std::make_unique<TestTraceWriter>(producer()),
      false);
  profile_builder.OnSampleCompleted({base::Frame(0x1010, &module)},
                                    base::TimeTicks());
  producer()->FlushPacketIfPossible();

  bool found_build_id = false;
  for (unsigned i = 0; i < producer()->GetFinalizedPacketCount(); ++i) {
    const perfetto::protos::TracePacket* packet =
        producer()->GetFinalizedPacket(i);
    if (!packet->has_interned_data() ||
        packet->interned_data().build_ids_size() == 0) {
      return;
    }

    found_build_id = true;
    EXPECT_EQ(packet->interned_data().build_ids(0).str(),
              "C215077FF8866CB110E4AD349CDA3B9B0");
  }
  EXPECT_TRUE(found_build_id);
}
#endif

}  // namespace tracing
