// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/perfetto_trace_producer.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/perfetto_platform.h"
#include "remoting/base/perfetto_tracing.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_category_registry.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"
#include "third_party/webrtc_overrides/init_webrtc.h"

namespace remoting {

namespace {

using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;

constexpr char kTestCategory[] = "test";
constexpr char kWebRtcCategory[] = "webrtc";
constexpr char kUnrelatedCategory[] = "ui";

constexpr char kTestEvent1[] = "TestEvent1";
constexpr char kTestEvent2[] = "TestEvent2";
constexpr char kTestRemotingEvent[] = "TestRemotingEvent";
constexpr char kTestVideoEvent[] = "TestVideoEvent";
constexpr char kTestDebugEvent[] = "TestDebugEvent";
constexpr char kTestWebRtcEvent[] = "TestWebRtcEvent";
constexpr char kTestUnrelatedEvent[] = "TestUnrelatedEvent";

struct ParsedTraceEvents {
  std::set<std::string> event_names;
  size_t packet_count = 0;
};

ParsedTraceEvents ParseTraceEvents(const std::vector<uint8_t>& trace_bytes) {
  ParsedTraceEvents summary;
  perfetto::protos::pbzero::Trace::Decoder trace(trace_bytes.data(),
                                                 trace_bytes.size());
  for (auto packet_it = trace.packet(); packet_it; ++packet_it) {
    ++summary.packet_count;
    perfetto::protos::pbzero::TracePacket::Decoder packet(*packet_it);

    if (packet.has_interned_data()) {
      perfetto::protos::pbzero::InternedData::Decoder interned(
          packet.interned_data());
      for (auto name_it = interned.event_names(); name_it; ++name_it) {
        perfetto::protos::pbzero::EventName::Decoder event_name(*name_it);
        if (event_name.has_name()) {
          summary.event_names.insert(event_name.name().ToStdString());
        }
      }
    }

    if (packet.has_track_event()) {
      perfetto::protos::pbzero::TrackEvent::Decoder event(packet.track_event());
      if (event.has_name()) {
        summary.event_names.insert(event.name().ToStdString());
      }
    }
  }
  return summary;
}

class PerfettoTraceProducerTest : public testing::Test {
 protected:
  using TimeSource = base::test::TaskEnvironment::TimeSource;

  explicit PerfettoTraceProducerTest(
      TimeSource time_source = TimeSource::SYSTEM_TIME);

  void SetUp() override;
  void TearDown() override;

  base::test::TaskEnvironment task_environment_;
};

// Uses mock time to drive `config.duration` expiry. Kept separate from
// `PerfettoTraceProducerTest` because with mock time an idle main thread
// fast-forwards to its next delayed task, which could expire the read timeout
// while Perfetto's task runner is still working.
class PerfettoTraceProducerMockTimeTest : public PerfettoTraceProducerTest {
 protected:
  PerfettoTraceProducerMockTimeTest();
};

PerfettoTraceProducerTest::PerfettoTraceProducerTest(TimeSource time_source)
    : task_environment_(time_source) {}

void PerfettoTraceProducerTest::SetUp() {
  InitPerfettoTracing("CRD UnitTest Process");
}

void PerfettoTraceProducerTest::TearDown() {
  ResetPerfettoTracingForTesting();
}

PerfettoTraceProducerMockTimeTest::PerfettoTraceProducerMockTimeTest()
    : PerfettoTraceProducerTest(TimeSource::MOCK_TIME) {}

std::optional<size_t> FindCategoryIndex(
    const perfetto::internal::TrackEventCategoryRegistry& registry,
    std::string_view name) {
  for (size_t i = 0; i < registry.category_count(); ++i) {
    if (registry.GetCategory(i)->name == name) {
      return i;
    }
  }
  return std::nullopt;
}

// Posts a barrier `QuitClosure` onto `PerfettoPlatform`'s FIFO task runner so
// queued `DestroyTracingSession` tasks finish before Perfetto's muxer is reset.
void FlushPerfettoTaskRunner() {
  auto platform_runner = base::tracing::PerfettoPlatform::Get().task_runner();
  ASSERT_NE(platform_runner, nullptr);
  base::RunLoop flush_loop;
  platform_runner->PostTask(FROM_HERE, flush_loop.QuitClosure());
  flush_loop.Run();
}

}  // namespace

TEST_F(PerfettoTraceProducerTest,
       WildcardCategoriesExcludeDebugTaggedCategoryByDefault) {
  PerfettoTraceProducer producer;
  PerfettoTraceConfig config;
  config.enabled_categories = {kRemotingTraceCategory,
                               kRemotingSubcategoriesPattern, kWebRtcCategory};

  base::test::TestFuture<std::vector<uint8_t>> trace_future;
  auto session = producer.StartTracing(config, trace_future.GetCallback());
  ASSERT_NE(session, nullptr);
  EXPECT_TRUE(producer.IsTracing());

  TRACE_EVENT(kRemotingTraceCategory, kTestRemotingEvent);
  TRACE_EVENT(kRemotingVideoTraceCategory, kTestVideoEvent);
  TRACE_EVENT(kRemotingDebugTraceCategory, kTestDebugEvent);
  TRACE_EVENT(kUnrelatedCategory, kTestUnrelatedEvent);
  TRACE_EVENT(kWebRtcCategory, kTestWebRtcEvent);

  // Verify WebRTC's separate internal TrackEventCategoryRegistry (registered
  // via `InitializeWebRtcModule()`) also has `"webrtc"` enabled while active.
  const auto& webrtc_registry = GetWebRtcTrackEventCategoryRegistry();
  std::optional<size_t> webrtc_index =
      FindCategoryIndex(webrtc_registry, kWebRtcCategory);
  ASSERT_TRUE(webrtc_index.has_value());
  EXPECT_NE(webrtc_registry.GetCategoryState(*webrtc_index)
                ->load(std::memory_order_relaxed),
            0u);

  session->StopAndFlush();
  ParsedTraceEvents summary = ParseTraceEvents(trace_future.Take());
  EXPECT_FALSE(producer.IsTracing());
  EXPECT_EQ(webrtc_registry.GetCategoryState(*webrtc_index)
                ->load(std::memory_order_relaxed),
            0u);
  EXPECT_THAT(summary.event_names, Contains(kTestRemotingEvent));
  EXPECT_THAT(summary.event_names, Contains(kTestVideoEvent));
  EXPECT_THAT(summary.event_names, Contains(kTestWebRtcEvent));
  EXPECT_THAT(summary.event_names, Not(Contains(kTestDebugEvent)));
  EXPECT_THAT(summary.event_names, Not(Contains(kTestUnrelatedEvent)));
}

TEST_F(PerfettoTraceProducerTest,
       ExplicitDebugCategoryAndNegativeCategoryExclusions) {
  PerfettoTraceProducer producer;
  PerfettoTraceConfig config;
  config.enabled_categories = {kRemotingTraceCategory,
                               kRemotingSubcategoriesPattern,
                               kRemotingDebugTraceCategory,
                               std::string("- ") + kRemotingVideoTraceCategory};
  config.ring_buffer = false;

  base::test::TestFuture<std::vector<uint8_t>> trace_future;
  auto session = producer.StartTracing(config, trace_future.GetCallback());
  ASSERT_NE(session, nullptr);

  TRACE_EVENT(kRemotingTraceCategory, kTestRemotingEvent);
  TRACE_EVENT(kRemotingVideoTraceCategory, kTestVideoEvent);
  TRACE_EVENT(kRemotingDebugTraceCategory, kTestDebugEvent);

  session->StopAndFlush();
  ParsedTraceEvents summary = ParseTraceEvents(trace_future.Take());
  EXPECT_THAT(summary.event_names, Contains(kTestRemotingEvent));
  EXPECT_THAT(summary.event_names, Contains(kTestDebugEvent));
  EXPECT_THAT(summary.event_names, Not(Contains(kTestVideoEvent)));
}

TEST_F(PerfettoTraceProducerMockTimeTest, DurationTimerAutoStopsAndFlushes) {
  PerfettoTraceProducer producer;
  // Disable the read watchdog so `MOCK_TIME` fast-forwarding on an idle main
  // thread cannot expire it while Perfetto's background thread runs.
  producer.SetReadTimeoutForTesting(base::TimeDelta::Max());
  PerfettoTraceConfig config;
  config.enabled_categories = {kTestCategory};
  config.duration = base::Seconds(30);

  base::test::TestFuture<std::vector<uint8_t>> trace_future;
  auto session = producer.StartTracing(config, trace_future.GetCallback());
  ASSERT_NE(session, nullptr);
  EXPECT_TRUE(producer.IsTracing());

  TRACE_EVENT(kTestCategory, kTestEvent1);

  task_environment_.FastForwardBy(base::Seconds(29));
  EXPECT_FALSE(trace_future.IsReady());

  // Advance to duration expiry, then drop `session`. Dropping the handle after
  // the timer has stopped recording must not cancel the timer-triggered flush.
  task_environment_.FastForwardBy(base::Seconds(1));
  session.reset();

  std::vector<uint8_t> trace_bytes = trace_future.Take();
  EXPECT_FALSE(producer.IsTracing());

  ParsedTraceEvents summary = ParseTraceEvents(trace_bytes);
  EXPECT_THAT(summary.event_names, Contains(kTestEvent1));
}

TEST_F(PerfettoTraceProducerTest, EarlyStopAndFlushSupportsImmediateReArm) {
  PerfettoTraceProducer producer;
  PerfettoTraceConfig config;
  config.enabled_categories = {kTestCategory};

  std::unique_ptr<PerfettoTraceProducer::Session> rearmed_session;
  base::test::TestFuture<std::vector<uint8_t>> rearmed_future;
  std::vector<uint8_t> first_trace_bytes;
  base::RunLoop first_done_loop;

  auto first_session = producer.StartTracing(
      config, base::BindLambdaForTesting([&](std::vector<uint8_t> bytes) {
        first_trace_bytes = std::move(bytes);
        EXPECT_FALSE(producer.IsTracing());
        rearmed_session =
            producer.StartTracing(config, rearmed_future.GetCallback());
        first_done_loop.Quit();
      }));
  ASSERT_NE(first_session, nullptr);

  // Starting a concurrent session while one is active must fail.
  EXPECT_EQ(producer.StartTracing(config, base::DoNothing()), nullptr);

  TRACE_EVENT(kTestCategory, kTestEvent1);

  // Calling `StopAndFlush()` and immediately dropping the `Session` handle must
  // still complete the flush rather than cancelling it.
  first_session->StopAndFlush();
  first_session.reset();
  first_done_loop.Run();

  ASSERT_NE(rearmed_session, nullptr);
  EXPECT_TRUE(producer.IsTracing());
  TRACE_EVENT(kTestCategory, kTestEvent2);
  rearmed_session->StopAndFlush();

  ParsedTraceEvents first_summary = ParseTraceEvents(first_trace_bytes);
  EXPECT_THAT(first_summary.event_names, Contains(kTestEvent1));

  ParsedTraceEvents second_summary = ParseTraceEvents(rearmed_future.Take());
  EXPECT_THAT(second_summary.event_names, Contains(kTestEvent2));
  EXPECT_THAT(second_summary.event_names, Not(Contains(kTestEvent1)));
}

TEST_F(PerfettoTraceProducerTest, DroppingSessionHandleCancelsAndDiscards) {
  PerfettoTraceProducer producer;
  PerfettoTraceConfig config;
  config.enabled_categories = {kTestCategory};

  bool callback_called = false;
  auto session = producer.StartTracing(
      config, base::BindLambdaForTesting(
                  [&](std::vector<uint8_t>) { callback_called = true; }));
  ASSERT_NE(session, nullptr);
  TRACE_EVENT(kTestCategory, kTestEvent1);

  // Destroy handle without calling `StopAndFlush()`.
  session.reset();
  EXPECT_FALSE(producer.IsTracing());

  // Producer should immediately accept a new session, and draining its flush
  // confirms the cancelled session's callback is never invoked.
  base::test::TestFuture<std::vector<uint8_t>> next_future;
  auto next_session = producer.StartTracing(config, next_future.GetCallback());
  ASSERT_NE(next_session, nullptr);
  TRACE_EVENT(kTestCategory, kTestEvent2);
  next_session->StopAndFlush();

  ParsedTraceEvents summary = ParseTraceEvents(next_future.Take());
  EXPECT_FALSE(callback_called);
  EXPECT_THAT(summary.event_names, Contains(kTestEvent2));
  EXPECT_THAT(summary.event_names, Not(Contains(kTestEvent1)));
}

TEST_F(PerfettoTraceProducerTest,
       CancelDuringFlushDiscardsAndAllowsNewSession) {
  PerfettoTraceProducer producer;
  PerfettoTraceConfig config;
  config.enabled_categories = {kTestCategory};

  bool callback_called = false;
  auto session = producer.StartTracing(
      config, base::BindLambdaForTesting(
                  [&](std::vector<uint8_t>) { callback_called = true; }));
  ASSERT_NE(session, nullptr);
  TRACE_EVENT(kTestCategory, kTestEvent1);

  // Cancel while the flush requested by `StopAndFlush()` is still in flight.
  session->StopAndFlush();
  EXPECT_TRUE(producer.IsTracing());
  session->Cancel();
  EXPECT_FALSE(producer.IsTracing());

  // The producer must accept a new session, and draining its flush confirms
  // the cancelled session's callback is never invoked.
  base::test::TestFuture<std::vector<uint8_t>> next_future;
  auto next_session = producer.StartTracing(config, next_future.GetCallback());
  ASSERT_NE(next_session, nullptr);
  TRACE_EVENT(kTestCategory, kTestEvent2);
  next_session->StopAndFlush();

  ParsedTraceEvents summary = ParseTraceEvents(next_future.Take());
  EXPECT_FALSE(callback_called);
  EXPECT_THAT(summary.event_names, Contains(kTestEvent2));
  EXPECT_THAT(summary.event_names, Not(Contains(kTestEvent1)));
}

TEST_F(PerfettoTraceProducerTest, ExpiredReadTimeoutStillCompletesSession) {
  PerfettoTraceProducer producer;
  // A zero timeout normally expires before the first chunk arrives, in which
  // case the delivered trace is empty. The session must complete either way.
  producer.SetReadTimeoutForTesting(base::TimeDelta());
  PerfettoTraceConfig config;
  config.enabled_categories = {kTestCategory};

  base::test::TestFuture<std::vector<uint8_t>> trace_future;
  auto session = producer.StartTracing(config, trace_future.GetCallback());
  ASSERT_NE(session, nullptr);

  session->StopAndFlush();
  EXPECT_TRUE(trace_future.Wait());
  EXPECT_FALSE(producer.IsTracing());

  // The producer must accept a new session afterwards.
  producer.SetReadTimeoutForTesting(base::Seconds(30));
  base::test::TestFuture<std::vector<uint8_t>> next_future;
  auto next_session = producer.StartTracing(config, next_future.GetCallback());
  ASSERT_NE(next_session, nullptr);
  TRACE_EVENT(kTestCategory, kTestEvent1);
  next_session->StopAndFlush();

  ParsedTraceEvents summary = ParseTraceEvents(next_future.Take());
  EXPECT_THAT(summary.event_names, Contains(kTestEvent1));
}

TEST_F(PerfettoTraceProducerTest, RejectsInvalidConfig) {
  PerfettoTraceProducer producer;
  PerfettoTraceConfig empty_categories;
  EXPECT_EQ(producer.StartTracing(empty_categories, base::DoNothing()),
            nullptr);
}

TEST_F(PerfettoTraceProducerTest, SessionErrorInvokesCallback) {
  PerfettoTraceProducer producer;
  PerfettoTraceConfig config;
  config.enabled_categories = {kTestCategory};

  base::test::TestFuture<std::vector<uint8_t>> trace_future;
  auto session = producer.StartTracing(config, trace_future.GetCallback());
  ASSERT_NE(session, nullptr);
  EXPECT_TRUE(producer.IsTracing());

  // Preempt the active session with a higher-priority exclusive Perfetto
  // session to trigger `OnTracingError()` after `StartTracing()` has returned
  // a valid `Session` handle.
  perfetto::TraceConfig exclusive_config = config.ToPerfettoConfig();
  exclusive_config.set_exclusive_prio(1);
  auto preempting_session =
      perfetto::Tracing::NewTrace(perfetto::kInProcessBackend);
  preempting_session->Setup(exclusive_config);
  preempting_session->StartBlocking();

  std::vector<uint8_t> trace_bytes = trace_future.Take();
  EXPECT_THAT(trace_bytes, IsEmpty());
  EXPECT_FALSE(producer.IsTracing());

  // Subsequent calls on the invalidated `Session` handle must be safe no-ops.
  session->StopAndFlush();
  session.reset();

  preempting_session->StopBlocking();
  preempting_session.reset();
  FlushPerfettoTaskRunner();
}

TEST(PerfettoTraceProducerUninitializedTest,
     ReturnsNullBeforePerfettoInitialized) {
  ASSERT_FALSE(perfetto::Tracing::IsInitialized());
  base::test::TaskEnvironment task_environment;
  PerfettoTraceProducer producer;
  PerfettoTraceConfig config;
  config.enabled_categories = {kTestCategory};
  EXPECT_EQ(producer.StartTracing(config, base::DoNothing()), nullptr);
}

}  // namespace remoting
