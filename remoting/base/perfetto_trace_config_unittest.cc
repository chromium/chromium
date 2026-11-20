// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/perfetto_trace_config.h"

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_config.h"
#include "third_party/perfetto/protos/perfetto/common/builtin_clock.gen.h"
#include "third_party/perfetto/protos/perfetto/config/track_event/track_event_config.gen.h"

namespace remoting {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

perfetto::protos::gen::TrackEventConfig ParseTrackEventConfig(
    const perfetto::TraceConfig& trace_config) {
  perfetto::protos::gen::TrackEventConfig track_event_config;
  if (trace_config.data_sources_size() > 0) {
    track_event_config.ParseFromString(
        trace_config.data_sources()[0].config().track_event_config_raw());
  }
  return track_event_config;
}

}  // namespace

TEST(PerfettoTraceConfigTest, DefaultConfigBuildsRingBufferAndPeriodicClear) {
  PerfettoTraceConfig config;
  config.enabled_categories = {"remoting", "remoting.*"};

  perfetto::TraceConfig proto = config.ToPerfettoConfig();
  ASSERT_EQ(proto.buffers_size(), 1);
  EXPECT_EQ(proto.buffers()[0].size_kb(), kDefaultPerfettoBufferSizeKb);
  EXPECT_EQ(proto.buffers()[0].fill_policy(),
            perfetto::TraceConfig::BufferConfig::RING_BUFFER);
  EXPECT_EQ(proto.builtin_data_sources().primary_trace_clock(),
            perfetto::protos::gen::BUILTIN_CLOCK_MONOTONIC);
  EXPECT_EQ(proto.incremental_state_config().clear_period_ms(),
            kDefaultPerfettoClearPeriodMs);

  ASSERT_EQ(proto.data_sources_size(), 1);
  EXPECT_EQ(proto.data_sources()[0].config().name(), "track_event");
  EXPECT_EQ(proto.data_sources()[0].config().target_buffer(), 0u);

  perfetto::protos::gen::TrackEventConfig track_event =
      ParseTrackEventConfig(proto);
  EXPECT_TRUE(track_event.enable_thread_time_sampling());
  EXPECT_THAT(track_event.enabled_categories(),
              ElementsAre("remoting", "remoting.*", "__metadata"));
  EXPECT_THAT(track_event.disabled_categories(), ElementsAre("*"));
}

TEST(PerfettoTraceConfigTest, DiscardBufferModeAndZeroClearPeriod) {
  PerfettoTraceConfig config;
  config.enabled_categories = {"remoting.video"};
  config.max_buffer_size_kb = kMaxPerfettoBufferSizeKb;
  config.ring_buffer = false;
  config.clear_period_ms = 0;

  perfetto::TraceConfig proto = config.ToPerfettoConfig();
  ASSERT_EQ(proto.buffers_size(), 1);
  EXPECT_EQ(proto.buffers()[0].size_kb(), kMaxPerfettoBufferSizeKb);
  EXPECT_EQ(proto.buffers()[0].fill_policy(),
            perfetto::TraceConfig::BufferConfig::DISCARD);
  EXPECT_FALSE(proto.has_incremental_state_config());
}

TEST(PerfettoTraceConfigTest,
     TrimsWhitespaceAndHandlesNegativeCategoryExclusions) {
  PerfettoTraceConfig config;
  config.enabled_categories = {
      "  remoting  ", "", "   ", "- remoting.video ", "-", " -  ", "webrtc"};

  perfetto::protos::gen::TrackEventConfig track_event =
      ParseTrackEventConfig(config.ToPerfettoConfig());
  EXPECT_THAT(track_event.enabled_categories(),
              ElementsAre("remoting", "webrtc", "__metadata"));
  EXPECT_THAT(track_event.disabled_categories(),
              ElementsAre("remoting.video", "*"));
}

TEST(PerfettoTraceConfigTest, WildcardCategoryOmitsGlobalDisable) {
  PerfettoTraceConfig config;
  config.enabled_categories = {"*", "-remoting.debug"};

  perfetto::protos::gen::TrackEventConfig track_event =
      ParseTrackEventConfig(config.ToPerfettoConfig());
  EXPECT_THAT(track_event.enabled_categories(), ElementsAre("*", "__metadata"));
  EXPECT_THAT(track_event.disabled_categories(), ElementsAre("remoting.debug"));
}

TEST(PerfettoTraceConfigTest, ValidatesBoundsAndPositiveCategories) {
  PerfettoTraceConfig valid;
  valid.enabled_categories = {"remoting"};
  EXPECT_TRUE(valid.IsValid());

  PerfettoTraceConfig empty_categories;
  EXPECT_FALSE(empty_categories.IsValid());

  PerfettoTraceConfig exclusions_only;
  exclusions_only.enabled_categories = {"-remoting.debug", "   "};
  EXPECT_FALSE(exclusions_only.IsValid());

  PerfettoTraceConfig zero_buffer = valid;
  zero_buffer.max_buffer_size_kb = 0;
  EXPECT_FALSE(zero_buffer.IsValid());

  PerfettoTraceConfig oversized_buffer = valid;
  oversized_buffer.max_buffer_size_kb = kMaxPerfettoBufferSizeKb + 1;
  EXPECT_FALSE(oversized_buffer.IsValid());

  PerfettoTraceConfig negative_duration = valid;
  negative_duration.duration = base::Seconds(-1);
  EXPECT_FALSE(negative_duration.IsValid());
}

}  // namespace remoting
