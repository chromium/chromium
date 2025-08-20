// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/track_name_recorder.h"

#include <optional>
#include <string>

#include "base/process/process_handle.h"
#include "base/tracing/protos/chrome_enums.pbzero.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/perfetto/include/perfetto/protozero/proto_decoder.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_process_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.pbzero.h"

namespace tracing {

namespace pbzero_enums = perfetto::protos::chrome_enums::pbzero;
using perfetto::protos::pbzero::ChromeProcessDescriptor;

TEST(TrackNameRecorderTest, GenerateProcessTrackDescriptor) {
  const auto process_track = perfetto::ProcessTrack::Current();
  const std::string process_name = "Test Process";
  const auto process_type = pbzero_enums::PROCESS_BROWSER;
  const base::ProcessId process_id = base::GetCurrentProcId();
  const int64_t process_start_timestamp = 54321;
  const absl::flat_hash_map<int, std::string> process_labels{
      {1, "label1"},
      {2, "label2"},
  };
  const std::optional<uint64_t> crash_trace_id = 12345;
  const std::string host_app_package_name = "Test Package";

  perfetto::protos::gen::TrackDescriptor track_descriptor =
      TrackNameRecorder::GenerateProcessTrackDescriptor(
          process_track, process_name, process_type, process_id,
          process_start_timestamp, process_labels, crash_trace_id,
          host_app_package_name);

  EXPECT_EQ(track_descriptor.uuid(), process_track.uuid);
  EXPECT_EQ(track_descriptor.parent_uuid(), process_track.parent_uuid);
  EXPECT_EQ(static_cast<base::ProcessId>(track_descriptor.process().pid()),
            process_id);
  EXPECT_EQ(track_descriptor.process().process_name(), process_name);
  EXPECT_EQ(track_descriptor.process().start_timestamp_ns(),
            process_start_timestamp);
  EXPECT_THAT(track_descriptor.process().process_labels(),
              ::testing::UnorderedElementsAre("label1", "label2"));

  // Make sure a deserialized pbzero ChromeProcessDescriptor has all the info.
  const std::string serialized_track_descriptor =
      track_descriptor.SerializeAsString();
  perfetto::protos::pbzero::TrackDescriptor::Decoder chrome_track_descriptor(
      serialized_track_descriptor);
  ASSERT_TRUE(chrome_track_descriptor.has_chrome_process());
  ChromeProcessDescriptor::Decoder chrome_process_descriptor(
      chrome_track_descriptor.chrome_process());
  EXPECT_EQ(static_cast<pbzero_enums::ProcessType>(
                chrome_process_descriptor.process_type()),
            process_type);
  EXPECT_EQ(chrome_process_descriptor.crash_trace_id(), crash_trace_id);
  EXPECT_EQ(chrome_process_descriptor.host_app_package_name().ToStdString(),
            host_app_package_name);
}

}  // namespace tracing
