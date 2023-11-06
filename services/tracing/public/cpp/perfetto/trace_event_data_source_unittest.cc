// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/user_metrics.h"
#include "base/process/current_process.h"
#include "base/process/current_process_test.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/common/task_annotator.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "base/tracing/trace_time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/perfetto/test_utils.h"
#include "services/tracing/public/cpp/perfetto/custom_event_recorder.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/producer_test_utils.h"
#include "services/tracing/public/cpp/perfetto/track_name_recorder.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_interned_data_index.h"
#include "third_party/perfetto/protos/perfetto/trace/clock_snapshot.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_process_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_thread_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/counter_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.pb.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

using TrackEvent = perfetto::protos::TrackEvent;

namespace tracing {

namespace {

constexpr char kTestProcess[] = "Browser";
base::CurrentProcessType kTestProcessType =
    base::CurrentProcessType::PROCESS_BROWSER;
constexpr char kTestThread[] = "CrTestMain";
constexpr const char kCategoryGroup[] = "browser";

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
std::unique_ptr<perfetto::TracingSession> g_tracing_session;
constexpr const char kPrivacyFiltered[] = "";
#else
constexpr uint32_t kClockIdAbsolute = 64;
constexpr uint32_t kClockIdIncremental = 65;
constexpr const char kPrivacyFiltered[] = "PRIVACY_FILTERED";
#endif

class TraceEventDataSourceTest
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    : public testing::Test
#else
    : public TracingUnitTest
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
{
 public:
  using PacketVector =
      std::vector<std::unique_ptr<perfetto::protos::TracePacket>>;
  void SetUp() override {
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    TracingUnitTest::SetUp();
    TraceEventDataSource::GetInstance()->RegisterStartupHooks();
    // TODO(eseckler): Initialize the entire perfetto client library instead.
    perfetto::internal::TrackRegistry::InitializeInstance();
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

    old_thread_name_ = base::PlatformThread::GetName();
    base::PlatformThread::SetName(kTestThread);
    old_process_name_ = base::test::CurrentProcessForTest::GetName();
    old_process_type_ = base::test::CurrentProcessForTest::GetType();
    base::CurrentProcess::GetInstance().SetProcessNameAndType(kTestProcess,
                                                              kTestProcessType);
    base::trace_event::TraceLog::GetInstance()->SetRecordHostAppPackageName(
        false);

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    PerfettoTracedProcess::GetTaskRunner()->ResetTaskRunnerForTesting(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    TrackNameRecorder::GetInstance();
    CustomEventRecorder::GetInstance();  //->ResetForTesting();
    TraceEventMetadataSource::GetInstance()->ResetForTesting();
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner();
    auto perfetto_wrapper = std::make_unique<base::tracing::PerfettoTaskRunner>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    producer_client_ =
        std::make_unique<TestProducerClient>(std::move(perfetto_wrapper));
    TraceEventMetadataSource::GetInstance()->ResetForTesting();
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  }

  void TearDown() override {
    // Reset the callback - it's stored in CustomEventRecorder, and otherwise
    // may stay for the next test(s).
    CustomEventRecorder::GetInstance()->SetActiveProcessesCallback({});
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    if (base::trace_event::TraceLog::GetInstance()->IsEnabled()) {
      base::RunLoop wait_for_tracelog_flush;
      TraceEventDataSource::GetInstance()->StopTracing(
          wait_for_tracelog_flush.QuitClosure());
      wait_for_tracelog_flush.Run();
    }

    // As MockTraceWriter keeps a pointer to our TestProducerClient,
    // we need to make sure to clean it up from TLS. The other sequences
    // get DummyTraceWriters that we don't care about.
    TraceEventDataSource::GetInstance()->FlushCurrentThread();
    producer_client_.reset();
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

    base::PlatformThread::SetName(kTestThread);
    base::CurrentProcess::GetInstance().SetProcessNameAndType(
        old_process_name_, old_process_type_);

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    TracingUnitTest::TearDown();
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

    // Destroy after task environment shuts down so that no other threads try to
    // add trace events.
    TraceEventDataSource::ResetForTesting();
  }

  void StartTraceEventDataSource(
      bool privacy_filtering_enabled = false,
      std::string chrome_trace_config = "foo,cat1,cat2,cat3,browser,-*",
      std::vector<std::string> histograms = {}) {
    base::trace_event::TraceConfig base_config(chrome_trace_config, "");
    for (const auto& histogram : histograms)
      base_config.EnableHistogram(histogram);
    chrome_trace_config = base_config.ToString();

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    perfetto::TraceConfig trace_config(
        tracing::GetPerfettoConfigWithDataSources(
            base_config,
            {{ tracing::mojom::kTraceEventDataSourceName }},
            privacy_filtering_enabled));
    g_tracing_session = perfetto::Tracing::NewTrace();
    g_tracing_session->Setup(trace_config);
    g_tracing_session->StartBlocking();
    // Make sure TraceEventDataSource::StartTracingImpl gets run.
    base::RunLoop().RunUntilIdle();
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    perfetto::DataSourceConfig config;
    config.mutable_chrome_config()->set_privacy_filtering_enabled(
        privacy_filtering_enabled);
    config.mutable_chrome_config()->set_trace_config(chrome_trace_config);
    TraceEventDataSource::GetInstance()->StartTracingImpl(producer_client(),
                                                          config);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  }

  void StartMetaDataSource(bool privacy_filtering_enabled = false) {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    perfetto::TraceConfig trace_config(
        tracing::GetPerfettoConfigWithDataSources(
            base::trace_event::TraceConfig(),
            {{ tracing::mojom::kMetaDataSourceName }},
            privacy_filtering_enabled));
    auto* data_source = &(*trace_config.mutable_data_sources())[0];
    data_source->mutable_config()->mutable_chrome_config()->set_trace_config(
        "");
    g_tracing_session = perfetto::Tracing::NewTrace();
    g_tracing_session->Setup(trace_config);
    g_tracing_session->StartBlocking();
    // Make sure TraceEventMetadataSource::StartTracingImpl gets run.
    base::RunLoop().RunUntilIdle();
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    perfetto::DataSourceConfig source_config;
    source_config.mutable_chrome_config()->set_privacy_filtering_enabled(
        privacy_filtering_enabled);
    TraceEventMetadataSource::GetInstance()->StartTracingImpl(producer_client(),
                                                              source_config);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  }

  void StopMetaDataSource() {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    EnsureTraceStopped();
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    base::RunLoop wait_for_stop;
    TraceEventMetadataSource::GetInstance()->StopTracing(
        wait_for_stop.QuitClosure());
    wait_for_stop.Run();
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  }

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  void EnsureTraceStopped() {
    if (!g_tracing_session)
      return;
    StopAndParseTrace();
  }

  void StopAndParseTrace() {
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
      // Filter out packets from the tracing service.
      if (packet.trusted_packet_sequence_id() == 1)
        continue;
      if (packet.has_chrome_metadata()) {
        metadata_packets_.push_back(packet.chrome_metadata());
      }
      if (packet.has_chrome_events() &&
          packet.chrome_events().metadata_size() > 0) {
        legacy_metadata_packets_.push_back(packet.chrome_events());
      }
      auto proto = std::make_unique<perfetto::protos::TracePacket>();
      *proto = packet;

      finalized_packets_.push_back(std::move(proto));
    }
  }
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  TestProducerClient* producer_client() { return producer_client_.get(); }

  const PacketVector& GetFinalizedPackets() {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    EnsureTraceStopped();
    return finalized_packets_;
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    return producer_client()->finalized_packets();
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  }

  const perfetto::protos::TracePacket* GetFinalizedPacket(size_t packet_index) {
    auto& packets = GetFinalizedPackets();
    CHECK(packet_index < packets.size());
    return packets.at(packet_index).get();
  }

  size_t GetFinalizedPacketCount() { return GetFinalizedPackets().size(); }

  const perfetto::protos::ChromeMetadataPacket* GetProtoChromeMetadata(
      size_t packet_index = 0) {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    EnsureTraceStopped();
    EXPECT_GT(metadata_packets_.size(), packet_index);
    return &metadata_packets_[packet_index];
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    return producer_client()->GetProtoChromeMetadata(packet_index);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  }

  const google::protobuf::RepeatedPtrField<perfetto::protos::ChromeMetadata>&
  GetChromeMetadata(size_t packet_index = 0) {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    EnsureTraceStopped();
    EXPECT_GT(legacy_metadata_packets_.size(), packet_index);
    return legacy_metadata_packets_[packet_index].metadata();
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    return *producer_client()->GetChromeMetadata(packet_index);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  }

  void ExpectClockSnapshotAndDefaults(
      const perfetto::protos::TracePacket* packet,
      uint64_t min_timestamp = 1u) {
    // In Perfetto mode, the tracing service emits clock snapshots.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    // ClockSnapshot for absolute & incremental microsecond clocks.
    ASSERT_TRUE(packet->has_clock_snapshot());
    ASSERT_EQ(packet->clock_snapshot().clocks().size(), 3);

    EXPECT_EQ(packet->clock_snapshot().clocks()[0].clock_id(),
              static_cast<uint32_t>(base::tracing::kTraceClockId));
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
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

    // Default to incremental clock and thread track.
    ASSERT_TRUE(packet->has_trace_packet_defaults());
    // TODO(skyostil): Use incremental timestamps with Perfetto.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    EXPECT_EQ(packet->trace_packet_defaults().timestamp_clock_id(),
              kClockIdIncremental);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
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
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
      EXPECT_TRUE(packet->track_descriptor().thread().has_thread_name());
#else
      EXPECT_FALSE(packet->track_descriptor().thread().has_thread_name());
#endif
    } else {
      EXPECT_EQ(packet->track_descriptor().thread().thread_name(), kTestThread);
    }

    if (!thread_id) {
      EXPECT_EQ(packet->track_descriptor().uuid(), default_track_uuid_);
      if (process_track_uuid_) {
        EXPECT_EQ(packet->track_descriptor().parent_uuid(),
                  process_track_uuid_);
      }

      // TODO(skyostil): Record the Chrome thread descriptor.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
      ASSERT_TRUE(packet->track_descriptor().has_chrome_thread());
      EXPECT_EQ(perfetto::protos::ChromeThreadDescriptor::THREAD_MAIN,
                packet->track_descriptor().chrome_thread().thread_type());
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
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

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    EXPECT_EQ(packet->sequence_flags(), 0u);
#endif
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
    // TODO(skyostil): Write Chrome process descriptors.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    ASSERT_TRUE(packet->track_descriptor().has_chrome_process());
#endif

    EXPECT_NE(packet->track_descriptor().uuid(), 0u);
    EXPECT_FALSE(packet->track_descriptor().has_parent_uuid());

    process_track_uuid_ = packet->track_descriptor().uuid();

    EXPECT_NE(packet->track_descriptor().process().pid(), 0);

    if (filtering_enabled) {
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
      EXPECT_FALSE(packet->track_descriptor().process().has_process_name());
#endif
    } else {
      EXPECT_EQ(packet->track_descriptor().process().process_name(),
                kTestProcess);
    }

    // TODO(skyostil): Record the Chrome process descriptor.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    ASSERT_TRUE(packet->track_descriptor().has_chrome_process());
    EXPECT_EQ(packet->track_descriptor().chrome_process().process_type(),
              perfetto::protos::ChromeProcessDescriptor::PROCESS_BROWSER);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    // ProcessDescriptors is only emitted when incremental state was reset, and
    // thus also always serves as indicator for the state reset to the consumer
    // (for the TraceEventDataSource's TraceWriter sequence).
    EXPECT_EQ(packet->sequence_flags(),
              static_cast<uint32_t>(perfetto::protos::pbzero::TracePacket::
                                        SEQ_INCREMENTAL_STATE_CLEARED));
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  }

  void ExpectChildTrack(const perfetto::protos::TracePacket* packet,
                        uint64_t uuid,
                        uint64_t parent_uuid) {
    EXPECT_EQ(packet->track_descriptor().uuid(), uuid);
    EXPECT_EQ(packet->track_descriptor().parent_uuid(), parent_uuid);
  }

  size_t ExpectStandardPreamble(size_t packet_index = 0,
                                bool privacy_filtering_enabled = false) {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    // With the client library, the first packet on a sequence is a metadata
    // packet, with first_packet_on_sequence = true. It may also contain the
    // clock information, or it could be a separate packet with no other
    // payload.
    // base::RunLoop().RunUntilIdle() in StartTraceEventDataSource() emits an
    // empty packet first, but only if no other events are emitted by the
    // runloop beforehand -- which would be the case if "toplevel" is enabled.
    const perfetto::protos::TracePacket* clock_packet = nullptr;
    if (packet_index == 0) {
      auto* first_metadata_packet = GetFinalizedPacket(packet_index++);
      EXPECT_TRUE(first_metadata_packet->first_packet_on_sequence());
      if (first_metadata_packet->has_clock_snapshot()) {
        clock_packet = first_metadata_packet;
      } else {
        clock_packet = GetFinalizedPacket(packet_index++);
      }
    } else {
      clock_packet = GetFinalizedPacket(packet_index++);
    }

    auto* tt_packet = GetFinalizedPacket(packet_index++);
    auto* pt_packet = GetFinalizedPacket(packet_index++);
#else
    auto* pt_packet = GetFinalizedPacket(packet_index++);
    auto* clock_packet = GetFinalizedPacket(packet_index++);
    auto* tt_packet = GetFinalizedPacket(packet_index++);
#endif

    ExpectProcessTrack(pt_packet, privacy_filtering_enabled);
    ExpectClockSnapshotAndDefaults(clock_packet);
    ExpectThreadTrack(tt_packet, /*thread_id=*/0, /*min_timestamp=*/1u,
                      privacy_filtering_enabled);

    if (base::ThreadTicks::IsSupported()) {
      auto* ttt_packet = GetFinalizedPacket(packet_index++);
      ExpectThreadTimeCounterTrack(ttt_packet);
    }

    return packet_index;
  }

  struct StringOrIid {
    // implicit
    StringOrIid(uint32_t iid) : iid(iid) {}
    // implicit
    StringOrIid(std::string value) : value(value) {}
    // implicit
    StringOrIid(const char* value) : value(value) {}

    std::string value;
    uint32_t iid = 0;
  };

  void ExpectTraceEvent(const perfetto::protos::TracePacket* packet,
                        uint32_t category_iid,
                        StringOrIid name,
                        char phase,
                        uint32_t flags = 0,
                        uint64_t id = 0,
                        uint64_t absolute_timestamp = 0,
                        int32_t tid_override = 0,
                        const perfetto::Track& track = perfetto::Track(),
                        int64_t explicit_thread_time = 0,
                        base::Location from_here = base::Location::Current()) {
    SCOPED_TRACE(from_here.ToString());
    // All TrackEvents need incremental state for delta timestamps / interning.
    EXPECT_EQ(packet->sequence_flags(),
              static_cast<uint32_t>(perfetto::protos::pbzero::TracePacket::
                                        SEQ_NEEDS_INCREMENTAL_STATE));

    EXPECT_TRUE(packet->has_track_event());

    if (absolute_timestamp > 0) {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
      // TODO(eseckler): Support microsecond encoding.
      EXPECT_EQ(packet->timestamp(), absolute_timestamp * 1000);
#else
      EXPECT_EQ(packet->timestamp_clock_id(), kClockIdAbsolute);
      EXPECT_EQ(packet->timestamp(), absolute_timestamp);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    } else {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
      if (packet->has_clock_snapshot()) {
        for (auto& clock : packet->clock_snapshot().clocks()) {
          if (clock.is_incremental()) {
            EXPECT_LE(last_timestamp_, clock.timestamp());
            last_timestamp_ = clock.timestamp();
          }
        }
      } else if (!packet->has_timestamp_clock_id()) {
        // Packets that don't have a timestamp_clock_id default to the
        // incremental clock.
        last_timestamp_ += packet->timestamp();
      }
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
      // Default to kClockIdIncremental.
      EXPECT_FALSE(packet->has_timestamp_clock_id());
      EXPECT_TRUE(packet->has_timestamp());
      EXPECT_GE(packet->timestamp(), 0u);
      EXPECT_LE(last_timestamp_ + packet->timestamp(),
                static_cast<uint64_t>(
                    TRACE_TIME_TICKS_NOW().since_origin().InMicroseconds()));
      last_timestamp_ += packet->timestamp();
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
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
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
      if (packet->track_event().category_iids_size()) {
        ASSERT_EQ(packet->track_event().category_iids_size(), 1);
        EXPECT_EQ(packet->track_event().category_iids(0), category_iid);
      } else {
        // Perfetto doesn't use interning for categories that are looked up
        // through TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED, but since only
        // one category is used in this test suite through this mechanism, we
        // can hardcode the expectation here.
        EXPECT_EQ(packet->track_event().categories(0), kCategoryGroup);
      }
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
      ASSERT_EQ(packet->track_event().category_iids_size(), 1);
      EXPECT_EQ(packet->track_event().category_iids(0), category_iid);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    } else {
      EXPECT_EQ(packet->track_event().category_iids_size(), 0);
    }

    if (name.iid > 0) {
      EXPECT_EQ(name.iid, packet->track_event().name_iid());
    } else if (!name.value.empty()) {
      EXPECT_EQ(name.value, packet->track_event().name());
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
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
      // Perfetto uses tid/pid overrides only for non-local processes.
      EXPECT_FALSE(legacy_event.has_tid_override());
      EXPECT_EQ(packet->track_event().track_uuid(), 0u);
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
      EXPECT_EQ(legacy_event.tid_override(), tid_override);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    }
  }

  void ExpectEventCategories(
      const perfetto::protos::TracePacket* packet,
      std::initializer_list<std::pair<uint32_t, std::string>> entries,
      base::Location from_here = base::Location::Current()) {
    // Perfetto doesn't use interning for test-only categories.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    ExpectInternedNames(packet->interned_data().event_categories(), entries,
                        from_here);
#endif
  }

  void ExpectInternedEventNames(
      const perfetto::protos::TracePacket* packet,
      std::initializer_list<std::pair<uint32_t, std::string>> entries,
      base::Location from_here = base::Location::Current()) {
    ExpectInternedNames(packet->interned_data().event_names(), entries,
                        from_here);
  }

  void ExpectInternedDebugAnnotationNames(
      const perfetto::protos::TracePacket* packet,
      std::initializer_list<std::pair<uint32_t, std::string>> entries,
      base::Location from_here = base::Location::Current()) {
    ExpectInternedNames(packet->interned_data().debug_annotation_names(),
                        entries, from_here);
  }

  template <typename T>
  void ExpectInternedNames(
      const google::protobuf::RepeatedPtrField<T>& field,
      std::initializer_list<std::pair<uint32_t, std::string>> expected_entries,
      base::Location from_here = base::Location::Current()) {
    SCOPED_TRACE(from_here.ToString());
    std::vector<std::pair<uint32_t, std::string>> entries;
    for (int i = 0; i < field.size(); ++i) {
      entries.emplace_back(field[i].iid(), field[i].name());
    }
    EXPECT_THAT(entries, testing::ElementsAreArray(expected_entries));
  }

  std::set<base::ProcessId> ActiveProcessesCallback() const {
    return {1, 2, 10};
  }

 protected:
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  base::test::TaskEnvironment task_environment_;
  base::test::TracingEnvironment tracing_environment_;
  PacketVector finalized_packets_;
  std::vector<perfetto::protos::ChromeMetadataPacket> metadata_packets_;
  std::vector<perfetto::protos::ChromeEventBundle> legacy_metadata_packets_;
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  std::unique_ptr<TestProducerClient> producer_client_;
  uint64_t last_timestamp_ = 0;
  int64_t last_thread_time_ = 0;
  uint64_t default_track_uuid_ = 0u;
  uint64_t process_track_uuid_ = 0u;
  std::vector<uint64_t> default_extra_counter_track_uuids_;

  std::string old_thread_name_;
  std::string old_process_name_;
  base::CurrentProcessType old_process_type_;
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
                      const base::Value::Dict& value) {
  EXPECT_TRUE(entry.has_json_value());

  absl::optional<base::Value::Dict> child_dict =
      base::JSONReader::ReadDict(entry.json_value());
  EXPECT_EQ(*child_dict, value);
}

template <typename T>
void MetadataHasNamedValue(const google::protobuf::RepeatedPtrField<
                               perfetto::protos::ChromeMetadata>& metadata,
                           const char* name,
                           const T& value) {
  for (const auto& entry : metadata) {
    if (entry.name() == name) {
      HasMetadataValue(entry, value);
      return;
    }
  }

  NOTREACHED();
}

absl::optional<base::Value::Dict> AddJsonMetadataGenerator() {
  base::Value::Dict metadata;
  metadata.Set("foo_int", 42);
  metadata.Set("foo_str", "bar");
  metadata.Set("foo_bool", true);

  base::Value::Dict child_dict;
  child_dict.Set("child_str", "child_val");
  metadata.Set("child_dict", std::move(child_dict));
  return metadata;
}

TEST_F(TraceEventDataSourceTest, MetadataGeneratorBeforeTracing) {
  auto* metadata_source = TraceEventMetadataSource::GetInstance();
  metadata_source->AddGeneratorFunction(
      base::BindRepeating(&AddJsonMetadataGenerator));

  StartMetaDataSource();
  StopMetaDataSource();

  auto& metadata = GetChromeMetadata();
  EXPECT_EQ(4, metadata.size());
  MetadataHasNamedValue(metadata, "foo_int", 42);
  MetadataHasNamedValue(metadata, "foo_str", "bar");
  MetadataHasNamedValue(metadata, "foo_bool", true);

  auto child_dict = base::Value::Dict().Set("child_str", "child_val");
  MetadataHasNamedValue(metadata, "child_dict", child_dict);
}

TEST_F(TraceEventDataSourceTest, MetadataGeneratorWhileTracing) {
  auto* metadata_source = TraceEventMetadataSource::GetInstance();

  StartMetaDataSource();
  metadata_source->AddGeneratorFunction(
      base::BindRepeating(&AddJsonMetadataGenerator));
  StopMetaDataSource();

  auto& metadata = GetChromeMetadata();
  EXPECT_EQ(4, metadata.size());
  MetadataHasNamedValue(metadata, "foo_int", 42);
  MetadataHasNamedValue(metadata, "foo_str", "bar");
  MetadataHasNamedValue(metadata, "foo_bool", true);

  auto child_dict = base::Value::Dict().Set("child_str", "child_val");
  MetadataHasNamedValue(metadata, "child_dict", child_dict);
}

TEST_F(TraceEventDataSourceTest, MultipleMetadataGenerators) {
  auto* metadata_source = TraceEventMetadataSource::GetInstance();
  metadata_source->AddGeneratorFunction(base::BindRepeating([]() {
    base::Value::Dict metadata;
    metadata.Set("before_int", 42);
    return absl::optional<base::Value::Dict>(std::move(metadata));
  }));

  StartMetaDataSource();
  metadata_source->AddGeneratorFunction(
      base::BindRepeating(&AddJsonMetadataGenerator));
  StopMetaDataSource();

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  auto& metadata = GetChromeMetadata(1);
  auto& metadata1 = GetChromeMetadata(0);
#else
  auto& metadata = GetChromeMetadata(0);
  auto& metadata1 = GetChromeMetadata(1);
#endif
  EXPECT_EQ(4, metadata.size());
  MetadataHasNamedValue(metadata, "foo_int", 42);
  MetadataHasNamedValue(metadata, "foo_str", "bar");
  MetadataHasNamedValue(metadata, "foo_bool", true);

  auto child_dict = base::Value::Dict().Set("child_str", "child_val");
  MetadataHasNamedValue(metadata, "child_dict", child_dict);

  EXPECT_EQ(1, metadata1.size());
  MetadataHasNamedValue(metadata1, "before_int", 42);
}

// With Perfetto client library, it's not possible to filter package names
// based on privacy settings, because privacy settings are per-session while
// track descriptors are global. But this is not a problem because the filtering
// is done at upload stage, so package names don't leak into Chrome traces.
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
TEST_F(TraceEventDataSourceTest,
       PackageNameNotRecordedPrivacyFilteringDisabledTraceLogNotSet) {
  StartTraceEventDataSource(/* privacy_filtering_enabled = false */);

  auto* e_packet = producer_client()->GetFinalizedPacket(0);
  ExpectProcessTrack(e_packet /* privacy_filtering_enabled = false */);
  ASSERT_FALSE(e_packet->track_descriptor()
                   .chrome_process()
                   .has_host_app_package_name());
}

TEST_F(TraceEventDataSourceTest,
       PackageNameNotRecordedPrivacyFilteringEnabledTraceLogNotSet) {
  StartTraceEventDataSource(true /* privacy_filtering_enabled */);

  auto* e_packet = producer_client()->GetFinalizedPacket(0);
  ExpectProcessTrack(e_packet, true /* privacy_filtering_enabled */);
  ASSERT_FALSE(e_packet->track_descriptor()
                   .chrome_process()
                   .has_host_app_package_name());
}

TEST_F(TraceEventDataSourceTest,
       PackageNameNotRecordedPrivacyFilteringEnabledTraceLogSet) {
  base::trace_event::TraceLog::GetInstance()->SetRecordHostAppPackageName(true);
  StartTraceEventDataSource(true /* privacy_filtering_enabled */);

  auto* e_packet = producer_client()->GetFinalizedPacket(0);
  ExpectProcessTrack(e_packet, true /* privacy_filtering_enabled */);
  ASSERT_FALSE(e_packet->track_descriptor()
                   .chrome_process()
                   .has_host_app_package_name());
}

TEST_F(TraceEventDataSourceTest,
       PackageNameRecordedPrivacyFilteringDisabledTraceLogSet) {
  base::trace_event::TraceLog::GetInstance()->SetRecordHostAppPackageName(true);
  StartTraceEventDataSource(/* privacy_filtering_enabled = false */);

  auto* e_packet = producer_client()->GetFinalizedPacket(0);
  ExpectProcessTrack(e_packet /* privacy_filtering_enabled = false */);
  ASSERT_TRUE(e_packet->track_descriptor()
                  .chrome_process()
                  .has_host_app_package_name());
  EXPECT_EQ(
      base::android::BuildInfo::GetInstance()->host_package_name(),
      e_packet->track_descriptor().chrome_process().host_app_package_name());
}
#endif  // BUILDFLAG(IS_ANDROID) && !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

TEST_F(TraceEventDataSourceTest, BasicTraceEvent) {
  StartTraceEventDataSource();

  TRACE_EVENT_BEGIN0(kCategoryGroup, "bar");

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});

// producer_client() is null under USE_PERFETTO_CLIENT_LIBRARY.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // The data source emits one empty packet after the ProcessDescriptor.
  EXPECT_EQ(producer_client()->empty_finalized_packets_count(), 1);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
}

TEST_F(TraceEventDataSourceTest, ActiveProcessesMetadata) {
  CustomEventRecorder::GetInstance()->SetActiveProcessesCallback(
      base::BindRepeating(&TraceEventDataSourceTest::ActiveProcessesCallback,
                          base::Unretained(this)));
  StartTraceEventDataSource();

  TRACE_EVENT_BEGIN0(kCategoryGroup, "bar");

  size_t packet_index = ExpectStandardPreamble();

  auto* active_processes_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(active_processes_packet, /*category_iid=*/1u,
                   /*name=*/1u, TRACE_EVENT_PHASE_INSTANT);
  ExpectEventCategories(active_processes_packet, {{1u, "__metadata"}});
  ExpectInternedEventNames(active_processes_packet, {{1u, "ActiveProcesses"}});
}

// For some reason this is failing in `cast_chrome`.
// Disabling it now to unblock perfetto-chrome autoroll and enabling it
// again in next CL after RCAing.
TEST_F(TraceEventDataSourceTest, DISABLED_TimestampedTraceEvent) {
  StartTraceEventDataSource();

  base::PlatformThreadId current_thread_tid =
      perfetto::ThreadTrack::Current().tid;

  TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0(
      kCategoryGroup, "bar", 42, current_thread_tid,
      base::TimeTicks() + base::Microseconds(424242));

  size_t packet_index = ExpectStandardPreamble();

  // Thread track for the overridden tid.
  auto* tt_packet = GetFinalizedPacket(packet_index++);
  ExpectThreadTrack(tt_packet,
                    /*thread_id=*/perfetto::ThreadTrack::Current().tid);

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(
      e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
      TRACE_EVENT_PHASE_ASYNC_BEGIN,
      TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP | TRACE_EVENT_FLAG_HAS_ID, /*id=*/42u,
      /*absolute_timestamp=*/424242, /*tid_override=*/current_thread_tid,
      perfetto::ThreadTrack::Current());

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, InstantTraceEvent) {
  StartTraceEventDataSource();

  TRACE_EVENT_INSTANT0(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, InstantTraceEventOnOtherThread) {
  StartTraceEventDataSource();

  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(
      TRACE_EVENT_PHASE_INSTANT, kCategoryGroup, "bar",
      trace_event_internal::kNoId, base::PlatformThreadId(1),
      base::TimeTicks() + base::Microseconds(10),
      /*flags=*/TRACE_EVENT_SCOPE_THREAD);
  size_t packet_index = ExpectStandardPreamble();

  // Thread track for the overridden tid.
  auto* tt_packet = GetFinalizedPacket(packet_index++);
  ExpectThreadTrack(tt_packet, /*thread_id=*/1);

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT,
                   TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP |
                       TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_HAS_ID,
                   /*id=*/0u,
                   /*absolute_timestamp=*/10, /*tid_override=*/1,
                   perfetto::ThreadTrack::ForThread(1));

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, EventWithStringArgs) {
  StartTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD,
                       "arg1_name", "arg1_val", "arg2_name", "arg2_val");

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].name_iid(), 1u);
  EXPECT_EQ(annotations[0].string_value(), "arg1_val");
  EXPECT_EQ(annotations[1].name_iid(), 2u);
  EXPECT_EQ(annotations[1].string_value(), "arg2_val");

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});
  ExpectInternedDebugAnnotationNames(e_packet,
                                     {{1u, "arg1_name"}, {2u, "arg2_name"}});
}

TEST_F(TraceEventDataSourceTest, EventWithCopiedStrings) {
  StartTraceEventDataSource();

  TRACE_EVENT_COPY_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD,
                            "arg1_name", "arg1_val", "arg2_name", "arg2_val");

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);

  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name=*/"bar",
                   TRACE_EVENT_PHASE_INSTANT,
                   TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_COPY);
  EXPECT_EQ(e_packet->track_event().name(), "bar");
  EXPECT_EQ(annotations[0].name(), "arg1_name");
  EXPECT_EQ(annotations[1].name(), "arg2_name");

  EXPECT_EQ(annotations[0].string_value(), "arg1_val");
  EXPECT_EQ(annotations[1].string_value(), "arg2_val");

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
}

TEST_F(TraceEventDataSourceTest, EventWithUIntArgs) {
  StartTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42u, "bar", 4242u);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].uint_value(), 42u);
  EXPECT_EQ(annotations[1].uint_value(), 4242u);
}

TEST_F(TraceEventDataSourceTest, EventWithIntArgs) {
  StartTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42, "bar", 4242);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].int_value(), 42);
  EXPECT_EQ(annotations[1].int_value(), 4242);
}

TEST_F(TraceEventDataSourceTest, EventWithBoolArgs) {
  StartTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       true, "bar", false);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_TRUE(annotations[0].has_bool_value());
  EXPECT_EQ(annotations[0].bool_value(), true);
  EXPECT_TRUE(annotations[1].has_bool_value());
  EXPECT_EQ(annotations[1].bool_value(), false);
}

TEST_F(TraceEventDataSourceTest, EventWithDoubleArgs) {
  StartTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42.42, "bar", 4242.42);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].double_value(), 42.42);
  EXPECT_EQ(annotations[1].double_value(), 4242.42);
}

TEST_F(TraceEventDataSourceTest, EventWithPointerArgs) {
  StartTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       reinterpret_cast<void*>(0xBEEF), "bar",
                       reinterpret_cast<void*>(0xF00D));

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].pointer_value(), static_cast<uintptr_t>(0xBEEF));
  EXPECT_EQ(annotations[1].pointer_value(), static_cast<uintptr_t>(0xF00D));
}

TEST_F(TraceEventDataSourceTest, EventWithConvertableArgs) {
  StartTraceEventDataSource();

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
    raw_ptr<int> num_calls_;
    const char* arg_value_;
  };

  std::unique_ptr<Convertable> conv1(new Convertable(&num_calls, kArgValue1));
  std::unique_ptr<Convertable> conv2(new Convertable(&num_calls, kArgValue2));

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD,
                       "foo_arg1", std::move(conv1), "foo_arg2",
                       std::move(conv2));

  EXPECT_EQ(2, num_calls);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].legacy_json_value(), kArgValue1);
  EXPECT_EQ(annotations[1].legacy_json_value(), kArgValue2);
}

TEST_F(TraceEventDataSourceTest, NestableAsyncTraceEvent) {
  constexpr bool kPrivacyFilteringEnabled = true;
  StartTraceEventDataSource(kPrivacyFilteringEnabled);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kCategoryGroup, "foo",
                                    TRACE_ID_WITH_SCOPE("foo", 1));
  // "foo" is the first name string interned.
  constexpr uint32_t kFooNameIID = 1u;

  // Same id, different scope.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kCategoryGroup, "bar",
                                    TRACE_ID_WITH_SCOPE("bar", 1));
  // "bar" is the first name string interned.
  constexpr uint32_t kBarNameIID = 2u;

  // Same scope, different id.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kCategoryGroup, "bar",
                                    TRACE_ID_WITH_SCOPE("bar", 2));

  TRACE_EVENT_NESTABLE_ASYNC_END0(kCategoryGroup, "bar",
                                  TRACE_ID_WITH_SCOPE("bar", 2));
  TRACE_EVENT_NESTABLE_ASYNC_END0(kCategoryGroup, "bar",
                                  TRACE_ID_WITH_SCOPE("bar", 1));
  TRACE_EVENT_NESTABLE_ASYNC_END0(kCategoryGroup, "foo",
                                  TRACE_ID_WITH_SCOPE("foo", 1));

  size_t packet_index = ExpectStandardPreamble(0, kPrivacyFilteringEnabled);

  // Helper function that puts the unscoped_id of the packet's legacy_event in
  // `id`. This uses an output parameter instead of a return value so that it
  // can use ASSERT macros for early return.
  auto get_legacy_event_id =
      [](const perfetto::protos::TracePacket* packet, uint64_t* id,
         base::Location from_here = base::Location::Current()) {
        SCOPED_TRACE(from_here.ToString());
        // Output 0 on error.
        *id = 0;
        ASSERT_TRUE(packet->has_track_event());
        ASSERT_TRUE(packet->track_event().has_legacy_event());
        ASSERT_TRUE(packet->track_event().legacy_event().has_unscoped_id());
        *id = packet->track_event().legacy_event().unscoped_id();
      };

  // kCategoryGroup is the first (and only) category string interned.
  constexpr uint32_t kCategoryIID = 1u;

  // Since privacy filtering is enabled, the event id's can be altered to avoid
  // conflicts when scope names are filtered out. The important thing is that
  // each begin event has a different id, because of the different scopes, and
  // each end event's id matches the corresponding begin event.
  uint64_t id1;
  auto* e_packet = GetFinalizedPacket(packet_index++);
  get_legacy_event_id(e_packet, &id1);
  ExpectTraceEvent(e_packet, kCategoryIID, kFooNameIID,
                   TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN,
                   TRACE_EVENT_FLAG_HAS_ID, id1);

  uint64_t id2;
  e_packet = GetFinalizedPacket(packet_index++);
  get_legacy_event_id(e_packet, &id2);
  EXPECT_NE(id2, id1);
  ExpectTraceEvent(e_packet, kCategoryIID, kBarNameIID,
                   TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN,
                   TRACE_EVENT_FLAG_HAS_ID, id2);

  uint64_t id3;
  e_packet = GetFinalizedPacket(packet_index++);
  get_legacy_event_id(e_packet, &id3);
  EXPECT_NE(id3, id1);
  EXPECT_NE(id3, id2);
  ExpectTraceEvent(e_packet, kCategoryIID, kBarNameIID,
                   TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN,
                   TRACE_EVENT_FLAG_HAS_ID, id3);

  // End events don't include the names.
  constexpr uint32_t kMissingNameIID = 0u;
  e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, kCategoryIID, kMissingNameIID,
                   TRACE_EVENT_PHASE_NESTABLE_ASYNC_END,
                   TRACE_EVENT_FLAG_HAS_ID, id3);
  e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, kCategoryIID, kMissingNameIID,
                   TRACE_EVENT_PHASE_NESTABLE_ASYNC_END,
                   TRACE_EVENT_FLAG_HAS_ID, id2);
  e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, kCategoryIID, kMissingNameIID,
                   TRACE_EVENT_PHASE_NESTABLE_ASYNC_END,
                   TRACE_EVENT_FLAG_HAS_ID, id1);
}

TEST_F(TraceEventDataSourceTest, TaskExecutionEvent) {
  StartTraceEventDataSource(/*privacy_filtering_enabled=*/false, "toplevel");

  base::TaskAnnotator task_annotator;
  for (int i = 0; i < 2; ++i) {
    base::PendingTask task;
    task.task = base::DoNothing();
    task.posted_from = base::Location::CreateForTesting(
        "my_func", "my_file", 0, /*program_counter=*/&task);
    // TaskAnnotator::RunTask is responsible for emitting the task execution
    // event.
    task_annotator.RunTask("ThreadControllerImpl::RunTask1", task);
  }

  size_t packet_index = ExpectStandardPreamble();
  size_t category_iid = 1;
  size_t name_iid = 1;
  size_t posted_from_iid = 1;

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // Since the "toplevel" category involves events other than the two being
  // tested above, we need to skip forward and ignore ones we don't care about
  // in the test.
  while (true) {
    auto* e_packet = GetFinalizedPacket(packet_index);
    if (e_packet->interned_data().event_names_size() &&
        e_packet->interned_data().event_names()[0].name() ==
            "ThreadControllerImpl::RunTask1") {
      category_iid = e_packet->track_event().category_iids()[0];
      name_iid = e_packet->track_event().name_iid();
      posted_from_iid =
          e_packet->track_event().task_execution().posted_from_iid();
      EXPECT_NE(category_iid, 0u);
      EXPECT_NE(name_iid, 0u);
      EXPECT_NE(posted_from_iid, 0u);
      break;
    }
    packet_index++;
  }
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, category_iid, name_iid, TRACE_EVENT_PHASE_BEGIN);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 0);

  EXPECT_EQ(e_packet->track_event().task_execution().posted_from_iid(),
            posted_from_iid);
  const auto& locations = e_packet->interned_data().source_locations();
  EXPECT_EQ(locations.size(), 1);
  EXPECT_EQ(locations[0].file_name(), "my_file");
  EXPECT_EQ(locations[0].function_name(), "my_func");

  // Second event should refer to the same interning entries.
  auto* e_packet2 = GetFinalizedPacket(++packet_index);
  ExpectTraceEvent(e_packet2, category_iid, name_iid, TRACE_EVENT_PHASE_BEGIN);

  EXPECT_EQ(e_packet2->track_event().task_execution().posted_from_iid(),
            posted_from_iid);
  EXPECT_EQ(e_packet2->interned_data().source_locations().size(), 0);
}

TEST_F(TraceEventDataSourceTest, TaskExecutionEventWithoutFunction) {
  StartTraceEventDataSource(/*privacy_filtering_enabled=*/false, "toplevel");

  base::TaskAnnotator task_annotator;
  base::PendingTask task;
  task.task = base::DoNothing();
  task.posted_from =
      base::Location::CreateForTesting(/*function_name=*/nullptr, "my_file", 0,
                                       /*program_counter=*/&task);

  // TaskAnnotator::RunTask is responsible for emitting the task execution
  // event.
  task_annotator.RunTask("ThreadControllerImpl::RunTask1", task);

  size_t packet_index = ExpectStandardPreamble();
  size_t category_iid = 1;
  size_t name_iid = 1;
  size_t posted_from_iid = 1;

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // Since the "toplevel" category involves events other than the two being
  // tested above, we need to skip forward and ignore ones we don't care about
  // in the test.
  while (true) {
    auto* e_packet = GetFinalizedPacket(packet_index);
    if (e_packet->interned_data().event_names_size() &&
        e_packet->interned_data().event_names()[0].name() ==
            "ThreadControllerImpl::RunTask1") {
      category_iid = e_packet->track_event().category_iids()[0];
      name_iid = e_packet->track_event().name_iid();
      posted_from_iid =
          e_packet->track_event().task_execution().posted_from_iid();
      EXPECT_NE(category_iid, 0u);
      EXPECT_NE(name_iid, 0u);
      EXPECT_NE(posted_from_iid, 0u);
      break;
    }
    packet_index++;
  }
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, category_iid, name_iid, TRACE_EVENT_PHASE_BEGIN,
                   TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 0);

  EXPECT_EQ(e_packet->track_event().task_execution().posted_from_iid(),
            posted_from_iid);
  const auto& locations = e_packet->interned_data().source_locations();
  EXPECT_EQ(locations.size(), 1);
  EXPECT_EQ(locations[0].file_name(), "my_file");
  EXPECT_FALSE(locations[0].has_function_name());
}

TEST_F(TraceEventDataSourceTest, UpdateDurationOfCompleteEvent) {
  StartTraceEventDataSource();

  static const char kEventName[] = "bar";

  auto* category_group_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(kCategoryGroup);

  trace_event_internal::TraceID trace_event_trace_id(
      trace_event_internal::kNoId);

  // COMPLETE events are split into a BEGIN/END event pair. Adding the event
  // writes the BEGIN event immediately.
  auto handle = trace_event_internal::AddTraceEventWithThreadIdAndTimestamp(
      TRACE_EVENT_PHASE_COMPLETE, category_group_enabled, kEventName,
      trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),
      /*thread_id=*/1, base::TimeTicks() + base::Microseconds(10),
      trace_event_trace_id.id_flags() | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,
      trace_event_internal::kNoId);

  // Updating the duration of the event as it goes out of scope results in the
  // corresponding END event being written. These END events don't contain any
  // event names or categories in the proto format.
  base::trace_event::TraceLog::GetInstance()->UpdateTraceEventDurationExplicit(
      category_group_enabled, kEventName, handle, /*thread_id=*/1,
      /*explicit_timestamps=*/true, base::TimeTicks() + base::Microseconds(30),
      base::ThreadTicks());

  // Updating the duration of an event that wasn't added before tracing begun
  // will only emit an END event, again without category or name.
  handle.event_index = 0;
  base::trace_event::TraceLog::GetInstance()->UpdateTraceEventDurationExplicit(
      category_group_enabled, "other_event_name", handle, /*thread_id=*/1,
      /*explicit_timestamps=*/true, base::TimeTicks() + base::Microseconds(40),
      base::ThreadTicks());

  // Complete event for the current thread emits thread time, too.
  trace_event_internal::AddTraceEventWithThreadIdAndTimestamp(
      TRACE_EVENT_PHASE_COMPLETE, category_group_enabled, kEventName,
      trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),
      base::PlatformThread::CurrentId(),
      base::TimeTicks() + base::Microseconds(10),
      trace_event_trace_id.id_flags() | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,
      trace_event_internal::kNoId);

  size_t packet_index = ExpectStandardPreamble();

  // Thread track for the overridden tid.
  auto* tt_packet = GetFinalizedPacket(packet_index++);
  ExpectThreadTrack(tt_packet, /*thread_id=*/1);

  auto* b_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(
      b_packet, /*category_iid=*/1u, /*name_iid=*/1u, TRACE_EVENT_PHASE_BEGIN,
      TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP | TRACE_EVENT_FLAG_HAS_ID, /*id=*/0u,
      /*absolute_timestamp=*/10, /*tid_override=*/1,
      perfetto::ThreadTrack::ForThread(1));

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END, TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,
                   /*id=*/0u,
                   /*absolute_timestamp=*/30, /*tid_override=*/1,
                   perfetto::ThreadTrack::ForThread(1));

  auto* e2_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e2_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END, TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,
                   /*id=*/0u,
                   /*absolute_timestamp=*/40, /*tid_override=*/1,
                   perfetto::ThreadTrack::ForThread(1));

  auto* b2_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(
      b2_packet, /*category_iid=*/1u, /*name_iid=*/1u, TRACE_EVENT_PHASE_BEGIN,
      TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP | TRACE_EVENT_FLAG_HAS_ID, /*id=*/0u,
      /*absolute_timestamp=*/10, /*tid_override=*/0);
}

// TODO(b/236578755)
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
#define MAYBE_ExplicitThreadTimeForDifferentThread \
  DISABLED_ExplicitThreadTimeForDifferentThread
#else
#define MAYBE_ExplicitThreadTimeForDifferentThread \
  ExplicitThreadTimeForDifferentThread
#endif
TEST_F(TraceEventDataSourceTest, MAYBE_ExplicitThreadTimeForDifferentThread) {
  StartTraceEventDataSource();

  static const char kEventName[] = "bar";

  auto* category_group_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(kCategoryGroup);

  trace_event_internal::TraceID trace_event_trace_id(
      trace_event_internal::kNoId);

  // Chrome's main thread buffers and later flushes EarlyJava events on behalf
  // of other threads, including explicit thread time values. Such an event
  // should add descriptors for the other thread's track and for an absolute
  // thread time track for the other thread.
  trace_event_internal::AddTraceEventWithThreadIdAndTimestamps(
      TRACE_EVENT_PHASE_BEGIN, category_group_enabled, kEventName,
      trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),
      /*thread_id=*/1, base::TimeTicks() + base::Microseconds(10),
      base::ThreadTicks() + base::Microseconds(20),
      trace_event_trace_id.id_flags() | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP);

  size_t packet_index = ExpectStandardPreamble();

  // Thread track for the overridden tid.
  auto* tt_packet = GetFinalizedPacket(packet_index++);
  ExpectThreadTrack(tt_packet, /*thread_id=*/1);

  // Absolute thread time track for the overridden tid.
  auto* ttt_packet = GetFinalizedPacket(packet_index++);
  ExpectThreadTimeCounterTrack(ttt_packet, /*thread_id=*/1);

  auto* b_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(
      b_packet, /*category_iid=*/1u, /*name_iid=*/1u, TRACE_EVENT_PHASE_BEGIN,
      TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP | TRACE_EVENT_FLAG_HAS_ID, /*id=*/0u,
      /*absolute_timestamp=*/10, /*tid_override=*/1,
      perfetto::ThreadTrack::ForThread(1), /*explicit_thread_time=*/20);
}

TEST_F(TraceEventDataSourceTest, TrackSupportOnBeginAndEndWithLambda) {
  StartTraceEventDataSource();

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

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // Custom track descriptor.
  auto* td_packet = GetFinalizedPacket(packet_index++);
  ExpectChildTrack(td_packet, track.uuid, track.parent_uuid);
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  auto* b_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(b_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0, track);

  ExpectEventCategories(b_packet, {{1u, "browser"}});
  ExpectInternedEventNames(b_packet, {{1u, "bar"}});

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  auto* t_packet = GetFinalizedPacket(packet_index++);
  ExpectChildTrack(t_packet, track.uuid, track.parent_uuid);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  auto* e_packet = GetFinalizedPacket(packet_index++);

  ExpectTraceEvent(e_packet, /*category_iid=*/0, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0, track);
}

TEST_F(TraceEventDataSourceTest, TrackSupportOnBeginAndEnd) {
  StartTraceEventDataSource();

  auto track = perfetto::Track(1);

  TRACE_EVENT_BEGIN("browser", "bar", track);
  TRACE_EVENT_END("browser", track);

  size_t packet_index = ExpectStandardPreamble();

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // Custom track descriptor.
  auto* td_packet = GetFinalizedPacket(packet_index++);
  ExpectChildTrack(td_packet, track.uuid, track.parent_uuid);
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  auto* b_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(b_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0, track);

  ExpectEventCategories(b_packet, {{1u, "browser"}});
  ExpectInternedEventNames(b_packet, {{1u, "bar"}});

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  auto* t_packet = GetFinalizedPacket(packet_index++);
  ExpectChildTrack(t_packet, track.uuid, track.parent_uuid);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  auto* e_packet = GetFinalizedPacket(packet_index++);

  ExpectTraceEvent(e_packet, /*category_iid=*/0, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0, track);
}

TEST_F(TraceEventDataSourceTest, TrackSupportWithTimestamp) {
  StartTraceEventDataSource();

  auto timestamp = TRACE_TIME_TICKS_NOW() - base::Microseconds(100);
  auto track = perfetto::Track(1);

  TRACE_EVENT_BEGIN("browser", "bar", track, timestamp);

  size_t packet_index = ExpectStandardPreamble();

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // Custom track descriptor.
  auto* td_packet = GetFinalizedPacket(packet_index++);
  ExpectChildTrack(td_packet, track.uuid, track.parent_uuid);
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  auto* b_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(
      b_packet, /*category_iid=*/1u, /*name_iid=*/1u, TRACE_EVENT_PHASE_BEGIN,
      /*flags=*/0, /*id=*/0,
      /*absolute_timestamp=*/timestamp.since_origin().InMicroseconds(),
      /*tid_override=*/0, track);

  ExpectEventCategories(b_packet, {{1u, "browser"}});
  ExpectInternedEventNames(b_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, TrackSupportWithTimestampAndLambda) {
  StartTraceEventDataSource();

  auto timestamp = TRACE_TIME_TICKS_NOW() - base::Microseconds(100);
  auto track = perfetto::Track(1);
  bool lambda_called = false;

  TRACE_EVENT_BEGIN("browser", "bar", track, timestamp,
                    [&](perfetto::EventContext ctx) { lambda_called = true; });

  EXPECT_TRUE(lambda_called);

  size_t packet_index = ExpectStandardPreamble();

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // Custom track descriptor.
  auto* td_packet = GetFinalizedPacket(packet_index++);
  ExpectChildTrack(td_packet, track.uuid, track.parent_uuid);
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  auto* b_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(
      b_packet, /*category_iid=*/1u, /*name_iid=*/1u, TRACE_EVENT_PHASE_BEGIN,
      /*flags=*/0, /*id=*/0,
      /*absolute_timestamp=*/timestamp.since_origin().InMicroseconds(),
      /*tid_override=*/0, track);

  ExpectEventCategories(b_packet, {{1u, "browser"}});
  ExpectInternedEventNames(b_packet, {{1u, "bar"}});
}

// TODO(ddrone): following tests should be re-enabled once we figure out how
// tracks on scoped events supposed to work
TEST_F(TraceEventDataSourceTest, DISABLED_TrackSupport) {
  StartTraceEventDataSource();

  auto track = perfetto::Track(1);

  { TRACE_EVENT("browser", "bar", track); }

  size_t packet_index = ExpectStandardPreamble();

  auto* b_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(b_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0, track);

  ExpectEventCategories(b_packet, {{1u, "browser"}});
  ExpectInternedEventNames(b_packet, {{1u, "bar"}});

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0, track);
}

TEST_F(TraceEventDataSourceTest, DISABLED_TrackSupportWithLambda) {
  StartTraceEventDataSource();

  auto track = perfetto::Track(1);
  bool lambda_called = false;

  {
    TRACE_EVENT("browser", "bar", track,
                [&](perfetto::EventContext ctx) { lambda_called = true; });
  }

  EXPECT_TRUE(lambda_called);

  size_t packet_index = ExpectStandardPreamble();

  auto* b_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(b_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0, track);

  ExpectEventCategories(b_packet, {{1u, "browser"}});
  ExpectInternedEventNames(b_packet, {{1u, "bar"}});

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END, /*flags=*/0, /*id=*/0,
                   /*absolute_timestamp=*/0, /*tid_override=*/0, track);
}

// TODO(eseckler): Add a test with multiple events + same strings (cat, name,
// arg names).

// TODO(eseckler): Add a test with multiple events + same strings with reset.

TEST_F(TraceEventDataSourceTest, InternedStrings) {
  StartTraceEventDataSource(/*privacy_filtering_enabled=*/false,
                            "browser,ui,-*");

  size_t packet_index = 0u;
  for (size_t i = 0; i < 2; i++) {
    TRACE_EVENT_INSTANT1("browser", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 4);
    TRACE_EVENT_INSTANT1("browser", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 2);
    TRACE_EVENT_INSTANT1("ui", "e2", TRACE_EVENT_SCOPE_THREAD, "arg2", 1);

    packet_index = ExpectStandardPreamble(packet_index);

    // First packet needs to emit new interning entries
    auto* e_packet1 = GetFinalizedPacket(packet_index++);
    ExpectTraceEvent(e_packet1, /*category_iid=*/1u, /*name_iid=*/1u,
                     TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

    const auto& annotations1 = e_packet1->track_event().debug_annotations();
    EXPECT_EQ(annotations1.size(), 1);
    EXPECT_EQ(annotations1[0].name_iid(), 1u);
    EXPECT_EQ(annotations1[0].int_value(), 4);

    ExpectEventCategories(e_packet1, {{1u, "browser"}});
    ExpectInternedEventNames(e_packet1, {{1u, "e1"}});
    ExpectInternedDebugAnnotationNames(e_packet1, {{1u, "arg1"}});

    // Second packet refers to the interning entries from packet 1.
    auto* e_packet2 = GetFinalizedPacket(packet_index++);
    ExpectTraceEvent(e_packet2, /*category_iid=*/1u, /*name_iid=*/1u,
                     TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

    const auto& annotations2 = e_packet2->track_event().debug_annotations();
    EXPECT_EQ(annotations2.size(), 1);
    EXPECT_EQ(annotations2[0].name_iid(), 1u);
    EXPECT_EQ(annotations2[0].int_value(), 2);

    ExpectEventCategories(e_packet2, {});
    ExpectInternedEventNames(e_packet2, {});
    ExpectInternedDebugAnnotationNames(e_packet2, {});

    // Third packet uses different names, so emits new entries.
    auto* e_packet3 = GetFinalizedPacket(packet_index++);
    ExpectTraceEvent(e_packet3, /*category_iid=*/2u, /*name_iid=*/2u,
                     TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

    const auto& annotations3 = e_packet3->track_event().debug_annotations();
    EXPECT_EQ(annotations3.size(), 1);
    EXPECT_EQ(annotations3[0].name_iid(), 2u);
    EXPECT_EQ(annotations3[0].int_value(), 1);

    ExpectEventCategories(e_packet3, {{2u, "ui"}});
    ExpectInternedEventNames(e_packet3, {{2u, "e2"}});
    ExpectInternedDebugAnnotationNames(e_packet3, {{2u, "arg2"}});

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    // TODO(skyostil): Perfetto doesn't let us clear the interning state
    // explicitly, so not testing that here for now.
    if (!i)
      break;
#else
    // Resetting the interning state causes ThreadDescriptor and interning
    // entries to be emitted again, with the same interning IDs.
    TraceEventDataSource::GetInstance()->ClearIncrementalState();
#endif
  }
}

// TODO(skyostil): Implement post-process event filtering.
TEST_F(TraceEventDataSourceTest, FilteringSimpleTraceEvent) {
  StartTraceEventDataSource(/* privacy_filtering_enabled =*/true);
  TRACE_EVENT_BEGIN0(kCategoryGroup, "bar");

  size_t packet_index = ExpectStandardPreamble(
      /*start_packet_index=*/0u,
      /*privacy_filtering_enabled=*/true);

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});
  ExpectInternedDebugAnnotationNames(e_packet, {});
}

TEST_F(TraceEventDataSourceTest, FilteringEventWithArgs) {
  StartTraceEventDataSource(/* privacy_filtering_enabled =*/true);
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // New SDK does not accept TRACE_EVENT_FLAG values.
  TRACE_EVENT_INSTANT(kCategoryGroup, "bar", "foo", 42, "bar", "string_val");
#else
  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42, "bar", "string_val");
#endif

  size_t packet_index = ExpectStandardPreamble(
      /*start_packet_index=*/0u,
      /*privacy_filtering_enabled=*/true);

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 0);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});
  ExpectInternedDebugAnnotationNames(e_packet, {});
}

TEST_F(TraceEventDataSourceTest, FilteringEventWithFlagCopy) {
  StartTraceEventDataSource(/* privacy_filtering_enabled =*/true);
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // 1). Perfetto SDK does not accept TRACE_EVENT_FLAG values.
  // 2). To include dynamic event names despite privacy filtering, we need to
  //     manually `set event()->set_name()`. Java names are a valid use case of
  //     this.
  TRACE_EVENT_INSTANT(kCategoryGroup, TRACE_STR_COPY(std::string("bar")),
                      "arg1_name", "arg1_val", "arg2_name", "arg2_val");
  TRACE_EVENT_INSTANT(
      kCategoryGroup, nullptr,
      [](perfetto::EventContext& ev) { ev.event()->set_name("javaName"); },
      "arg1_name", "arg1_val", "arg2_name", "arg2_val");
#else
  TRACE_EVENT_COPY_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD,
                            "arg1_name", "arg1_val", "arg2_name", "arg2_val");
  TRACE_EVENT_COPY_INSTANT2(
      kCategoryGroup, "javaName",
      TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_JAVA_STRING_LITERALS,
      "arg1_name", "arg1_val", "arg2_name", "arg2_val");
#endif

  size_t packet_index = ExpectStandardPreamble(
      /*start_packet_index=*/0u,
      /*privacy_filtering_enabled=*/true);

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, kPrivacyFiltered,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 0);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectInternedDebugAnnotationNames(e_packet, {});

  e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, "javaName",
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations2 = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations2.size(), 0);

  ExpectInternedDebugAnnotationNames(e_packet, {});
}

TEST_F(TraceEventDataSourceTest, FilteringMetadataSource) {
  auto* metadata_source = TraceEventMetadataSource::GetInstance();
  metadata_source->AddGeneratorFunction(base::BindRepeating([]() {
    base::Value::Dict metadata;
    metadata.Set("foo_int", 42);
    metadata.Set("foo_str", "bar");
    metadata.Set("foo_bool", true);

    base::Value::Dict child_dict;
    child_dict.Set("child_str", "child_val");
    metadata.Set("child_dict", std::move(child_dict));
    return absl::optional<base::Value::Dict>(std::move(metadata));
  }));

  StartMetaDataSource(/*privacy_filtering_enabled=*/true);
  StopMetaDataSource();

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  EXPECT_TRUE(legacy_metadata_packets_.empty());
#else
  EXPECT_FALSE(producer_client()->GetChromeMetadata());
#endif
}

TEST_F(TraceEventDataSourceTest, ProtoMetadataSource) {
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

  StartMetaDataSource(/*privacy_filtering_enabled=*/true);
  StopMetaDataSource();

  const auto* metadata = GetProtoChromeMetadata();
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
    // Reset the data source to pick it up the command line flag.
    PerfettoTracedProcess::Get()->ClearDataSourcesForTesting();
    TraceEventDataSource::ResetForTesting();
    TraceEventDataSourceTest::SetUp();
  }
};

// TODO(skyostil): Add support for disabling interning in Perfetto.
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
#define MAYBE_InterningScopedToPackets DISABLED_InterningScopedToPackets
#else
#define MAYBE_InterningScopedToPackets InterningScopedToPackets
#endif
TEST_F(TraceEventDataSourceNoInterningTest, MAYBE_InterningScopedToPackets) {
  StartTraceEventDataSource(/*privacy_filtering_enabled=*/false,
                            "browser,ui,-*");

  TRACE_EVENT_INSTANT1("browser", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 4);
  TRACE_EVENT_INSTANT1("browser", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 2);
  TRACE_EVENT_INSTANT1("ui", "e2", TRACE_EVENT_SCOPE_THREAD, "arg2", 1);

  size_t packet_index = ExpectStandardPreamble();

  // First packet needs to emit new interning entries
  auto* e_packet1 = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet1, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations1 = e_packet1->track_event().debug_annotations();
  EXPECT_EQ(annotations1.size(), 1);
  EXPECT_EQ(annotations1[0].name_iid(), 1u);
  EXPECT_EQ(annotations1[0].int_value(), 4);

  ExpectEventCategories(e_packet1, {{1u, "browser"}});
  ExpectInternedEventNames(e_packet1, {{1u, "e1"}});
  ExpectInternedDebugAnnotationNames(e_packet1, {{1u, "arg1"}});

  // Second packet reemits the entries the same way.
  auto* e_packet2 = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet2, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations2 = e_packet2->track_event().debug_annotations();
  EXPECT_EQ(annotations2.size(), 1);
  EXPECT_EQ(annotations2[0].name_iid(), 1u);
  EXPECT_EQ(annotations2[0].int_value(), 2);

  ExpectEventCategories(e_packet1, {{1u, "browser"}});
  ExpectInternedEventNames(e_packet1, {{1u, "e1"}});
  ExpectInternedDebugAnnotationNames(e_packet1, {{1u, "arg1"}});

  // Third packet emits entries with the same IDs but different strings.
  auto* e_packet3 = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet3, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations3 = e_packet3->track_event().debug_annotations();
  EXPECT_EQ(annotations3.size(), 1);
  EXPECT_EQ(annotations3[0].name_iid(), 1u);
  EXPECT_EQ(annotations3[0].int_value(), 1);

  ExpectEventCategories(e_packet3, {{1u, "ui"}});
  ExpectInternedEventNames(e_packet3, {{1u, "e2"}});
  ExpectInternedDebugAnnotationNames(e_packet3, {{1u, "arg2"}});
}

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
// TODO(skyostil): Add support for startup tracing.
#define MAYBE_StartupTracingTimeout DISABLED_StartupTracingTimeout
#else
#define MAYBE_StartupTracingTimeout StartupTracingTimeout
#endif
TEST_F(TraceEventDataSourceTest, MAYBE_StartupTracingTimeout) {
  constexpr char kStartupTestEvent1[] = "startup_registry";
  auto* data_source = TraceEventDataSource::GetInstance();
  PerfettoTracedProcess::Get()->AddDataSource(data_source);

  // Start startup tracing with no timeout. This would cause startup tracing to
  // abort and flush as soon the current thread can run tasks.
  producer_client()->set_startup_tracing_timeout_for_testing(base::TimeDelta());
  producer_client()->SetupStartupTracing(
      base::trace_event::TraceConfig("foo,-*", ""),
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
            // This event can be hit while flushing the startup tracing session,
            // or when the subsequent tracing session is started or when even
            // that one was already stopped.
            TRACE_EVENT_BEGIN0(kCategoryGroup, "maybe_lost");

            // Make sure that this thread's the trace writer is cleared away.
            TraceEventDataSource::FlushCurrentThread();
          },
          std::move(wait_for_start_tracing)));

  // Let tasks run on this thread, which should abort startup tracing and flush
  // TraceLog, since the data source hasn't been started by a producer.
  producer_client()->OnThreadPoolAvailable();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(base::trace_event::TraceLog::GetInstance()->IsEnabled());

  // Start tracing while flush is running.
  perfetto::DataSourceConfig config;
  data_source->StartTracingImpl(producer_client(), config);
  wait_ptr->Signal();

  // Verify that the trace buffer does not have the event added to startup
  // tracing session.
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

  // Make sure that the TraceWriter destruction task posted from the ThreadPool
  // task's flush is executed.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  RunUntilIdle();
#endif
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
  StartTraceEventDataSource();

  bool begin_called = false;

  TRACE_EVENT_BEGIN("browser", "bar", [&](perfetto::EventContext ctx) {
    begin_called = true;
    ctx.event()->set_log_message()->set_body_iid(42);
  });

  EXPECT_TRUE(begin_called);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOnEnd) {
  StartTraceEventDataSource();

  bool end_called = false;

  TRACE_EVENT_END("browser", [&](perfetto::EventContext ctx) {
    end_called = true;
    ctx.event()->set_log_message()->set_body_iid(42);
  });

  EXPECT_TRUE(end_called);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END);

  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOnBeginAndEnd) {
  StartTraceEventDataSource();

  TRACE_EVENT_BEGIN("browser", "bar", [&](perfetto::EventContext ctx) {
    ctx.event()->set_log_message()->set_body_iid(42);
  });
  TRACE_EVENT_END("browser", [&](perfetto::EventContext ctx) {
    ctx.event()->set_log_message()->set_body_iid(84);
  });

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);

  e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END);

  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 84u);
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOnInstant) {
  StartTraceEventDataSource();

  TRACE_EVENT_INSTANT("browser", "bar", [&](perfetto::EventContext ctx) {
    ctx.event()->set_log_message()->set_body_iid(42);
  });

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOnScoped) {
  StartTraceEventDataSource();

  // Use a if statement with no brackets to ensure that the Scoped TRACE_EVENT
  // macro properly emits the end event when leaving the single expression
  // associated with the if(true) statement.
  if (true)
    TRACE_EVENT("browser", "bar", [&](perfetto::EventContext ctx) {
      ctx.event()->set_log_message()->set_body_iid(42);
    });

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);

  e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END);

  EXPECT_FALSE(e_packet->track_event().has_log_message());
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOnScopedCapture) {
  StartTraceEventDataSource();

  bool called = false;
  {
    TRACE_EVENT("browser", "bar", [&](perfetto::EventContext ctx) {
      called = true;
      ctx.event()->set_log_message()->set_body_iid(42);
    });
  }

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);

  e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END);

  EXPECT_FALSE(e_packet->track_event().has_log_message());
  EXPECT_TRUE(called);
}

TEST_F(TraceEventDataSourceTest, TypedArgumentsTracingOnScopedMultipleEvents) {
  StartTraceEventDataSource();

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
  auto* e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 42u);

  // The second TRACE_EVENT begin.
  e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(), 43u);

  // The second TRACE_EVENT end.
  e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END);

  EXPECT_FALSE(e_packet->track_event().has_log_message());

  // The first TRACE_EVENT end.
  e_packet = GetFinalizedPacket(packet_index++);
  ExpectTraceEvent(e_packet, /*category_iid=*/0u, /*name_iid=*/0u,
                   TRACE_EVENT_PHASE_END);
  EXPECT_FALSE(e_packet->track_event().has_log_message());
}

TEST_F(TraceEventDataSourceTest, HistogramSampleTraceConfigEmpty) {
  StartTraceEventDataSource(/*privacy_filtering_enabled=*/false,
                            "-*,disabled-by-default-histogram_samples");

  UMA_HISTOGRAM_BOOLEAN("Foo.Bar", true);

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);

  ExpectEventCategories(e_packet,
                        {{1u, TRACE_DISABLED_BY_DEFAULT("histogram_samples")}});
  ExpectInternedEventNames(e_packet, {{1u, "HistogramSample"}});
  ASSERT_TRUE(e_packet->track_event().has_chrome_histogram_sample());
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().sample(), 1u);
  ASSERT_TRUE(e_packet->has_interned_data());
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().name_iid(),
            e_packet->interned_data().histogram_names()[0].iid());
  EXPECT_EQ(e_packet->interned_data().histogram_names()[0].name(), "Foo.Bar");
}

// TODO(crbug.com/1464718): The test is flaky across platforms.
TEST_F(TraceEventDataSourceTest, DISABLED_HistogramSampleTraceConfigNotEmpty) {
  std::vector<std::string> histograms;
  histograms.push_back("Foo1.Bar1");
  histograms.push_back("Foo3.Bar3");
  StartTraceEventDataSource(/*privacy_filtering_enabled=*/false,
                            "-*,disabled-by-default-histogram_samples",
                            std::move(histograms));

  UMA_HISTOGRAM_BOOLEAN("Foo1.Bar1", true);
  UMA_HISTOGRAM_BOOLEAN("Foo2.Bar2", true);
  UMA_HISTOGRAM_BOOLEAN("Foo3.Bar3", true);
  base::RunLoop().RunUntilIdle();

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);

  ExpectEventCategories(e_packet,
                        {{1u, TRACE_DISABLED_BY_DEFAULT("histogram_samples")}});
  ExpectInternedEventNames(e_packet, {{1u, "HistogramSample"}});
  ASSERT_TRUE(e_packet->track_event().has_chrome_histogram_sample());
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().name_hash(),
            base::HashMetricName("Foo1.Bar1"));
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().sample(), 1u);
  ASSERT_TRUE(e_packet->has_interned_data());
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().name_iid(),
            e_packet->interned_data().histogram_names()[0].iid());
  EXPECT_EQ(e_packet->interned_data().histogram_names()[0].name(), "Foo1.Bar1");

  e_packet = GetFinalizedPacket(packet_index++);

  ExpectEventCategories(e_packet, {});
  ExpectInternedEventNames(e_packet, {});
  ASSERT_TRUE(e_packet->track_event().has_chrome_histogram_sample());
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().name_hash(),
            base::HashMetricName("Foo3.Bar3"));
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().sample(), 1u);
  ASSERT_TRUE(e_packet->has_interned_data());
  EXPECT_EQ(e_packet->track_event().chrome_histogram_sample().name_iid(),
            e_packet->interned_data().histogram_names()[0].iid());
  EXPECT_EQ(e_packet->interned_data().histogram_names()[0].name(), "Foo3.Bar3");
}

TEST_F(TraceEventDataSourceTest, UserActionEvent) {
  base::SetRecordActionTaskRunner(
      base::SingleThreadTaskRunner::GetCurrentDefault());

  StartTraceEventDataSource(/*privacy_filtering_enabled=*/false,
                            "-*,disabled-by-default-user_action_samples");

  // Wait for registering callback on current thread.
  base::RunLoop().RunUntilIdle();

  base::RecordAction(base::UserMetricsAction("Test_Action"));

  size_t packet_index = ExpectStandardPreamble();

  auto* e_packet = GetFinalizedPacket(packet_index++);

  ExpectEventCategories(
      e_packet, {{1u, TRACE_DISABLED_BY_DEFAULT("user_action_samples")}});
  ExpectInternedEventNames(e_packet, {{1u, "UserAction"}});
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
  StartTraceEventDataSource();

  {
    TRACE_EVENT("browser", "bar", [&](perfetto::EventContext ctx) {
      size_t iid = InternedLogMessageBody::Get(&ctx, "Hello interned world!");
      ctx.event()->set_log_message()->set_body_iid(iid);
    });
  }
  size_t packet_index = ExpectStandardPreamble();
  auto* e_packet = GetFinalizedPacket(packet_index++);

  ExpectEventCategories(e_packet, {{1u, "browser"}});
  ExpectInternedEventNames(e_packet, {{1u, "bar"}});
  ASSERT_TRUE(e_packet->track_event().has_log_message());
  ASSERT_TRUE(e_packet->has_interned_data());
  EXPECT_EQ(e_packet->track_event().log_message().body_iid(),
            e_packet->interned_data().log_message_body()[0].iid());
  ASSERT_EQ("Hello interned world!",
            e_packet->interned_data().log_message_body()[0].body());
}

TEST_F(TraceEventDataSourceTest, TypedAndUntypedEventsWithDebugAnnotations) {
  StartTraceEventDataSource();

  TRACE_EVENT_INSTANT1("browser", "Event1", TRACE_EVENT_SCOPE_THREAD, "arg1",
                       1);
  TRACE_EVENT_INSTANT("browser", "Event2", "arg2", 2);

  size_t packet_index = ExpectStandardPreamble();
  auto* e_packet1 = GetFinalizedPacket(packet_index++);

  ExpectEventCategories(e_packet1, {{1u, "browser"}});
  ExpectInternedEventNames(e_packet1, {{1u, "Event1"}});
  ExpectInternedDebugAnnotationNames(e_packet1, {{1u, "arg1"}});

  auto* e_packet2 = GetFinalizedPacket(packet_index++);

  ExpectInternedEventNames(e_packet2, {{2u, "Event2"}});
  ExpectInternedDebugAnnotationNames(e_packet2, {{2u, "arg2"}});
}

TEST_F(TraceEventDataSourceTest, EmptyPacket) {
  StartTraceEventDataSource();

  TRACE_EVENT_INSTANT("browser", "Event");
  PERFETTO_INTERNAL_ADD_EMPTY_EVENT();
  // Second empty packet should not be emitted because the prior packet was
  // already an empty packet.
  PERFETTO_INTERNAL_ADD_EMPTY_EVENT();

  size_t packet_index = ExpectStandardPreamble();
  auto* instant_packet = GetFinalizedPacket(packet_index++);

  ExpectEventCategories(instant_packet, {{1u, "browser"}});
  ExpectInternedEventNames(instant_packet, {{1u, "Event"}});

// The client library employs a real tracing service, which skips empty packets
// when reading from the trace buffer. The functionality of the
// PERFETTO_INTERNAL_ADD_EMPTY_EVENT macro is instead tested in Perfetto's API
// integration tests.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // Expect one more empty event in addition to the one emitted after the
  // ProcessDescriptor.
  EXPECT_EQ(producer_client()->empty_finalized_packets_count(), 2);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
}

TEST_F(TraceEventDataSourceTest, SupportNullptrEventName) {
  StartTraceEventDataSource();
  TRACE_EVENT_INSTANT("browser", nullptr, [&](::perfetto::EventContext& ctx) {
    ctx.event()->set_name(std::string("EventName"));
  });
  const auto& packets = GetFinalizedPackets();
  ASSERT_GT(packets.size(), 0u);

  const perfetto::protos::TracePacket* track_event_packet = nullptr;
  for (const auto& packet : packets) {
    if (packet->has_track_event()) {
      track_event_packet = packet.get();
      break;
    }
  }
  EXPECT_NE(track_event_packet, nullptr);

  EXPECT_FALSE(track_event_packet->track_event().has_name_iid());
  EXPECT_TRUE(track_event_packet->track_event().has_name());
  EXPECT_EQ("EventName", track_event_packet->track_event().name());
  EXPECT_TRUE(track_event_packet->has_interned_data());
  EXPECT_EQ(0, track_event_packet->interned_data().event_names().size());
}

// TODO(eseckler): Add startup tracing unittests.

}  // namespace

}  // namespace tracing
