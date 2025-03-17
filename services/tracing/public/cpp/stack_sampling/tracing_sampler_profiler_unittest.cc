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
#include "base/test/gmock_callback_support.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/trace_test_utils.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/test_utils.h"
#include "services/tracing/public/cpp/buildflags.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/producer_test_utils.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"
#include "services/tracing/tracing_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
#include "base/test/trace_event_analyzer.h"
#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampler_win.h"
#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampling_thread_win.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "base/profiler/core_unwinders.h"
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
using PacketVector =
    std::vector<std::unique_ptr<perfetto::protos::TracePacket>>;

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

    TracingSamplerProfiler::RegisterDataSource();
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

    tracing_session_ = perfetto::Tracing::NewTrace();
    tracing_session_->Setup(trace_config);
    tracing_session_->StartBlocking();
  }

  void WaitForEvents() { base::PlatformThread::Sleep(base::Milliseconds(200)); }

  void EnsureTraceStopped() {
    if (!tracing_session_) {
      return;
    }

    base::TrackEvent::Flush();

    base::RunLoop wait_for_stop;
    tracing_session_->SetOnStopCallback(
        [&wait_for_stop] { wait_for_stop.Quit(); });
    tracing_session_->Stop();
    wait_for_stop.Run();
    std::vector<char> serialized_data = tracing_session_->ReadTraceBlocking();
    tracing_session_.reset();

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
  base::trace_event::TraceResultBuffer trace_buffer_;

  base::test::TaskEnvironment task_environment_;
  base::test::TracingEnvironment tracing_environment_;
  std::vector<std::unique_ptr<perfetto::protos::TracePacket>>
      finalized_packets_;

  std::unique_ptr<perfetto::TracingSession> tracing_session_;

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
#if !BUILDFLAG(IS_ANDROID)
    return base::CreateCoreUnwindersFactory();
#else
    return base::StackSamplingProfiler::UnwindersFactory();
#endif
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
  WaitForEvents();
  EndTracing();
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
  WaitForEvents();
  EndTracing();
  ValidateReceivedEvents();
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
  WaitForEvents();
  EndTracing();
  ValidateReceivedEvents();
  sampled_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TracingSamplerProfiler::DeleteOnChildThreadForTesting));
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
  WaitForEvents();
  EndTracing();

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
  WaitForEvents();
  EndTracing();

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
  WaitForEvents();
  EndTracing();

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
  WaitForEvents();
  EndTracing();
  sampled_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TracingSamplerProfiler::DeleteOnChildThreadForTesting));

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
  WaitForEvents();
  EndTracing();

  // The loader lock may or may not be held during the test, so there's no
  // output to test. The test passes if it reaches the end without crashing.
}

#endif  // BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)

TEST(TracingProfileBuilderTest, ValidModule) {
  base::TestModule module;
  TracingSamplerProfiler::TracingProfileBuilder profile_builder(
      base::PlatformThreadId(), std::make_unique<DummyTraceWriter>(), false);
  profile_builder.OnSampleCompleted({base::Frame(0x1010, &module)},
                                    base::TimeTicks());
}

TEST(TracingProfileBuilderTest, InvalidModule) {
  TracingSamplerProfiler::TracingProfileBuilder profile_builder(
      base::PlatformThreadId(), std::make_unique<DummyTraceWriter>(), false);
  profile_builder.OnSampleCompleted({base::Frame(0x1010, nullptr)},
                                    base::TimeTicks());
}

}  // namespace tracing
