// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/thread_instruction_count.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "services/tracing/public/cpp/perfetto/producer_test_utils.h"
#include "services/tracing/public/cpp/perfetto/trace_time.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_interned_data_index.h"
#include "third_party/perfetto/protos/perfetto/trace/clock_snapshot.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_process_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_thread_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/counter_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.pb.h"

using TrackEvent = perfetto::protos::TrackEvent;

namespace tracing {

namespace {

constexpr char kTestProcess[] = "Browser";
constexpr char kTestThread[] = "CrTestMain";
constexpr const char kCategoryGroup[] = "foo";

constexpr uint32_t kClockIdAbsolute = 64;
constexpr uint32_t kClockIdIncremental = 65;

class TraceEventDataSourceTest : public testing::Test {
 public:
  void SetUp() override {
    TraceEventDataSource::GetInstance()->RegisterStartupHooks();
    // TODO(eseckler): Initialize the entire perfetto client library instead.
    perfetto::internal::TrackRegistry::InitializeInstance();

    old_thread_name_ =
        base::ThreadIdNameManager::GetInstance()->GetNameForCurrentThread();
    base::ThreadIdNameManager::GetInstance()->SetName(kTestThread);
    old_process_name_ =
        base::trace_event::TraceLog::GetInstance()->process_name();
    base::trace_event::TraceLog::GetInstance()->set_process_name(kTestProcess);

    PerfettoTracedProcess::Get()->ClearDataSourcesForTesting();
    PerfettoTracedProcess::ResetTaskRunnerForTesting();
    PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner();
    auto perfetto_wrapper = std::make_unique<PerfettoTaskRunner>(
        task_environment_.GetMainThreadTaskRunner());
    producer_client_ =
        std::make_unique<TestProducerClient>(std::move(perfetto_wrapper));
    TraceEventMetadataSource::GetInstance()->ResetForTesting();
  }

  void TearDown() override {
    if (base::trace_event::TraceLog::GetInstance()->IsEnabled()) {
      base::RunLoop wait_for_tracelog_flush;

      TraceEventDataSource::GetInstance()->StopTracing(base::BindOnce(
          [](const base::RepeatingClosure& quit_closure) {
            quit_closure.Run();
          },
          wait_for_tracelog_flush.QuitClosure()));

      wait_for_tracelog_flush.Run();
    }

    // As MockTraceWriter keeps a pointer to our TestProducerClient,
    // we need to make sure to clean it up from TLS. The other sequences
    // get DummyTraceWriters that we don't care about.
    TraceEventDataSource::GetInstance()->FlushCurrentThread();
    producer_client_.reset();

    base::ThreadIdNameManager::GetInstance()->SetName(old_thread_name_);
    base::trace_event::TraceLog::GetInstance()->set_process_name(
        old_process_name_);
  }

  void CreateTraceEventDataSource(bool privacy_filtering_enabled = false,
                                  bool start_trace = true,
                                  const std::string& chrome_trace_config = "") {
    task_environment_.RunUntilIdle();
    base::RunLoop tracing_started;
    base::SequencedTaskRunnerHandle::Get()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce([]() { TraceEventDataSource::ResetForTesting(); }),
        tracing_started.QuitClosure());
    tracing_started.Run();
    if (start_trace) {
      perfetto::DataSourceConfig config;
      config.mutable_chrome_config()->set_privacy_filtering_enabled(
          privacy_filtering_enabled);
      config.mutable_chrome_config()->set_trace_config(chrome_trace_config);
      TraceEventDataSource::GetInstance()->StartTracing(producer_client(),
                                                        config);
    }
  }

  TestProducerClient* producer_client() { return producer_client_.get(); }

  void ExpectClockSnapshotAndDefaults(
      const perfetto::protos::TracePacket* packet,
      uint64_t min_timestamp = 1u) {
    // ClockSnapshot for absolute & incremental microsecond clocks.
    ASSERT_TRUE(packet->has_clock_snapshot());
    ASSERT_EQ(packet->clock_snapshot().clocks().size(), 3);

    EXPECT_EQ(packet->clock_snapshot().clocks()[0].clock_id(),
              static_cast<uint32_t>(kTraceClockId));
    EXPECT_FALSE(packet->clock_snapshot().clocks()[0].has_unit_multiplier_ns());
    EXPECT_FALSE(packet->clock_snapshot().clocks()[0].has_is_incremental());

    EXPECT_EQ(packet->clock_snapshot().clocks()[1].clock_id(),
              kClockIdAbsolute);
    EXPECT_GE(packet->clock_snapshot().clocks()[1].timestamp(), min_timestamp);
    EXPECT_EQ(packet->clock_snapshot().clocks()[1].unit_multiplier_ns(), 1000u);
    EXPECT_FALSE(packet->clock_snapshot().clocks()[1].has_is_incremental());

    EXPECT_EQ(packet->clock_snapshot().clocks()[2].clock_id(),
              kClockIdIncremental);
    EXPECT_GE(packet->clock_snapshot().clocks()[2].timestamp(), min_timestamp);
    EXPECT_EQ(packet->clock_snapshot().clocks()[2].unit_multiplier_ns(), 1000u);
    EXPECT_TRUE(packet->clock_snapshot().clocks()[2].is_incremental());

    EXPECT_EQ(packet->clock_snapshot().clocks()[1].timestamp(),
              packet->clock_snapshot().clocks()[2].timestamp());

    EXPECT_GE(packet->clock_snapshot().clocks()[1].timestamp(),
              last_timestamp_);
    EXPECT_LE(packet->clock_snapshot().clocks()[1].timestamp(),
              static_cast<uint64_t>(
                  TRACE_TIME_TICKS_NOW().since_origin().InMicroseconds()));

    last_timestamp_ = packet->clock_snapshot().clocks()[1].timestamp();

    // Default to incremental clock and thread track.
    ASSERT_TRUE(packet->has_trace_packet_defaults());
    EXPECT_EQ(packet->trace_packet_defaults().timestamp_clock_id(),
              kClockIdIncremental);
    ASSERT_TRUE(packet->trace_packet_defaults().has_track_event_defaults());
    EXPECT_NE(
        packet->trace_packet_defaults().track_event_defaults().track_uuid(),
        0u);

    if (base::ThreadTicks::IsSupported()) {
      EXPECT_GT(packet->trace_packet_defaults()
                    .track_event_defaults()
                    .extra_counter_track_uuids_size(),
                0);
    }

    default_track_uuid_ =
        packet->trace_packet_defaults().track_event_defaults().track_uuid();

    default_extra_counter_track_uuids_.clear();
    for (const auto& uuid : packet->trace_packet_defaults()
                                .track_event_defaults()
                                .extra_counter_track_uuids()) {
      default_extra_counter_track_uuids_.push_back(uuid);
    }

    // ClockSnapshot is only emitted when incremental state was reset, and
    // thus also always serves as indicator for the state reset to the consumer.
    EXPECT_EQ(packet->sequence_flags(),
              static_cast<uint32_t>(perfetto::protos::pbzero::TracePacket::
                                        SEQ_INCREMENTAL_STATE_CLEARED));
  }

  void ExpectThreadTrack(const perfetto::protos::TracePacket* packet,
                         int thread_id = 0,
                         uint64_t min_timestamp = 1u,
                         bool filtering_enabled = false) {
    ASSERT_TRUE(packet->has_track_descriptor());
    ASSERT_TRUE(packet->track_descriptor().has_thread());

    EXPECT_NE(packet->track_descriptor().uuid(), 0u);
    EXPECT_NE(packet->track_descriptor().parent_uuid(), 0u);

    EXPECT_NE(packet->track_descriptor().thread().pid(), 0);
    EXPECT_NE(packet->track_descriptor().thread().tid(), 0);
    if (thread_id) {
      EXPECT_EQ(packet->track_descriptor().thread().tid(), thread_id);
    }

    EXPECT_FALSE(
        packet->track_descriptor().thread().has_reference_timestamp_us());
    EXPECT_FALSE(
        packet->track_descriptor().thread().has_reference_thread_time_us());

    if (filtering_enabled || thread_id) {
      EXPECT_FALSE(packet->track_descriptor().thread().has_thread_name());
    } else {
      EXPECT_EQ(packet->track_descriptor().thread().thread_name(), kTestThread);
    }

    if (!thread_id) {
      EXPECT_EQ(packet->track_descriptor().uuid(), default_track_uuid_);
      if (process_track_uuid_) {
        EXPECT_EQ(packet->track_descriptor().parent_uuid(),
                  process_track_uuid_);
      }

      ASSERT_TRUE(packet->track_descriptor().has_chrome_thread());
      EXPECT_EQ(perfetto::protos::ChromeThreadDescriptor::THREAD_MAIN,
                packet->track_descriptor().chrome_thread().thread_type());
    } else {
      EXPECT_EQ(packet->track_descriptor().uuid(),
                perfetto::ThreadTrack::ForThread(thread_id).uuid);
      if (process_track_uuid_) {
        EXPECT_EQ(packet->track_descriptor().parent_uuid(),
                  process_track_uuid_);
      }
    }

    EXPECT_EQ(packet->interned_data().event_categories_size(), 0);
    EXPECT_EQ(packet->interned_data().event_names_size(), 0);

    EXPECT_EQ(packet->sequence_flags(), 0u);
  }

  void ExpectThreadTimeCounterTrack(const perfetto::protos::TracePacket* packet,
                                    int thread_id = 0) {
    EXPECT_NE(packet->track_descriptor().uuid(), 0u);
    EXPECT_NE(packet->track_descriptor().parent_uuid(), 0u);

    if (!thread_id) {
      EXPECT_EQ(packet->track_descriptor().uuid(),
                default_extra_counter_track_uuids_[0]);
      EXPECT_EQ(packet->track_descriptor().parent_uuid(), default_track_uuid_);
      EXPECT_TRUE(packet->track_descriptor().counter().is_incremental());
      last_thread_time_ = 0;
    } else {
      constexpr uint64_t kAbsoluteThreadTimeTrackUuidBit =
          static_cast<uint64_t>(1u) << 33;
      EXPECT_EQ(packet->track_descriptor().uuid(),
                perfetto::ThreadTrack::ForThread(thread_id).uuid ^
                    kAbsoluteThreadTimeTrackUuidBit);
      EXPECT_EQ(packet->track_descriptor().parent_uuid(),
                perfetto::ThreadTrack::ForThread(thread_id).uuid);
      EXPECT_FALSE(packet->track_descriptor().counter().is_incremental());
    }

    EXPECT_EQ(packet->track_descriptor().counter().type(),
              perfetto::protos::CounterDescriptor::COUNTER_THREAD_TIME_NS);
    EXPECT_EQ(packet->track_descriptor().counter().unit_multiplier(), 1000u);
  }

  void ExpectProcessTrack(const perfetto::protos::TracePacket* packet,
                          bool filtering_enabled = false) {
    ASSERT_TRUE(packet->has_track_descriptor());
    ASSERT_TRUE(packet->track_descriptor().has_process());
    ASSERT_TRUE(packet->track_descriptor().has_chrome_process());

    EXPECT_NE(packet->track_descriptor().uuid(), 0u);
    EXPECT_FALSE(packet->track_descriptor().has_parent_uuid());

    process_track_uuid_ = packet->track_descriptor().uuid();

    EXPECT_NE(packet->track_descriptor().process().pid(), 0);

    if (filtering_enabled) {
      EXPECT_FALSE(packet->track_descriptor().process().has_process_name());
    } else {
      EXPECT_EQ(packet->track_descriptor().process().process_name(),
                kTestProcess);
    }

    EXPECT_EQ(packet->track_descriptor().chrome_process().process_type(),
              perfetto::protos::ChromeProcessDescriptor::PROCESS_BROWSER);

    // ProcessDescriptors is only emitted when incremental state was reset, and
    // thus also always serves as indicator for the state reset to the consumer
    // (for the TraceEventDataSource's TraceWriter sequence).
    EXPECT_EQ(packet->sequence_flags(),
              static_cast<uint32_t>(perfetto::protos::pbzero::TracePacket::
                                        SEQ_INCREMENTAL_STATE_CLEARED));
  }

  void ExpectChildTrack(const perfetto::protos::TracePacket* packet,
                        uint64_t uuid,
                        uint64_t parent_uuid) {
    EXPECT_EQ(packet->track_descriptor().uuid(), uuid);
    EXPECT_EQ(packet->track_descriptor().parent_uuid(), parent_uuid);
  }

  size_t ExpectStandardPreamble(size_t packet_index = 0,
                                bool privacy_filtering_enabled = false) {
    auto* pt_packet = producer_client()->GetFinalizedPacket(packet_index++);
    ExpectProcessTrack(pt_packet, privacy_filtering_enabled);

    auto* clock_packet = producer_client()->GetFinalizedPacket(packet_index++);
    ExpectClockSnapshotAndDefaults(clock_packet);

    auto* tt_packet = producer_client()->GetFinalizedPacket(packet_index++);
    ExpectThreadTrack(tt_packet, /*thread_id=*/0, /*min_timestamp=*/1u,
                      privacy_filtering_enabled);

    if (base::ThreadTicks::IsSupported()) {
      auto* ttt_packet = producer_client()->GetFinalizedPacket(packet_index++);
      ExpectThreadTimeCounterTrack(ttt_packet);
    }

    return packet_index;
  }

  void ExpectTraceEvent(const perfetto::protos::TracePacket* packet,
                        uint32_t category_iid,
                        uint32_t name_iid,
                        char phase,
                        uint32_t flags = 0,
                        uint64_t id = 0,
                        uint64_t absolute_timestamp = 0,
                        int32_t tid_override = 0,
                        int32_t pid_override = 0,
                        const perfetto::Track& track = perfetto::Track(),
                        int64_t explicit_thread_time = 0) {
    // All TrackEvents need incremental state for delta timestamps / interning.
    EXPECT_EQ(packet->sequence_flags(),
              static_cast<uint32_t>(perfetto::protos::pbzero::TracePacket::
                                        SEQ_NEEDS_INCREMENTAL_STATE));

    EXPECT_TRUE(packet->has_track_event());

    if (absolute_timestamp > 0) {
      EXPECT_EQ(packet->timestamp_clock_id(), kClockIdAbsolute);
      EXPECT_EQ(packet->timestamp(), absolute_timestamp);
    } else {
      // Default to kClockIdIncremental.
      EXPECT_FALSE(packet->has_timestamp_clock_id());
      EXPECT_TRUE(packet->has_timestamp());
      EXPECT_GE(packet->timestamp(), 0u);
      EXPECT_LE(last_timestamp_ + packet->timestamp(),
                static_cast<uint64_t>(
                    TRACE_TIME_TICKS_NOW().since_origin().InMicroseconds()));
      last_timestamp_ += packet->timestamp();
    }

    if (explicit_thread_time) {
      // Absolute thread time counter value for a different thread. Chrome uses
      // this for early java events. These events only specify thread time, no
      // instruction count.
      EXPECT_EQ(packet->track_event().extra_counter_track_uuids_size(), 1);
      EXPECT_NE(packet->track_event().extra_counter_track_uuids(0), 0u);
      EXPECT_EQ(packet->track_event().extra_counter_values_size(), 1);
      EXPECT_EQ(packet->track_event().extra_counter_values(0),
                explicit_thread_time);
    } else {
      EXPECT_EQ(packet->track_event().extra_counter_track_uuids_size(), 0);
      if (packet->track_event().extra_counter_values_size()) {
        // If the event is for a different thread or track, we shouldn't have
        // thread timestamps except for the explicit thread timestamps above.
        EXPECT_TRUE(tid_override == 0);
        EXPECT_EQ(track.uuid, 0u);
        int64_t thread_time_delta =
            packet->track_event().extra_counter_values()[0];
        EXPECT_LE(last_thread_time_ + thread_time_delta,
                  TRACE_TIME_TICKS_NOW().since_origin().InMicroseconds());
        last_thread_time_ += thread_time_delta;
      }
    }

    if (category_iid > 0) {
      ASSERT_EQ(packet->track_event().category_iids_size(), 1);
      EXPECT_EQ(packet->track_event().category_iids(0), category_iid);
    } else {
      EXPECT_EQ(packet->track_event().category_iids_size(), 0);
    }

    if (name_iid > 0) {
      EXPECT_EQ(packet->track_event().name_iid(), name_iid);
    }

    TrackEvent::Type track_event_type;
    switch (phase) {
      case TRACE_EVENT_PHASE_BEGIN:
        track_event_type = TrackEvent::TYPE_SLICE_BEGIN;
        break;
      case TRACE_EVENT_PHASE_END:
        track_event_type = TrackEvent::TYPE_SLICE_END;
        break;
      case TRACE_EVENT_PHASE_INSTANT:
        track_event_type = TrackEvent::TYPE_INSTANT;
        break;
      default:
        track_event_type = TrackEvent::TYPE_UNSPECIFIED;
        break;
    }

    if (track_event_type != TrackEvent::TYPE_UNSPECIFIED) {
      // Pid override is not supported for these events.
      ASSERT_FALSE(pid_override);

      EXPECT_EQ(packet->track_event().type(), track_event_type);
      if (phase == TRACE_EVENT_PHASE_INSTANT) {
        switch (flags & TRACE_EVENT_FLAG_SCOPE_MASK) {
          case TRACE_EVENT_SCOPE_GLOBAL:
            // Use the global track.
            EXPECT_EQ(packet->track_event().track_uuid(), 0u);
            break;

          case TRACE_EVENT_SCOPE_PROCESS:
            EXPECT_EQ(packet->track_event().track_uuid(), process_track_uuid_);
            break;

          case TRACE_EVENT_SCOPE_THREAD:
            if (!track) {
              // Default to thread track.
              EXPECT_FALSE(packet->track_event().has_track_uuid());
            } else {
              EXPECT_TRUE(packet->track_event().has_track_uuid());
              EXPECT_EQ(packet->track_event().track_uuid(), track.uuid);
            }
            break;
        }
      } else {
        if (!track) {
          // Default to thread track.
          EXPECT_FALSE(packet->track_event().has_track_uuid());
        } else {
          EXPECT_TRUE(packet->track_event().has_track_uuid());
          EXPECT_EQ(packet->track_event().track_uuid(), track.uuid);
        }
      }
    } else {
      // Track is cleared, to fall back on legacy tracks (async ids / thread
      // descriptor track).
      EXPECT_TRUE(packet->track_event().has_track_uuid());
      EXPECT_EQ(packet->track_event().track_uuid(), 0u);
    }

    // We don't emit the legacy event if we don't need it.
    bool needs_legacy_event = false;

    // These events need a phase.
    needs_legacy_event = track_event_type == TrackEvent::TYPE_UNSPECIFIED;

    // These events have some flag that is emitted in LegacyEvent.
    needs_legacy_event |=
        (flags & ~TRACE_EVENT_FLAG_SCOPE_MASK & ~TRACE_EVENT_FLAG_COPY &
         ~TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP);

    if (!needs_legacy_event) {
      EXPECT_FALSE(packet->track_event().has_legacy_event());
      return;
    }

    EXPECT_TRUE(packet->track_event().has_legacy_event());
    const auto& legacy_event = packet->track_event().legacy_event();

    if (track_event_type == TrackEvent::TYPE_UNSPECIFIED) {
      EXPECT_EQ(legacy_event.phase(), phase);
    }

    EXPECT_FALSE(legacy_event.has_instant_event_scope());

    switch (flags & (TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_HAS_LOCAL_ID |
                     TRACE_EVENT_FLAG_HAS_GLOBAL_ID)) {
      case TRACE_EVENT_FLAG_HAS_ID:
        EXPECT_EQ(legacy_event.unscoped_id(), id);
        EXPECT_EQ(legacy_event.local_id(), 0u);
        EXPECT_EQ(legacy_event.global_id(), 0u);
        break;
      case TRACE_EVENT_FLAG_HAS_LOCAL_ID:
        EXPECT_EQ(legacy_event.unscoped_id(), 0u);
        EXPECT_EQ(legacy_event.local_id(), id);
        EXPECT_EQ(legacy_event.global_id(), 0u);
        break;
      case TRACE_EVENT_FLAG_HAS_GLOBAL_ID:
        EXPECT_EQ(legacy_event.unscoped_id(), 0u);
        EXPECT_EQ(legacy_event.local_id(), 0u);
        EXPECT_EQ(legacy_event.global_id(), id);
        break;
      default:
        EXPECT_EQ(legacy_event.unscoped_id(), 0u);
        EXPECT_EQ(legacy_event.local_id(), 0u);
        EXPECT_EQ(legacy_event.global_id(), 0u);
        break;
    }

    EXPECT_EQ(legacy_event.use_async_tts(), flags & TRACE_EVENT_FLAG_ASYNC_TTS);

    switch (flags & (TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN)) {
      case TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN:
        EXPECT_EQ(legacy_event.flow_direction(),
                  TrackEvent::LegacyEvent::FLOW_INOUT);
        break;
      case TRACE_EVENT_FLAG_FLOW_OUT:
        EXPECT_EQ(legacy_event.flow_direction(),
                  TrackEvent::LegacyEvent::FLOW_OUT);
        break;
      case TRACE_EVENT_FLAG_FLOW_IN:
        EXPECT_EQ(legacy_event.flow_direction(),
                  TrackEvent::LegacyEvent::FLOW_IN);
        break;
      default:
        EXPECT_EQ(legacy_event.flow_direction(),
                  TrackEvent::LegacyEvent::FLOW_UNSPECIFIED);
        break;
    }

    EXPECT_EQ(legacy_event.bind_to_enclosing(),
              flags & TRACE_EVENT_FLAG_BIND_TO_ENCLOSING);

    if (track_event_type == TrackEvent::TYPE_UNSPECIFIED) {
      EXPECT_EQ(legacy_event.tid_override(), tid_override);
    }
    EXPECT_EQ(legacy_event.pid_override(), pid_override);
  }

  void ExpectEventCategories(
      const perfetto::protos::TracePacket* packet,
      std::initializer_list<std::pair<uint32_t, std::string>> entries) {
    ExpectInternedNames(packet->interned_data().event_categories(), entries);
  }

  void ExpectEventNames(
      const perfetto::protos::TracePacket* packet,
      std::initializer_list<std::pair<uint32_t, std::string>> entries) {
    ExpectInternedNames(packet->interned_data().event_names(), entries);
  }

  void ExpectDebugAnnotationNames(
      const perfetto::protos::TracePacket* packet,
      std::initializer_list<std::pair<uint32_t, std::string>> entries) {
    ExpectInternedNames(packet->interned_data().debug_annotation_names(),
                        entries);
  }

  template <typename T>
  void ExpectInternedNames(
      const google::protobuf::RepeatedPtrField<T>& field,
      std::initializer_list<std::pair<uint32_t, std::string>> entries) {
    ASSERT_EQ(field.size(), static_cast<int>(entries.size()));
    int i = 0;
    for (const auto& entry : entries) {
      EXPECT_EQ(field[i].iid(), entry.first);
      EXPECT_EQ(field[i].name(), entry.second);
      i++;
    }
  }

 protected:
  // Should be the first member.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestProducerClient> producer_client_;
  uint64_t last_timestamp_ = 0;
  int64_t last_thread_time_ = 0;
  uint64_t default_track_uuid_ = 0u;
  uint64_t process_track_uuid_ = 0u;
  std::vector<uint64_t> default_extra_counter_track_uuids_;

  std::string old_thread_name_;
  std::string old_process_name_;
};

void HasMetadataValue(const perfetto::protos::ChromeMetadata& entry,
                      const char* value) {
  EXPECT_TRUE(entry.has_string_value());
  EXPECT_EQ(entry.string_value(), value);
}

void HasMetadataValue(const perfetto::protos::ChromeMetadata& entry,
                      int value) {
  EXPECT_TRUE(entry.has_int_value());
  EXPECT_EQ(entry.int_value(), value);
}

void HasMetadataValue(const perfetto::protos::ChromeMetadata& entry,
                      bool value) {
  EXPECT_TRUE(entry.has_bool_value());
  EXPECT_EQ(entry.bool_value(), value);
}

void HasMetadataValue(const perfetto::protos::ChromeMetadata& entry,
                      const base::DictionaryValue& value) {
  EXPECT_TRUE(entry.has_json_value());

  std::unique_ptr<base::Value> child_dict =
      base::JSONReader::ReadDeprecated(entry.json_value());
  EXPECT_EQ(*child_dict, value);
}

template <typename T>
void MetadataHasNamedValue(const google::protobuf::RepeatedPtrField<
                               perfetto::protos::ChromeMetadata>& metadata,
                           const char* name,
                           const T& value) {
  for (int i = 0; i < metadata.size(); i++) {
    auto& entry = metadata[i];
    if (entry.name() == name) {
      HasMetadataValue(entry, value);
      return;
    }
  }

  NOTREACHED();
}

std::unique_ptr<base::DictionaryValue> AddJsonMetadataGenerator() {
  auto metadata = std::make_unique<base::DictionaryValue>();
  metadata->SetInteger("foo_int", 42);
  metadata->SetString("foo_str", "bar");
  metadata->SetBoolean("foo_bool", true);

  auto child_dict = std::make_unique<base::DictionaryValue>();
  child_dict->SetString("child_str", "child_val");
  metadata->Set("child_dict", std::move(child_dict));
  return metadata;
}

TEST_F(TraceEventDataSourceTest, MetadataGeneratorBeforeTracing) {
  auto* metadata_source = TraceEventMetadataSource::GetInstance();
  metadata_source->AddGeneratorFunction(
      base::BindRepeating(&AddJsonMetadataGenerator));

  metadata_source->StartTracing(producer_client(),
                                perfetto::DataSourceConfig());

  base::RunLoop wait_for_stop;
  metadata_source->StopTracing(wait_for_stop.QuitClosure());
  wait_for_stop.Run();

  auto& metadata = *producer_client()->GetChromeMetadata();
  EXPECT_EQ(4, metadata.size());
  MetadataHasNamedValue(metadata, "foo_int", 42);
  MetadataHasNamedValue(metadata, "foo_str", "bar");
  MetadataHasNamedValue(metadata, "foo_bool", true);

  auto child_dict = std::make_unique<base::DictionaryValue>();
  child_dict->SetString("child_str", "child_val");
  MetadataHasNamedValue(metadata, "child_dict", *child_dict);
}

TEST_F(TraceEventDataSourceTest, MetadataGeneratorWhileTracing) {
  auto* metadata_source = TraceEventMetadataSource::GetInstance();

  metadata_source->StartTracing(producer_client(),
                                perfetto::DataSourceConfig());
  metadata_source->AddGeneratorFunction(
      base::BindRepeating(&AddJsonMetadataGenerator));

  base::RunLoop wait_for_stop;
  metadata_source->StopTracing(wait_for_stop.QuitClosure());
  wait_for_stop.Run();

  auto& metadata = *producer_client()->GetChromeMetadata();
  EXPECT_EQ(4, metadata.size());
  MetadataHasNamedValue(metadata, "foo_int", 42);
  MetadataHasNamedValue(metadata, "foo_str", "bar");
  MetadataHasNamedValue(metadata, "foo_bool", true);

  auto child_dict = std::make_unique<base::DictionaryValue>();
  child_dict->SetString("child_str", "child_val");
  MetadataHasNamedValue(metadata, "child_dict", *child_dict);
}

TEST_F(TraceEventDataSourceTest, MultipleMetadataGenerators) {
  auto* metadata_source = TraceEventMetadataSource::GetInstance();
  metadata_source->AddGeneratorFunction(base::BindRepeating([]() {
    auto metadata = std::make_unique<base::DictionaryValue>();
    metadata->SetInteger("before_int", 42);
    return metadata;
  }));

  metadata_source->StartTracing(producer_client(),
                                perfetto::DataSourceConfig());
  metadata_source->AddGeneratorFunction(
      base::BindRepeating(&AddJsonMetadataGenerator));

  base::RunLoop wait_for_stop;
  metadata_source->StopTracing(wait_for_stop.QuitClosure());
  wait_for_stop.Run();

  auto& metadata = *producer_client()->GetChromeMetadata();
  EXPECT_EQ(4, metadata.size());
  MetadataHasNamedValue(metadata, "foo_int", 42);
  MetadataHasNamedValue(metadata, "foo_str", "bar");
  MetadataHasNamedValue(metadata, "foo_bool", true);

  auto child_dict = std::make_unique<base::DictionaryValue>();
  child_dict->SetString("child_str", "child_val");
  MetadataHasNamedValue(metadata, "child_dict", *child_dict);

  auto& metadata1 = *producer_client()->GetChromeMetadata(1);
  EXPECT_EQ(1, metadata1.size());
  MetadataHasNamedValue(metadata1, "before_int", 42);
}

TEST_F(TraceEventDataSourceTest, BasicTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_BEGIN0(kCategoryGroup, "bar");

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, TraceLogMetadataEvents) {
  CreateTraceEventDataSource();

  base::RunLoop wait_for_flush;
  TraceEventDataSource::GetInstance()->StopTracing(
      wait_for_flush.QuitClosure());
  wait_for_flush.Run();

  bool has_process_uptime_event = false;
  for (size_t i = 0; i < producer_client()->GetFinalizedPacketCount(); ++i) {
    auto* packet = producer_client()->GetFinalizedPacket(i);
    for (auto& event_name : packet->interned_data().event_names()) {
      if (event_name.name() == "process_uptime_seconds") {
        has_process_uptime_event = true;
        break;
      }
    }
  }

  EXPECT_TRUE(has_process_uptime_event);
}

TEST_F(TraceEventDataSourceTest, TimestampedTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0(
      kCategoryGroup, "bar", 42, 4242,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(424242));

  size_t packet_index = ExpectStandardPreamble();

  // Thread track for the overridden tid.
  auto* tt_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectThreadTrack(tt_packet, /*thread_id=*/4242);

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(
      e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
      TRACE_EVENT_PHASE_ASYNC_BEGIN,
      TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP | TRACE_EVENT_FLAG_HAS_ID, /*id=*/42u,
      /*absolute_timestamp=*/424242, /*tid_override=*/4242, /*pid_override=*/0,
      perfetto::ThreadTrack::ForThread(4242));

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, InstantTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT0(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, InstantTraceEventOnOtherThread) {
  CreateTraceEventDataSource();

  auto* category_group_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(kCategoryGroup);

  trace_event_internal::AddTraceEventWithThreadIdAndTimestamp(
      TRACE_EVENT_PHASE_INSTANT, category_group_enabled, "bar",
      trace_event_internal::kGlobalScope, trace_event_internal::kNoId,
      /*thread_id=*/1,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(10),
      TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP, trace_event_internal::kNoId);

  size_t packet_index = ExpectStandardPreamble();

  // Thread track for the overridden tid.
  auto* tt_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectThreadTrack(tt_packet, /*thread_id=*/1);

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT,
                   TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP, /*id=*/0u,
                   /*absolute_timestamp=*/10, /*tid_override=*/1,
                   /*pid_override=*/0, perfetto::ThreadTrack::ForThread(1));

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, EventWithStringArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD,
                       "arg1_name", "arg1_val", "arg2_name", "arg2_val");

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].name_iid(), 1u);
  EXPECT_EQ(annotations[0].string_value(), "arg1_val");
  EXPECT_EQ(annotations[1].name_iid(), 2u);
  EXPECT_EQ(annotations[1].string_value(), "arg2_val");

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ExpectDebugAnnotationNames(e_packet, {{1u, "arg1_name"}, {2u, "arg2_name"}});
}

TEST_F(TraceEventDataSourceTest, EventWithCopiedStrings) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar",
                       TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_COPY,
                       "arg1_name", "arg1_val", "arg2_name", "arg2_val");

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT,
                   TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_COPY);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].name_iid(), 1u);
  EXPECT_EQ(annotations[0].string_value(), "arg1_val");
  EXPECT_EQ(annotations[1].name_iid(), 2u);
  EXPECT_EQ(annotations[1].string_value(), "arg2_val");

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ExpectDebugAnnotationNames(e_packet, {{1u, "arg1_name"}, {2u, "arg2_name"}});
}

TEST_F(TraceEventDataSourceTest, EventWithUIntArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42u, "bar", 4242u);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].uint_value(), 42u);
  EXPECT_EQ(annotations[1].uint_value(), 4242u);
}

TEST_F(TraceEventDataSourceTest, EventWithIntArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42, "bar", 4242);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].int_value(), 42);
  EXPECT_EQ(annotations[1].int_value(), 4242);
}

TEST_F(TraceEventDataSourceTest, EventWithBoolArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       true, "bar", false);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_TRUE(annotations[0].has_bool_value());
  EXPECT_EQ(annotations[0].bool_value(), true);
  EXPECT_TRUE(annotations[1].has_bool_value());
  EXPECT_EQ(annotations[1].int_value(), false);
}

TEST_F(TraceEventDataSourceTest, EventWithDoubleArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42.42, "bar", 4242.42);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].double_value(), 42.42);
  EXPECT_EQ(annotations[1].double_value(), 4242.42);
}

TEST_F(TraceEventDataSourceTest, EventWithPointerArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       reinterpret_cast<void*>(0xBEEF), "bar",
                       reinterpret_cast<void*>(0xF00D));

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].pointer_value(), static_cast<uintptr_t>(0xBEEF));
  EXPECT_EQ(annotations[1].pointer_value(), static_cast<uintptr_t>(0xF00D));
}

TEST_F(TraceEventDataSourceTest, EventWithConvertableArgs) {
  CreateTraceEventDataSource();

  static const char kArgValue1[] = "\"conv_value1\"";
  static const char kArgValue2[] = "\"conv_value2\"";

  int num_calls = 0;

  class Convertable : public base::trace_event::ConvertableToTraceFormat {
   public:
    explicit Convertable(int* num_calls, const char* arg_value)
        : num_calls_(num_calls), arg_value_(arg_value) {}
    ~Convertable() override = default;

    void AppendAsTraceFormat(std::string* out) const override {
      (*num_calls_)++;
      out->append(arg_value_);
    }

   private:
    int* num_calls_;
    const char* arg_value_;
  };

  std::unique_ptr<Convertable> conv1(new Convertable(&num_calls, kArgValue1));
  std::unique_ptr<Convertable> conv2(new Convertable(&num_calls, kArgValue2));

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD,
                       "foo_arg1", std::move(conv1), "foo_arg2",
                       std::move(conv2));

  EXPECT_EQ(2, num_calls);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].legacy_json_value(), kArgValue1);
  EXPECT_EQ(annotations[1].legacy_json_value(), kArgValue2);
}

TEST_F(TraceEventDataSourceTest, TaskExecutionEvent) {
  CreateTraceEventDataSource();

  INTERNAL_TRACE_EVENT_ADD(
      TRACE_EVENT_PHASE_INSTANT, "toplevel", "ThreadControllerImpl::RunTask",
      TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_TYPED_PROTO_ARGS, "src_file",
      "my_file", "src_func", "my_func");
  INTERNAL_TRACE_EVENT_ADD(
      TRACE_EVENT_PHASE_INSTANT, "toplevel", "ThreadControllerImpl::RunTask",
      TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_TYPED_PROTO_ARGS, "src_file",
      "my_file", "src_func", "my_func");

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 0);

  EXPECT_EQ(e_packet->track_event().task_execution().posted_from_iid(), 1u);
  const auto& locations = e_packet->interned_data().source_locations();
  EXPECT_EQ(locations.size(), 1);
  EXPECT_EQ(locations[0].file_name(), "my_file");
  EXPECT_EQ(locations[0].function_name(), "my_func");

  // Second event should refer to the same interning entries.
  auto* e_packet2 = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet2, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  EXPECT_EQ(e_packet2->track_event().task_execution().posted_from_iid(), 1u);
  EXPECT_EQ(e_packet2->interned_data().source_locations().size(), 0);
}

TEST_F(TraceEventDataSourceTest, TaskExecutionEventWithoutFunction) {
  CreateTraceEventDataSource();

  INTERNAL_TRACE_EVENT_ADD(
      TRACE_EVENT_PHASE_INSTANT, "toplevel", "ThreadControllerImpl::RunTask",
      TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_TYPED_PROTO_ARGS, "src",
      "my_file");

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 0);

  EXPECT_EQ(e_packet->track_event().task_execution().posted_from_iid(), 1u);
  const auto& locations = e_packet->interned_data().source_locations();
  EXPECT_EQ(locations.size(), 1);
  EXPECT_EQ(locations[0].file_name(), "my_file");
  EXPECT_FALSE(locations[0].has_function_name());
}

TEST_F(TraceEventDataSourceTest, UpdateDurationOfCompleteEvent) {
  CreateTraceEventDataSource();

  static const char kEventName[] = "bar";

  auto* category_group_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(kCategoryGroup);

  trace_event_internal::TraceID trace_event_trace_id =
      trace_event_internal::kNoId;

  // COMPLETE events are split into a BEGIN/END event pair. Adding the event
  // writes the BEGIN event immediately.
  auto handle = trace_event_internal::AddTraceEventWithThreadIdAndTimestamp(
      TRACE_EVENT_PHASE_COMPLETE, category_group_enabled, kEventName,
      trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),
      /*thread_id=*/1,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(10),
      trace_event_trace_id.id_flags() | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,
      trace_event_internal::kNoId);

  size_t packet_index = ExpectStandardPreamble();

  // Thread track for the overridden tid.
  auto* tt_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectThreadTrack(tt_packet, /*thread_id=*/1);

  auto* b_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(
      b_packet, /*category_iid=*/1u, /*name_iid=*/1u, TRACE_EVENT_PHASE_BEGIN,
      TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP | TRACE_EVENT_FLAG_HAS_ID, /*id=*/0u,
      /*absolute_timestamp=*/10, /*tid_override=*/1, /*pid_override=*/0,
      perfetto::ThreadTrack::ForThread(1));

  // Updating the duration of the event as it goes out of scope results in the
  // corresponding END event being written. These END events don't contain any
  // event names or categories in the proto format.
  base::trace_event::TraceLog::GetInstance()->UpdateTraceEventDurationExplicit(
      category_group_enabled, kEventName, handle, /*thread_id=*/1,
      /*explicit_timestamps=*/true,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(30),
      base::ThreadTicks(), base::trace_event::ThreadInstructionCount());

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END, TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,
                   /*id=*/0u,
                   /*absolute_timestamp=*/30, /*tid_override=*/1,
                   /*pid_override=*/0, perfetto::ThreadTrack::ForThread(1));

  // Updating the duration of an event that wasn't added before tracing begun
  // will only emit an END event, again without category or name.
  handle.event_index = 0;
  base::trace_event::TraceLog::GetInstance()->UpdateTraceEventDurationExplicit(
      category_group_enabled, "other_event_name", handle, /*thread_id=*/1,
      /*explicit_timestamps=*/true,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(40),
      base::ThreadTicks(), base::trace_event::ThreadInstructionCount());

  auto* e2_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e2_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END, TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,
                   /*id=*/0u,
                   /*absolute_timestamp=*/40, /*tid_override=*/1,
                   /*pid_override=*/0, perfetto::ThreadTrack::ForThread(1));

  // Complete event for the current thread emits thread time, too.
  trace_event_internal::AddTraceEventWithThreadIdAndTimestamp(
      TRACE_EVENT_PHASE_COMPLETE, category_group_enabled, kEventName,
      trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),
      base::PlatformThread::CurrentId(),
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(10),
      trace_event_trace_id.id_flags() | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,
      trace_event_internal::kNoId);

  auto* b2_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(
      b2_packet, /*category_iid=*/1u, /*name_iid=*/1u, TRACE_EVENT_PHASE_BEGIN,
      TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP | TRACE_EVENT_FLAG_HAS_ID, /*id=*/0u,
      /*absolute_timestamp=*/10, /*tid_override=*/0, /*pid_override=*/0);
}

TEST_F(TraceEventDataSourceTest, ExplicitThreadTimeForDifferentThread) {
  CreateTraceEventDataSource();

  static const char kEventName[] = "bar";

  auto* category_group_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(kCategoryGroup);

  trace_event_internal::TraceID trace_event_trace_id =
      trace_event_internal::kNoId;

  // Chrome's main thread buffers and later flushes EarlyJava events on behalf
  // of other threads, including explicit thread time values. Such an event
  // should add descriptors for the other thread's track and for an absolute
  // thread time track for the other thread.
  trace_event_internal::AddTraceEventWithThreadIdAndTimestamps(
      TRACE_EVENT_PHASE_BEGIN, category_group_enabled, kEventName,
      trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),
      /*thread_id=*/1,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(10),
      base::ThreadTicks() + base::TimeDelta::FromMicroseconds(20),
      trace_event_trace_id.id_flags() | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP);

  size_t packet_index = ExpectStandardPreamble();

  // Thread track for the overridden tid.
  auto* tt_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectThreadTrack(tt_packet, /*thread_id=*/1);

  // Absolute thread time track for the overridden tid.
  auto* ttt_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectThreadTimeCounterTrack(ttt_packet, /*thread_id=*/1);

  auto* b_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(
      b_packet, /*category_iid=*/1u, /*name_iid=*/1u, TRACE_EVENT_PHASE_BEGIN,
      TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP | TRACE_EVENT_FLAG_HAS_ID, /*id=*/0u,
      /*absolute_timestamp=*/10, /*tid_override=*/1, /*pid_override=*/0,
      perfetto::ThreadTrack::ForThread(1), /*explicit_thread_time=*/20);
}

TEST_F(TraceEventDataSourceTest, TrackSupportOnBeginAndEndWithLambda) {
  CreateTraceEventDataSource();

  auto track = perfetto::Track(1);
  bool begin_called = false;
  bool end_called = false;

  TRACE_EVENT_BEGIN("browser", "bar", track,
                    [&](perfetto::EventContext ctx) { begin_called = true; });
  EXPECT_TRUE(begin_called);

  TRACE_EVENT_END("browser", track,
                  [&](perfetto::EventContext ctx) { end_called = true; });
  EXPECT_TRUE(end_called);

  size_t packet_index = ExpectStandardPreamble();

  auto* b_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(b_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0,
                   /*pid_override=*/0, track);

  ExpectEventCategories(b_packet, {{1u, "browser"}});
  ExpectEventNames(b_packet, {{1u, "bar"}});

  auto* t_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectChildTrack(t_packet, track.uuid, track.parent_uuid);

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);

  ExpectTraceEvent(e_packet, /*category_iid=*/0, /*name_iid=*/0,
                   TRACE_EVENT_PHASE_END, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0,
                   /*pid_override=*/0, track);
}

TEST_F(TraceEventDataSourceTest, TrackSupportOnBeginAndEnd) {
  CreateTraceEventDataSource();

  auto track = perfetto::Track(1);

  TRACE_EVENT_BEGIN("browser", "bar", track);
  TRACE_EVENT_END("browser", track);

  size_t packet_index = ExpectStandardPreamble();

  auto* b_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(b_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0,
                   /*pid_override=*/0, track);

  ExpectEventCategories(b_packet, {{1u, "browser"}});
  ExpectEventNames(b_packet, {{1u, "bar"}});

  auto* t_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectChildTrack(t_packet, track.uuid, track.parent_uuid);

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);

  ExpectTraceEvent(e_packet, /*category_iid=*/0, /*name_iid=*/0,
                   TRACE_EVENT_PHASE_END, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0,
                   /*pid_override=*/0, track);
}

TEST_F(TraceEventDataSourceTest, TrackSupportWithTimestamp) {
  CreateTraceEventDataSource();

  auto timestamp =
      TRACE_TIME_TICKS_NOW() - base::TimeDelta::FromMicroseconds(100);
  auto track = perfetto::Track(1);

  TRACE_EVENT_BEGIN("browser", "bar", track, timestamp);

  size_t packet_index = ExpectStandardPreamble();

  auto* b_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(
      b_packet, /*category_iid=*/1u, /*name_iid=*/1u, TRACE_EVENT_PHASE_BEGIN,
      /*flags=*/0, /*id=*/0,
      /*absolute_timestamp=*/timestamp.since_origin().InMicroseconds(),
      /*tid_override=*/0,
      /*pid_override=*/0, track);

  ExpectEventCategories(b_packet, {{1u, "browser"}});
  ExpectEventNames(b_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, TrackSupportWithTimestampAndLambda) {
  CreateTraceEventDataSource();

  auto timestamp =
      TRACE_TIME_TICKS_NOW() - base::TimeDelta::FromMicroseconds(100);
  auto track = perfetto::Track(1);
  bool lambda_called = false;

  TRACE_EVENT_BEGIN("browser", "bar", track, timestamp,
                    [&](perfetto::EventContext ctx) { lambda_called = true; });

  EXPECT_TRUE(lambda_called);

  size_t packet_index = ExpectStandardPreamble();

  auto* b_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(
      b_packet, /*category_iid=*/1u, /*name_iid=*/1u, TRACE_EVENT_PHASE_BEGIN,
      /*flags=*/0, /*id=*/0,
      /*absolute_timestamp=*/timestamp.since_origin().InMicroseconds(),
      /*tid_override=*/0,
      /*pid_override=*/0, track);

  ExpectEventCategories(b_packet, {{1u, "browser"}});
  ExpectEventNames(b_packet, {{1u, "bar"}});
}

// TODO(ddrone): following tests should be re-enabled once we figure out how
// tracks on scoped events supposed to work
TEST_F(TraceEventDataSourceTest, DISABLED_TrackSupport) {
  CreateTraceEventDataSource();

  auto track = perfetto::Track(1);

  { TRACE_EVENT("browser", "bar", track); }

  size_t packet_index = ExpectStandardPreamble();

  auto* b_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(b_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0,
                   /*pid_override=*/0, track);

  ExpectEventCategories(b_packet, {{1u, "browser"}});
  ExpectEventNames(b_packet, {{1u, "bar"}});

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0, /*name_iid=*/0,
                   TRACE_EVENT_PHASE_END, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0,
                   /*pid_override=*/0, track);
}

TEST_F(TraceEventDataSourceTest, DISABLED_TrackSupportWithLambda) {
  CreateTraceEventDataSource();

  auto track = perfetto::Track(1);
  bool lambda_called = false;

  {
    TRACE_EVENT("browser", "bar", track,
                [&](perfetto::EventContext ctx) { lambda_called = true; });
  }

  EXPECT_TRUE(lambda_called);

  size_t packet_index = ExpectStandardPreamble();

  auto* b_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(b_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0,
                   /*pid_override=*/0, track);

  ExpectEventCategories(b_packet, {{1u, "browser"}});
  ExpectEventNames(b_packet, {{1u, "bar"}});

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0, /*name_iid=*/0,
                   TRACE_EVENT_PHASE_END, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0,
                   /*pid_override=*/0, track);
}

// TODO(eseckler): Add a test with multiple events + same strings (cat, name,
// arg names).

// TODO(eseckler): Add a test with multiple events + same strings with reset.

TEST_F(TraceEventDataSourceTest, InternedStrings) {
  CreateTraceEventDataSource();

  size_t packet_index = 0u;
  for (size_t i = 0; i < 2; i++) {
    TRACE_EVENT_INSTANT1("cat1", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 4);
    TRACE_EVENT_INSTANT1("cat1", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 2);
    TRACE_EVENT_INSTANT1("cat2", "e2", TRACE_EVENT_SCOPE_THREAD, "arg2", 1);

    packet_index = ExpectStandardPreamble(packet_index);

    // First packet needs to emit new interning entries
    auto* e_packet1 = producer_client()->GetFinalizedPacket(packet_index++);
    ExpectTraceEvent(e_packet1, /*category_iid=*/1u, /*name_iid=*/1u,
                     TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

    const auto& annotations1 = e_packet1->track_event().debug_annotations();
    EXPECT_EQ(annotations1.size(), 1);
    EXPECT_EQ(annotations1[0].name_iid(), 1u);
    EXPECT_EQ(annotations1[0].int_value(), 4);

    ExpectEventCategories(e_packet1, {{1u, "cat1"}});
    ExpectEventNames(e_packet1, {{1u, "e1"}});
    ExpectDebugAnnotationNames(e_packet1, {{1u, "arg1"}});

    // Second packet refers to the interning entries from packet 1.
    auto* e_packet2 = producer_client()->GetFinalizedPacket(packet_index++);
    ExpectTraceEvent(e_packet2, /*category_iid=*/1u, /*name_iid=*/1u,
                     TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

    const auto& annotations2 = e_packet2->track_event().debug_annotations();
    EXPECT_EQ(annotations2.size(), 1);
    EXPECT_EQ(annotations2[0].name_iid(), 1u);
    EXPECT_EQ(annotations2[0].int_value(), 2);

    ExpectEventCategories(e_packet2, {});
    ExpectEventNames(e_packet2, {});
    ExpectDebugAnnotationNames(e_packet2, {});

    // Third packet uses different names, so emits new entries.
    auto* e_packet3 = producer_client()->GetFinalizedPacket(packet_index++);
    ExpectTraceEvent(e_packet3, /*category_iid=*/2u, /*name_iid=*/2u,
                     TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

    const auto& annotations3 = e_packet3->track_event().debug_annotations();
    EXPECT_EQ(annotations3.size(), 1);
    EXPECT_EQ(annotations3[0].name_iid(), 2u);
    EXPECT_EQ(annotations3[0].int_value(), 1);

    ExpectEventCategories(e_packet3, {{2u, "cat2"}});
    ExpectEventNames(e_packet3, {{2u, "e2"}});
    ExpectDebugAnnotationNames(e_packet3, {{2u, "arg2"}});

    // Resetting the interning state causes ThreadDescriptor and interning
    // entries to be emitted again, with the same interning IDs.
    TraceEventDataSource::GetInstance()->ClearIncrementalState();
  }
}

TEST_F(TraceEventDataSourceTest, FilteringSimpleTraceEvent) {
  CreateTraceEventDataSource(/* privacy_filtering_enabled =*/true);
  TRACE_EVENT_BEGIN0(kCategoryGroup, "bar");

  size_t packet_index = ExpectStandardPreamble(
      /*start_packet_index=*/0u,
      /*privacy_filtering_enabled=*/true);

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ExpectDebugAnnotationNames(e_packet, {});
}

TEST_F(TraceEventDataSourceTest, FilteringEventWithArgs) {
  CreateTraceEventDataSource(/* privacy_filtering_enabled =*/true);
  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42, "bar", "string_val");

  size_t packet_index = ExpectStandardPreamble(
      /*start_packet_index=*/0u,
      /*privacy_filtering_enabled=*/true);

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 0);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ExpectDebugAnnotationNames(e_packet, {});
}

TEST_F(TraceEventDataSourceTest, FilteringEventWithFlagCopy) {
  CreateTraceEventDataSource(/* privacy_filtering_enabled =*/true);
  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar",
                       TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_COPY,
                       "arg1_name", "arg1_val", "arg2_name", "arg2_val");
  TRACE_EVENT_INSTANT2(kCategoryGroup, "javaName",
                       TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_COPY |
                           TRACE_EVENT_FLAG_JAVA_STRING_LITERALS,
                       "arg1_name", "arg1_val", "arg2_name", "arg2_val");

  size_t packet_index = ExpectStandardPreamble(
      /*start_packet_index=*/0u,
      /*privacy_filtering_enabled=*/true);

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 0);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "PRIVACY_FILTERED"}});
  ExpectDebugAnnotationNames(e_packet, {});

  e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/2u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations2 = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations2.size(), 0);

  ExpectEventNames(e_packet, {{2u, "javaName"}});
  ExpectDebugAnnotationNames(e_packet, {});
}

TEST_F(TraceEventDataSourceTest, FilteringMetadataSource) {
  auto* metadata_source = TraceEventMetadataSource::GetInstance();
  metadata_source->AddGeneratorFunction(base::BindRepeating([]() {
    auto metadata = std::make_unique<base::DictionaryValue>();
    metadata->SetInteger("foo_int", 42);
    metadata->SetString("foo_str", "bar");
    metadata->SetBoolean("foo_bool", true);

    auto child_dict = std::make_unique<base::DictionaryValue>();
    child_dict->SetString("child_str", "child_val");
    metadata->Set("child_dict", std::move(child_dict));
    return metadata;
  }));

  perfetto::DataSourceConfig config;
  config.mutable_chrome_config()->set_privacy_filtering_enabled(true);
  metadata_source->StartTracing(producer_client(), config);

  base::RunLoop wait_for_stop;
  metadata_source->StopTracing(wait_for_stop.QuitClosure());
  wait_for_stop.Run();

  EXPECT_FALSE(producer_client()->GetChromeMetadata());
}

TEST_F(TraceEventDataSourceTest, ProtoMetadataSource) {
  CreateTraceEventDataSource();
  auto* metadata_source = TraceEventMetadataSource::GetInstance();
  metadata_source->AddGeneratorFunction(base::BindRepeating(
      [](perfetto::protos::pbzero::ChromeMetadataPacket* metadata,
         bool privacy_filtering_enabled) {
        EXPECT_TRUE(privacy_filtering_enabled);
        auto* field1 = metadata->set_background_tracing_metadata();
        auto* rule = field1->set_triggered_rule();
        rule->set_trigger_type(
            perfetto::protos::pbzero::BackgroundTracingMetadata::TriggerRule::
                MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE);
        rule->set_histogram_rule()->set_histogram_min_trigger(123);
      }));

  perfetto::DataSourceConfig config;
  config.mutable_chrome_config()->set_privacy_filtering_enabled(true);
  metadata_source->StartTracing(producer_client(), config);

  base::RunLoop wait_for_stop;
  metadata_source->StopTracing(wait_for_stop.QuitClosure());
  wait_for_stop.Run();

  const auto* metadata = producer_client()->GetProtoChromeMetadata();
  EXPECT_TRUE(metadata->has_background_tracing_metadata());
  const auto& rule = metadata->background_tracing_metadata().triggered_rule();
  EXPECT_EQ(perfetto::protos::BackgroundTracingMetadata::TriggerRule::
                MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE,
            rule.trigger_type());
  EXPECT_EQ(123, rule.histogram_rule().histogram_min_trigger());
}

class TraceEventDataSourceNoInterningTest : public TraceEventDataSourceTest {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kPerfettoDisableInterning);
    TraceEventDataSourceTest::SetUp();
  }
};

TEST_F(TraceEventDataSourceNoInterningTest, InterningScopedToPackets) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT1("cat1", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 4);
  TRACE_EVENT_INSTANT1("cat1", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 2);
  TRACE_EVENT_INSTANT1("cat2", "e2", TRACE_EVENT_SCOPE_THREAD, "arg2", 1);

  size_t packet_index = ExpectStandardPreamble();

  // First packet needs to emit new interning entries
  auto* e_packet1 = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet1, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations1 = e_packet1->track_event().debug_annotations();
  EXPECT_EQ(annotations1.size(), 1);
  EXPECT_EQ(annotations1[0].name_iid(), 1u);
  EXPECT_EQ(annotations1[0].int_value(), 4);

  ExpectEventCategories(e_packet1, {{1u, "cat1"}});
  ExpectEventNames(e_packet1, {{1u, "e1"}});
  ExpectDebugAnnotationNames(e_packet1, {{1u, "arg1"}});

  // Second packet reemits the entries the same way.
  auto* e_packet2 = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet2, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations2 = e_packet2->track_event().debug_annotations();
  EXPECT_EQ(annotations2.size(), 1);
  EXPECT_EQ(annotations2[0].name_iid(), 1u);
  EXPECT_EQ(annotations2[0].int_value(), 2);

  ExpectEventCategories(e_packet1, {{1u, "cat1"}});
  ExpectEventNames(e_packet1, {{1u, "e1"}});
  ExpectDebugAnnotationNames(e_packet1, {{1u, "arg1"}});

  // Third packet emits entries with the same IDs but different strings.
  auto* e_packet3 = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet3, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations3 = e_packet3->track_event().debug_annotations();
  EXPECT_EQ(annotations3.size(), 1);
  EXPECT_EQ(annotations3[0].name_iid(), 1u);
  EXPECT_EQ(annotations3[0].int_value(), 1);

  ExpectEventCategories(e_packet3, {{1u, "cat2"}});
  ExpectEventNames(e_packet3, {{1u, "e2"}});
  ExpectDebugAnnotationNames(e_packet3, {{1u, "arg2"}});
}

TEST_F(TraceEventDataSourceTest, StartupTracingTimeout) {
  CreateTraceEventDataSource(/* privacy_filtering_enabled = */ false,
                             /* start_trace = */ false);
  PerfettoTracedProcess::ResetTaskRunnerForTesting(
      base::SequencedTaskRunnerHandle::Get());
  constexpr char kStartupTestEvent1[] = "startup_registry";
  auto* data_source = TraceEventDataSource::GetInstance();
  PerfettoTracedProcess::Get()->AddDataSource(data_source);

  // Start startup tracing with no timeout. This would cause startup tracing to
  // abort and flush as soon the current thread can run tasks.
  producer_client()->set_startup_tracing_timeout_for_testing(base::TimeDelta());
  producer_client()->SetupStartupTracing(base::trace_event::TraceConfig(),
                                         /*privacy_filtering_enabled=*/true);

  // The trace event will be added to the SMB for the (soon to be aborted)
  // startup tracing session, since the abort didn't run yet.
  TRACE_EVENT_BEGIN0(kCategoryGroup, kStartupTestEvent1);

  // Run task on background thread to add trace events while aborting and
  // starting tracing on the data source. This is to test we do not have any
  // crashes when a background thread is trying to create trace writers when
  // aborting startup tracing and resetting tracing for the next session.
  auto wait_for_start_tracing = std::make_unique<base::WaitableEvent>();
  base::WaitableEvent* wait_ptr = wait_for_start_tracing.get();
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](std::unique_ptr<base::WaitableEvent> wait_for_start_tracing) {
            // This event can be hit anytime before startup tracing is
            // aborted to after tracing is started using the producer.
            TRACE_EVENT_BEGIN0(kCategoryGroup, "maybe_lost");
            base::ScopedAllowBaseSyncPrimitivesForTesting allow;
            wait_for_start_tracing->Wait();
            // This event can be hit while flushing for startup registry or when
            // tracing is started or when already stopped tracing.
            TRACE_EVENT_BEGIN0(kCategoryGroup, "maybe_lost");
          },
          std::move(wait_for_start_tracing)));

  // Let tasks run on this thread, which should abort startup tracing and flush
  // TraceLog, since the data source hasn't been started by a producer.
  producer_client()->OnThreadPoolAvailable();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(base::trace_event::TraceLog::GetInstance()->IsEnabled());

  // Start tracing while flush is running.
  perfetto::DataSourceConfig config;
  data_source->StartTracing(producer_client(), config);
  wait_ptr->Signal();

  // Verify that the trace buffer does not have the event added to startup
  // registry.
  producer_client()->FlushPacketIfPossible();
  std::set<std::string> event_names;
  for (const auto& packet : producer_client()->finalized_packets()) {
    if (packet->has_interned_data()) {
      for (const auto& name : packet->interned_data().event_names()) {
        event_names.insert(name.name());
      }
    }
  }
  EXPECT_EQ(event_names.end(), event_names.find(kStartupTestEvent1));

  // Stop tracing must be called even if tracing is not started to clear the
  // pending task.
  base::RunLoop wait_for_stop;
  data_source->StopTracing(base::BindOnce(
      [](const base::RepeatingClosure& quit_closure) { quit_closure.Run(); },
      wait_for_stop.QuitClosure()));

  wait_for_stop.Run();
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOff) {
  TRACE_EVENT_BEGIN("log", "LogMessage", [](perfetto::EventContext ctx) {
    ADD_FAILURE() << "lambda was called when tracing was off";
  });

  TRACE_EVENT_END("log", [](perfetto::EventContext ctx) {
    ADD_FAILURE() << "lambda was called when tracing was off";
  });
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOnBegin) {
  CreateTraceEventDataSource();

  bool begin_called = false;

  TRACE_EVENT_BEGIN("browser", "bar", [&](perfetto::EventContext ctx) {
    begin_called = true;
    ctx.event()->set_log_message()->set_body_iid(42);
  });

  EXPECT_TRUE(begin_called);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOnEnd) {
  CreateTraceEventDataSource();

  bool end_called = false;

  TRACE_EVENT_END("browser", [&](perfetto::EventContext ctx) {
    end_called = true;
    ctx.event()->set_log_message()->set_body_iid(42);
  });

  EXPECT_TRUE(end_called);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END);

  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOnBeginAndEnd) {
  CreateTraceEventDataSource();

  TRACE_EVENT_BEGIN("browser", "bar", [&](perfetto::EventContext ctx) {
    ctx.event()->set_log_message()->set_body_iid(42);
  });
  TRACE_EVENT_END("browser", [&](perfetto::EventContext ctx) {
    ctx.event()->set_log_message()->set_body_iid(84);
  });

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);

  e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END);

  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 84u);
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOnInstant) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT("browser", "bar", TRACE_EVENT_SCOPE_THREAD,
                      [&](perfetto::EventContext ctx) {
                        ctx.event()->set_log_message()->set_body_iid(42);
                      });

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOnScoped) {
  CreateTraceEventDataSource();

  // Use a if statement with no brackets to ensure that the Scoped TRACE_EVENT
  // macro properly emits the end event when leaving the single expression
  // associated with the if(true) statement.
  if (true)
    TRACE_EVENT("browser", "bar", [&](perfetto::EventContext ctx) {
      ctx.event()->set_log_message()->set_body_iid(42);
    });

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);

  e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END);

  EXPECT_FALSE(e_packet->track_event().has_log_message());
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOnScopedCapture) {
  CreateTraceEventDataSource();

  bool called = false;
  {
    TRACE_EVENT("browser", "bar", [&](perfetto::EventContext ctx) {
      called = true;
      ctx.event()->set_log_message()->set_body_iid(42);
    });
  }

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);

  e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END);

  EXPECT_FALSE(e_packet->track_event().has_log_message());
  EXPECT_TRUE(called);
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOnScopedMultipleEvents) {
  CreateTraceEventDataSource();

  {
    TRACE_EVENT("browser", "bar", [&](perfetto::EventContext ctx) {
      ctx.event()->set_log_message()->set_body_iid(42);
    });
    TRACE_EVENT("browser", "bar", [&](perfetto::EventContext ctx) {
      ctx.event()->set_log_message()->set_body_iid(43);
    });
  }

  size_t packet_index = ExpectStandardPreamble();

  // The first TRACE_EVENT begin.
  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);

  // The second TRACE_EVENT begin.
  e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 43u);

  // The second TRACE_EVENT end.
  e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END);

  EXPECT_FALSE(e_packet->track_event().has_log_message());

  // The first TRACE_EVENT end.
  e_packet = producer_client()->GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END);
  EXPECT_FALSE(e_packet->track_event().has_log_message());
}

TEST_F(TraceEventDataSourceTest, HistogramSampleTraceConfigEmpty) {
  base::trace_event::TraceConfig trace_config(
      "-*,disabled-by-default-histogram_samples",
      base::trace_event::RECORD_UNTIL_FULL);

  CreateTraceEventDataSource(/*privacy_filtering_enabled=*/false,
                             /*start_trace=*/true, trace_config.ToString());

  UMA_HISTOGRAM_BOOLEAN("Foo.Bar", true);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);

  ExpectEventCategories(e_packet,
                        {{1u, TRACE_DISABLED_BY_DEFAULT("histogram_samples")}});
  ExpectEventNames(e_packet, {{1u, "HistogramSample"}});
  ASSERT_TRUE(e_packet->track_event().has_chrome_histogram_sample());
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().name_hash(),
            base::HashMetricName("Foo.Bar"));
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().sample(), 1u);
}

TEST_F(TraceEventDataSourceTest, HistogramSampleTraceConfigNotEmpty) {
  base::trace_event::TraceConfig trace_config(
      "-*,disabled-by-default-histogram_samples",
      base::trace_event::RECORD_UNTIL_FULL);
  trace_config.EnableHistogram("Foo1.Bar1");
  trace_config.EnableHistogram("Foo3.Bar3");

  CreateTraceEventDataSource(/*privacy_filtering_enabled=*/false,
                             /*start_trace=*/true, trace_config.ToString());

  UMA_HISTOGRAM_BOOLEAN("Foo1.Bar1", true);
  UMA_HISTOGRAM_BOOLEAN("Foo2.Bar2", true);
  UMA_HISTOGRAM_BOOLEAN("Foo3.Bar3", true);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);

  ExpectEventCategories(e_packet,
                        {{1u, TRACE_DISABLED_BY_DEFAULT("histogram_samples")}});
  ExpectEventNames(e_packet, {{1u, "HistogramSample"}});
  ASSERT_TRUE(e_packet->track_event().has_chrome_histogram_sample());
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().name_hash(),
            base::HashMetricName("Foo1.Bar1"));
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().name(),
            "Foo1.Bar1");
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().sample(), 1u);

  e_packet = producer_client()->GetFinalizedPacket(packet_index++);

  ExpectEventCategories(e_packet, {});
  ExpectEventNames(e_packet, {});
  ASSERT_TRUE(e_packet->track_event().has_chrome_histogram_sample());
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().name_hash(),
            base::HashMetricName("Foo3.Bar3"));
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().name(),
            "Foo3.Bar3");
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().sample(), 1u);

  EXPECT_EQ(packet_index, producer_client()->GetFinalizedPacketCount());
}

TEST_F(TraceEventDataSourceTest, UserActionEvent) {
  base::SetRecordActionTaskRunner(base::ThreadTaskRunnerHandle::Get());

  base::trace_event::TraceConfig trace_config(
      "-*,disabled-by-default-user_action_samples",
      base::trace_event::RECORD_UNTIL_FULL);

  CreateTraceEventDataSource(/*privacy_filtering_enabled=*/false,
                             /*start_trace=*/true, trace_config.ToString());

  // Wait for registering callback on current thread.
  base::RunLoop().RunUntilIdle();

  base::RecordAction(base::UserMetricsAction("Test_Action"));

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);

  ExpectEventCategories(
      e_packet, {{1u, TRACE_DISABLED_BY_DEFAULT("user_action_samples")}});
  ExpectEventNames(e_packet, {{1u, "UserAction"}});
  ASSERT_TRUE(e_packet->track_event().has_chrome_user_event());
  EXPECT_EQ(e_packet->track_event().chrome_user_event().action_hash(),
            base::HashMetricName("Test_Action"));
}

namespace {

struct InternedLogMessageBody
    : public perfetto::TrackEventInternedDataIndex<
          InternedLogMessageBody,
          perfetto::protos::pbzero::InternedData::kLogMessageBodyFieldNumber,
          std::string> {
  static void Add(perfetto::protos::pbzero::InternedData* interned_data,
                  size_t iid,
                  const std::string& body) {
    auto* msg = interned_data->add_log_message_body();
    msg->set_iid(iid);
    msg->set_body(body);
  }
};

}  // namespace

TEST_F(TraceEventDataSourceTest, TypedEventInterning) {
  CreateTraceEventDataSource();

  {
    TRACE_EVENT("browser", "bar", [&](perfetto::EventContext ctx) {
      size_t iid = InternedLogMessageBody::Get(&ctx, "Hello interned world!");
      ctx.event()->set_log_message()->set_body_iid(iid);
    });
  }
  size_t packet_index = ExpectStandardPreamble();
  auto* e_packet = producer_client()->GetFinalizedPacket(packet_index++);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  ASSERT_TRUE(e_packet->has_interned_data());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(),
            e_packet->interned_data().log_message_body()[0].iid());
  ASSERT_EQ("Hello interned world!",
            e_packet->interned_data().log_message_body()[0].body());
}

// TODO(eseckler): Add startup tracing unittests.

}  // namespace

}  // namespace tracing
