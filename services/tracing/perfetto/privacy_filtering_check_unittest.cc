// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/privacy_filtering_check.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pb.h"

namespace tracing {

void FillDisallowedTestField(perfetto::protos::TracePacket* packet) {
  auto* for_testing = packet->mutable_for_testing();
  for_testing->set_str("TestField");
  for_testing->set_counter(10);
}

perfetto::protos::Trace GetFilteredTrace(const perfetto::protos::Trace& trace) {
  std::string serialized = trace.SerializeAsString();
  PrivacyFilteringCheck check;
  check.RemoveBlockedFields(serialized);

  perfetto::protos::Trace filtered;
  EXPECT_TRUE(filtered.ParseFromString(serialized));
  return filtered;
}

TEST(PrivacyFilteringTest, EmptyTrace) {
  perfetto::protos::Trace trace;
  perfetto::protos::Trace filtered = GetFilteredTrace(trace);
  ASSERT_EQ(0, filtered.packet_size());
}

TEST(PrivacyFilteringTest, SafeToplevelField) {
  perfetto::protos::Trace trace;
  trace.add_packet()->set_timestamp(10);

  perfetto::protos::Trace filtered = GetFilteredTrace(trace);
  ASSERT_EQ(1, filtered.packet_size());
  EXPECT_EQ(10u, filtered.packet(0).timestamp());
}

TEST(PrivacyFilteringTest, SafeToplevelMessageField) {
  perfetto::protos::Trace trace;
  trace.add_packet()->mutable_track_event()->set_track_uuid(11);

  perfetto::protos::Trace filtered = GetFilteredTrace(trace);
  ASSERT_EQ(1, filtered.packet_size());
  EXPECT_EQ(11u, filtered.packet(0).track_event().track_uuid());
}

TEST(PrivacyFilteringTest, RepeatedFields) {
  perfetto::protos::Trace trace;
  auto* track_event = trace.add_packet()->mutable_track_event();
  track_event->add_debug_annotations()->set_name_iid(5);
  track_event->add_debug_annotations()->set_name_iid(2);
  track_event->add_debug_annotations()->set_name_iid(8);
  track_event->add_flow_ids(3);
  track_event->add_flow_ids(6);
  track_event->add_flow_ids(9);

  perfetto::protos::Trace filtered = GetFilteredTrace(trace);
  ASSERT_EQ(1, filtered.packet_size());
  EXPECT_EQ(0, filtered.packet(0).track_event().debug_annotations_size());
  EXPECT_EQ(3u, filtered.packet(0).track_event().flow_ids(0));
  EXPECT_EQ(6u, filtered.packet(0).track_event().flow_ids(1));
  EXPECT_EQ(9u, filtered.packet(0).track_event().flow_ids(2));
}

TEST(PrivacyFilteringTest, UnsafeToplevelField) {
  perfetto::protos::Trace trace;
  FillDisallowedTestField(trace.add_packet());

  perfetto::protos::Trace filtered = GetFilteredTrace(trace);
  ASSERT_EQ(1, filtered.packet_size());
}

TEST(PrivacyFilteringTest, SafeMessageWithOnlyUnsafeFields) {
  perfetto::protos::Trace trace;
  auto* packet = trace.add_packet();
  packet->mutable_track_event()->mutable_legacy_event();
  auto* debug_annotations =
      packet->mutable_track_event()->add_debug_annotations();
  debug_annotations->set_name_iid(2);
  debug_annotations->set_int_value(10);
  packet->mutable_track_event()->mutable_log_message()->set_body_iid(1);

  perfetto::protos::Trace filtered = GetFilteredTrace(trace);
  ASSERT_EQ(1, filtered.packet_size());
}

TEST(PrivacyFilteringTest, SafeAndUnsafeFields) {
  perfetto::protos::Trace trace;
  perfetto::protos::TracePacket* packet = trace.add_packet();
  FillDisallowedTestField(packet);
  packet->mutable_trace_packet_defaults()->set_timestamp_clock_id(11);

  perfetto::protos::Trace filtered = GetFilteredTrace(trace);
  ASSERT_EQ(1, filtered.packet_size());
  EXPECT_EQ(11u,
            filtered.packet(0).trace_packet_defaults().timestamp_clock_id());
  EXPECT_FALSE(filtered.packet(0).has_for_testing());
}

TEST(PrivacyFilteringTest, SafeAndUnsafePackets) {
  perfetto::protos::Trace trace;
  FillDisallowedTestField(trace.add_packet());
  trace.add_packet()->mutable_trace_packet_defaults()->set_timestamp_clock_id(
      11);

  perfetto::protos::Trace filtered = GetFilteredTrace(trace);
  ASSERT_EQ(2, filtered.packet_size());
  EXPECT_FALSE(filtered.packet(1).has_for_testing());
  EXPECT_EQ(11u,
            filtered.packet(1).trace_packet_defaults().timestamp_clock_id());
}

TEST(PrivacyFilteringTest, NestedSafeAndUnsafeFields) {
  perfetto::protos::Trace trace;
  perfetto::protos::TracePacket* packet = trace.add_packet();
  FillDisallowedTestField(packet);
  packet->set_timestamp(50);
  auto* track_event = packet->mutable_track_event();
  track_event->set_track_uuid(11);
  track_event->mutable_log_message()->set_body_iid(10);
  track_event->set_name_iid(1);
  track_event->add_category_iids(2);
  auto* hist = track_event->mutable_chrome_histogram_sample();
  hist->set_name_hash(4);
  hist->set_name("hist");
  hist->set_sample(5);
  track_event->add_flow_ids(3);
  track_event->add_debug_annotations()->set_name_iid(2);
  track_event->add_flow_ids(6);
  track_event->add_debug_annotations()->set_name_iid(5);
  track_event->add_flow_ids(9);
  track_event->add_debug_annotations()->set_name_iid(8);

  perfetto::protos::Trace filtered = GetFilteredTrace(trace);
  ASSERT_EQ(1, filtered.packet_size());
  const auto& packet1 = filtered.packet(0);
  EXPECT_FALSE(packet1.has_for_testing());
  EXPECT_EQ(50u, packet1.timestamp());

  const auto& event = packet1.track_event();
  EXPECT_EQ(11u, event.track_uuid());
  EXPECT_FALSE(event.has_log_message());
  EXPECT_EQ(1u, event.name_iid());
  ASSERT_EQ(1, event.category_iids_size());
  EXPECT_EQ(2u, event.category_iids(0));
  EXPECT_EQ(0, event.debug_annotations_size());
  ASSERT_EQ(3, event.flow_ids_size());
  EXPECT_EQ(3u, event.flow_ids(0));
  EXPECT_EQ(6u, event.flow_ids(1));
  EXPECT_EQ(9u, event.flow_ids(2));

  const auto& histogram = event.chrome_histogram_sample();
  EXPECT_EQ(4u, histogram.name_hash());
  EXPECT_FALSE(histogram.has_name());
  EXPECT_EQ(5, histogram.sample());
}

}  // namespace tracing
