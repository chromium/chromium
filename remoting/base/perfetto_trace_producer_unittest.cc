// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/perfetto_trace_producer.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/trace_event/trace_event.h"
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
using ::testing::Not;

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
  void SetUp() override;
  void TearDown() override;

  base::test::TaskEnvironment task_environment_;
};

void PerfettoTraceProducerTest::SetUp() {
  InitPerfettoTracing("CRD UnitTest Process");
}

void PerfettoTraceProducerTest::TearDown() {
  ResetPerfettoTracingForTesting();
}

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

}  // namespace

TEST_F(PerfettoTraceProducerTest,
       WildcardCategoriesExcludeDebugTaggedCategoryByDefault) {
  PerfettoTraceProducer producer;
  PerfettoTraceConfig config;
  config.enabled_categories = {"remoting", "remoting.*", "webrtc"};

  base::test::TestFuture<std::vector<uint8_t>> trace_future;
  auto session = producer.StartTracing(config, trace_future.GetCallback());
  ASSERT_NE(session, nullptr);
  EXPECT_TRUE(producer.IsTracing());

  TRACE_EVENT("remoting", "LifecycleSlice");
  TRACE_EVENT("remoting.video", "VideoSlice");
  TRACE_EVENT("remoting.debug", "VerboseStripSlice");
  TRACE_EVENT("ui", "UnrelatedUiSlice");
  TRACE_EVENT("webrtc", "WebRtcSlice");

  // Verify WebRTC's separate internal TrackEventCategoryRegistry (registered
  // via `InitializeWebRtcModule()`) also has `"webrtc"` enabled while active.
  const auto& webrtc_registry = GetWebRtcTrackEventCategoryRegistry();
  std::optional<size_t> webrtc_index =
      FindCategoryIndex(webrtc_registry, "webrtc");
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
  EXPECT_THAT(summary.event_names, Contains("LifecycleSlice"));
  EXPECT_THAT(summary.event_names, Contains("VideoSlice"));
  EXPECT_THAT(summary.event_names, Contains("WebRtcSlice"));
  EXPECT_THAT(summary.event_names, Not(Contains("VerboseStripSlice")));
  EXPECT_THAT(summary.event_names, Not(Contains("UnrelatedUiSlice")));
}

TEST_F(PerfettoTraceProducerTest,
       ExplicitDebugCategoryAndNegativeCategoryExclusions) {
  PerfettoTraceProducer producer;
  PerfettoTraceConfig config;
  config.enabled_categories = {"remoting", "remoting.*", "remoting.debug",
                               "- remoting.video"};
  config.ring_buffer = false;

  base::test::TestFuture<std::vector<uint8_t>> trace_future;
  auto session = producer.StartTracing(config, trace_future.GetCallback());
  ASSERT_NE(session, nullptr);

  TRACE_EVENT("remoting", "LifecycleSlice");
  TRACE_EVENT("remoting.video", "ExcludedVideoSlice");
  TRACE_EVENT("remoting.debug", "ExplicitStripSlice");

  session->StopAndFlush();
  ParsedTraceEvents summary = ParseTraceEvents(trace_future.Take());
  EXPECT_THAT(summary.event_names, Contains("LifecycleSlice"));
  EXPECT_THAT(summary.event_names, Contains("ExplicitStripSlice"));
  EXPECT_THAT(summary.event_names, Not(Contains("ExcludedVideoSlice")));
}

TEST_F(PerfettoTraceProducerTest, RejectsInvalidConfig) {
  PerfettoTraceProducer producer;
  PerfettoTraceConfig empty_categories;
  EXPECT_EQ(producer.StartTracing(empty_categories, base::DoNothing()),
            nullptr);
}

TEST(PerfettoTraceProducerUninitializedTest,
     ReturnsNullBeforePerfettoInitialized) {
  ASSERT_FALSE(perfetto::Tracing::IsInitialized());
  base::test::TaskEnvironment task_environment;
  PerfettoTraceProducer producer;
  PerfettoTraceConfig config;
  config.enabled_categories = {"remoting"};
  EXPECT_EQ(producer.StartTracing(config, base::DoNothing()), nullptr);
}

}  // namespace remoting
