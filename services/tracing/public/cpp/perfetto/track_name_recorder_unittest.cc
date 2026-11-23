// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/track_name_recorder.h"

#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/process/current_process.h"
#include "base/process/process_handle.h"
#include "base/test/task_environment.h"
#include "base/test/tracing/trace_test_utils.h"
#include "base/tracing/protos/chrome_enums.pbzero.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/perfetto/include/perfetto/protozero/proto_decoder.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_process_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_process_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.pb.h"
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

namespace {

std::optional<perfetto::protos::TrackDescriptor>
ReadProcessTrackDescriptorFromRegistry() {
  auto track_info = perfetto::internal::TrackRegistry::Get()->FindTrackInfo(
      perfetto::ProcessTrack::Current().uuid);
  if (!track_info) {
    return std::nullopt;
  }
  perfetto::protos::TrackDescriptor desc;
  if (!desc.ParseFromString(track_info->desc)) {
    return std::nullopt;
  }
  return desc;
}

}  // namespace

TEST(TrackNameRecorderTest, UpdateProcessLabelPreservesProcessDescriptor) {
  base::test::TaskEnvironment task_environment;
  base::test::TracingEnvironment tracing_environment;

  auto* recorder = TrackNameRecorder::GetInstance();
  recorder->StartRecording();
  base::ScopedClosureRunner stop_recording(base::BindOnce(
      &TrackNameRecorder::StopRecording, base::Unretained(recorder)));
  base::CurrentProcess::GetInstance().SetProcessType(
      pbzero_enums::PROCESS_RENDERER_TOP_WEBUI);

  const int label_id_1 = recorder->GetNewProcessLabelId();
  const int label_id_2 = recorder->GetNewProcessLabelId();
  base::ScopedClosureRunner remove_labels(base::BindOnce(
      [](TrackNameRecorder* r, int id1, int id2) {
        r->RemoveProcessLabel(id1);
        r->RemoveProcessLabel(id2);
      },
      recorder, label_id_1, label_id_2));

  recorder->UpdateProcessLabel(label_id_1, "initial-label-1");
  recorder->UpdateProcessLabel(label_id_2, "label-2");
  recorder->UpdateProcessLabel(label_id_1, "updated-label-1");

  auto desc = ReadProcessTrackDescriptorFromRegistry();
  ASSERT_TRUE(desc.has_value());
  const auto process_track = perfetto::ProcessTrack::Current();
  EXPECT_EQ(desc->uuid(), process_track.uuid);
  ASSERT_TRUE(desc->has_process());
  EXPECT_EQ(desc->process().pid(), static_cast<int32_t>(process_track.pid));
  EXPECT_EQ(desc->process().process_name(), "WebUI Top Renderer");
  EXPECT_GT(desc->process().start_timestamp_ns(), 0);
  EXPECT_THAT(desc->process().process_labels(),
              ::testing::UnorderedElementsAre("updated-label-1", "label-2"));
  ASSERT_TRUE(desc->has_chrome_process());
  EXPECT_EQ(desc->chrome_process().process_type(),
            static_cast<int32_t>(pbzero_enums::PROCESS_RENDERER_TOP_WEBUI));
}

TEST(TrackNameRecorderTest, RemoveProcessLabelUpdatesProcessDescriptor) {
  base::test::TaskEnvironment task_environment;
  base::test::TracingEnvironment tracing_environment;

  auto* recorder = TrackNameRecorder::GetInstance();
  recorder->StartRecording();
  base::ScopedClosureRunner stop_recording(base::BindOnce(
      &TrackNameRecorder::StopRecording, base::Unretained(recorder)));
  base::CurrentProcess::GetInstance().SetProcessType(
      pbzero_enums::PROCESS_RENDERER);

  const int label_id_1 = recorder->GetNewProcessLabelId();
  const int label_id_2 = recorder->GetNewProcessLabelId();
  const int unused_label_id = recorder->GetNewProcessLabelId();
  base::ScopedClosureRunner remove_labels(base::BindOnce(
      [](TrackNameRecorder* r, int id1, int id2) {
        r->RemoveProcessLabel(id1);
        r->RemoveProcessLabel(id2);
      },
      recorder, label_id_1, label_id_2));

  recorder->UpdateProcessLabel(label_id_1, "page-1");
  recorder->UpdateProcessLabel(label_id_2, "page-2");

  // Removing an unset label should be a no-op.
  recorder->RemoveProcessLabel(unused_label_id);

  recorder->RemoveProcessLabel(label_id_1);
  auto desc = ReadProcessTrackDescriptorFromRegistry();
  ASSERT_TRUE(desc.has_value());
  ASSERT_TRUE(desc->has_process());
  EXPECT_EQ(desc->process().process_name(), "Renderer");
  EXPECT_THAT(desc->process().process_labels(),
              ::testing::UnorderedElementsAre("page-2"));
  ASSERT_TRUE(desc->has_chrome_process());
  EXPECT_EQ(desc->chrome_process().process_type(),
            static_cast<int32_t>(pbzero_enums::PROCESS_RENDERER));

  // Passing an empty label delegates to RemoveProcessLabel.
  recorder->UpdateProcessLabel(label_id_2, "");
  desc = ReadProcessTrackDescriptorFromRegistry();
  ASSERT_TRUE(desc.has_value());
  ASSERT_TRUE(desc->has_process());
  EXPECT_EQ(desc->process().process_name(), "Renderer");
  EXPECT_THAT(desc->process().process_labels(), ::testing::IsEmpty());
  ASSERT_TRUE(desc->has_chrome_process());
  EXPECT_EQ(desc->chrome_process().process_type(),
            static_cast<int32_t>(pbzero_enums::PROCESS_RENDERER));
}

TEST(TrackNameRecorderTest, StartRecordingSetsProcessTrackDescriptor) {
  base::test::TaskEnvironment task_environment;
  base::test::TracingEnvironment tracing_environment;

  // Simulate Linux RendererMain calling SetProcessType() before
  // TrackNameRecorder::StartRecording() runs, without any track_event OnSetup.
  base::CurrentProcess::GetInstance().SetProcessType(
      pbzero_enums::PROCESS_RENDERER);

  auto* recorder = TrackNameRecorder::GetInstance();
  recorder->StartRecording();
  base::ScopedClosureRunner stop_recording(base::BindOnce(
      &TrackNameRecorder::StopRecording, base::Unretained(recorder)));

  auto desc = ReadProcessTrackDescriptorFromRegistry();
  ASSERT_TRUE(desc.has_value());
  const auto process_track = perfetto::ProcessTrack::Current();
  EXPECT_EQ(desc->uuid(), process_track.uuid);
  ASSERT_TRUE(desc->has_process());
  EXPECT_EQ(desc->process().pid(), static_cast<int32_t>(process_track.pid));
  EXPECT_EQ(desc->process().process_name(), "Renderer");
  ASSERT_TRUE(desc->has_chrome_process());
  EXPECT_EQ(desc->chrome_process().process_type(),
            static_cast<int32_t>(pbzero_enums::PROCESS_RENDERER));
}

}  // namespace tracing
