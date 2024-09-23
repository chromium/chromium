// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "base/at_exit.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/profiler/frame.h"
#include "base/profiler/module_cache.h"
#include "base/profiler/register_context.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/stack_sampling_profiler_test_util.h"
#include "base/profiler/unwinder.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "services/tracing/perfetto/test_utils.h"
#include "services/tracing/public/cpp/buildflags.h"
#include "services/tracing/public/cpp/perfetto/producer_test_utils.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
#include "base/test/trace_event_analyzer.h"
#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampler_win.h"
#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampling_thread_win.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace tracing {
namespace {

using ::base::trace_event::TraceLog;
using ::perfetto::protos::TracePacket;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;
using PacketVector = TestProducerClient::PacketVector;

std::unique_ptr<perfetto::TracingSession> g_tracing_session;

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)

class MockLoaderLockSampler : public LoaderLockSampler {
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
                LoaderLockSamplingThread::kLoaderLockHeldEventName),
        &events);
  }
};

#endif  // BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)

class TracingSampleProfilerTest : public testing::Test {
 public:
  TracingSampleProfilerTest() = default;

  TracingSampleProfilerTest(const TracingSampleProfilerTest&) = delete;
  TracingSampleProfilerTest& operator=(const TracingSampleProfilerTest&) =
      delete;

  ~TracingSampleProfilerTest() override = default;

  void SetUp() override {

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
    // Override the default LoaderLockSampler because in production it is
    // expected to be called from a single thread, and each test may re-create
    // the sampling thread.
    ON_CALL(mock_loader_lock_sampler_, IsLoaderLockHeld())
        .WillByDefault(Return(false));
    LoaderLockSamplingThread::SetLoaderLockSamplerForTesting(
        &mock_loader_lock_sampler_);
#endif

    events_stack_received_count_ = 0u;

    PerfettoTracedProcess::GetTaskRunner()->ResetTaskRunnerForTesting(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    TracingSamplerProfiler::ResetDataSourceForTesting();
  }

  void TearDown() override {
#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
    LoaderLockSamplingThread::SetLoaderLockSamplerForTesting(nullptr);
#endif
  }

  void BeginTrace() {
    perfetto::TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(1024);
    auto* ds_cfg = trace_config.add_data_sources()->mutable_config();
    ds_cfg->set_name(mojom::kSamplerProfilerSourceName);
    ds_cfg = trace_config.add_data_sources()->mutable_config();
    ds_cfg->set_name("track_event");

    g_tracing_session = perfetto::Tracing::NewTrace();
    g_tracing_session->Setup(trace_config);
    g_tracing_session->StartBlocking();
    // Make sure TraceEventMetadataSource::StartTracingImpl gets run.
    base::RunLoop().RunUntilIdle();
  }

  void WaitForEvents() { base::PlatformThread::Sleep(base::Milliseconds(200)); }

  void EnsureTraceStopped() {
    if (!g_tracing_session)
      return;

    base::TrackEvent::Flush();

    base::RunLoop wait_for_stop;
    g_tracing_session->SetOnStopCallback(
        [&wait_for_stop] { wait_for_stop.Quit(); });
    g_tracing_session->Stop();
    wait_for_stop.Run();

    std::vector<char> serialized_data = g_tracing_session->ReadTraceBlocking();
    g_tracing_session.reset();

    perfetto::protos::Trace trace;
    EXPECT_TRUE(
        trace.ParseFromArray(serialized_data.data(), serialized_data.size()));
    for (const auto& packet : trace.packet()) {
      auto proto = std::make_unique<perfetto::protos::TracePacket>();
      *proto = packet;
      finalized_packets_.push_back(std::move(proto));
    }
  }

  const PacketVector& GetFinalizedPackets() {
    EnsureTraceStopped();
    return finalized_packets_;
  }

  void EndTracing() {
    EnsureTraceStopped();
    auto& packets = GetFinalizedPackets();
    for (auto& packet : packets) {
      if (packet->has_streaming_profile_packet()) {
        events_stack_received_count_++;
      }
    }
  }

  void ValidateReceivedEvents() {
    if (TracingSamplerProfiler::IsStackUnwindingSupportedForTesting()) {
      EXPECT_GT(events_stack_received_count_, 0U);
    } else {
      EXPECT_EQ(events_stack_received_count_, 0U);
    }
  }

  uint32_t FindProfilerSequenceId() {
    uint32_t profile_sequence_id = std::numeric_limits<uint32_t>::max();
    auto& packets = GetFinalizedPackets();
    for (auto& packet : packets) {
      if (packet->has_streaming_profile_packet()) {
        profile_sequence_id = packet->trusted_packet_sequence_id();
        break;
      }
    }
    EXPECT_NE(profile_sequence_id, std::numeric_limits<uint32_t>::max());
    return profile_sequence_id;
  }

 protected:
  // We want our singleton torn down after each test.
  base::ShadowingAtExitManager at_exit_manager_;
  base::trace_event::TraceResultBuffer trace_buffer_;

  base::test::TaskEnvironment task_environment_;
  base::test::TracingEnvironment tracing_environment_;
  std::vector<std::unique_ptr<perfetto::protos::TracePacket>>
      finalized_packets_;

  // Number of stack sampling events received.
  size_t events_stack_received_count_ = 0;

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
  MockLoaderLockSampler mock_loader_lock_sampler_;
#endif
};

class MockUnwinder : public base::Unwinder {
 public:
  MOCK_CONST_METHOD1(CanUnwindFrom, bool(const base::Frame& current_frame));
  MOCK_METHOD4(TryUnwind,
               base::UnwindResult(base::UnwinderStateCapture* capture_state,
                                  base::RegisterContext* thread_context,
                                  uintptr_t stack_top,
                                  std::vector<base::Frame>* stack));
};

// Note that this is relevant only for Android, since TracingSamplingProfiler
// ignores any provided unwinder factory for non-Android platforms:
// https://source.chromium.org/chromium/chromium/src/+/main:services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.cc;l=905-908;drc=70d839a3b8bcf1ef43c42a54a4b27f14ee149750
base::StackSamplingProfiler::UnwindersFactory
MakeMockUnwinderFactoryWithExpectations() {
  if (!TracingSamplerProfiler::IsStackUnwindingSupportedForTesting()) {
    return base::StackSamplingProfiler::UnwindersFactory();
  }
  return base::BindOnce([] {
    auto mock_unwinder = std::make_unique<MockUnwinder>();
    EXPECT_CALL(*mock_unwinder, CanUnwindFrom(_))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_unwinder, TryUnwind(_, _, _, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(base::UnwindResult::kCompleted));

    std::vector<std::unique_ptr<base::Unwinder>> mock_unwinders;
    mock_unwinders.push_back(std::move(mock_unwinder));
    return mock_unwinders;
  });
}

}  // namespace

TEST_F(TracingSampleProfilerTest, OnSampleCompleted) {
  auto profiler =
      TracingSamplerProfiler::CreateOnMainThread(base::BindRepeating(
          [] { return MakeMockUnwinderFactoryWithExpectations(); }));
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();
  ValidateReceivedEvents();
}

// This is needed because this code is racy (example:
// http://b/41494892#comment1) by design: tracing needs to have minimal runtime
// overhead, so tracing code assumes certain things are already initialized, and
// never uninitialized. However, tests uninitialize and reinitialize state,
// which races with use of this state. Therefore, we disable this test case when
// TSan is enabled.
#if defined(THREAD_SANITIZER)
#define MAYBE_JoinRunningTracing DISABLED_JoinRunningTracing
#else
#define MAYBE_JoinRunningTracing JoinRunningTracing
#endif
TEST_F(TracingSampleProfilerTest, MAYBE_JoinRunningTracing) {
  BeginTrace();
  auto profiler =
      TracingSamplerProfiler::CreateOnMainThread(base::BindRepeating(
          [] { return MakeMockUnwinderFactoryWithExpectations(); }));
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();
  ValidateReceivedEvents();
}

TEST_F(TracingSampleProfilerTest, TestStartupTracing) {
  auto profiler =
      TracingSamplerProfiler::CreateOnMainThread(base::BindRepeating(
          [] { return MakeMockUnwinderFactoryWithExpectations(); }));
  TracingSamplerProfiler::SetupStartupTracingForTesting();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  auto start_tracing_ts = TRACE_TIME_TICKS_NOW();
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();
  if (TracingSamplerProfiler::IsStackUnwindingSupportedForTesting()) {
    uint32_t seq_id = FindProfilerSequenceId();
    auto& packets = GetFinalizedPackets();
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
  TracingSamplerProfiler::SetupStartupTracingForTesting();
  base::RunLoop().RunUntilIdle();
  auto profiler =
      TracingSamplerProfiler::CreateOnMainThread(base::BindRepeating(
          [] { return MakeMockUnwinderFactoryWithExpectations(); }));
  WaitForEvents();
  auto start_tracing_ts = TRACE_TIME_TICKS_NOW();
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();
  if (TracingSamplerProfiler::IsStackUnwindingSupportedForTesting()) {
    uint32_t seq_id = FindProfilerSequenceId();
    auto& packets = GetFinalizedPackets();
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

// This is needed because this code is racy (example:
// https://crbug.com/338398659#comment1) by design: tracing needs to have
// minimal runtime overhead, so tracing code assumes certain things are already
// initialized, and never uninitialized. However, tests uninitialize and
// reinitialize state, which races with use of this state. Therefore, we disable
// this test case when TSan is enabled.
#if defined(THREAD_SANITIZER)
#define MAYBE_SamplingChildThread DISABLED_SamplingChildThread
#else
#define MAYBE_SamplingChildThread SamplingChildThread
#endif
TEST_F(TracingSampleProfilerTest, MAYBE_SamplingChildThread) {
  base::Thread sampled_thread("sampling_profiler_test");
  sampled_thread.Start();
  sampled_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TracingSamplerProfiler::CreateOnChildThreadWithCustomUnwinders,
          base::BindRepeating(
              [] { return MakeMockUnwinderFactoryWithExpectations(); })));
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
  ASSERT_GE(call_count, 1U);
  EXPECT_EQ(event_analyzer.CountEvents(), call_count);
}

TEST_F(TracingSampleProfilerTest, SampleLoaderLockAlwaysHeld) {
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
  // Use the real loader lock sampler. This tests that it is initialized
  // correctly in TracingSamplerProfiler.
  LoaderLockSamplingThread::SetLoaderLockSamplerForTesting(nullptr);

  // This must be the only thread that uses the real loader lock sampler in the
  // test process.
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

class TracingProfileBuilderTest : public TracingUnitTest {
 public:
  void SetUp() override {
    TracingUnitTest::SetUp();

    auto perfetto_wrapper = std::make_unique<base::tracing::PerfettoTaskRunner>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    producer_client_ = std::make_unique<TestProducerClient>(
        std::move(perfetto_wrapper), /*log_only_main_thread=*/false);
  }

  void TearDown() override {
    producer_client_.reset();
    TracingUnitTest::TearDown();
  }

  TestProducerClient* producer() { return producer_client_.get(); }

 private:
  std::unique_ptr<TestProducerClient> producer_client_;
};

TEST_F(TracingProfileBuilderTest, ValidModule) {
  base::TestModule module;
  TracingSamplerProfiler::TracingProfileBuilder profile_builder(
      base::PlatformThreadId(),
      std::make_unique<TestTraceWriter>(producer()),
      false);
  profile_builder.OnSampleCompleted({base::Frame(0x1010, &module)},
                                    base::TimeTicks());
}

TEST_F(TracingProfileBuilderTest, InvalidModule) {
  TracingSamplerProfiler::TracingProfileBuilder profile_builder(
      base::PlatformThreadId(),
      std::make_unique<TestTraceWriter>(producer()),
      false);
  profile_builder.OnSampleCompleted({base::Frame(0x1010, nullptr)},
                                    base::TimeTicks());
}

}  // namespace tracing
