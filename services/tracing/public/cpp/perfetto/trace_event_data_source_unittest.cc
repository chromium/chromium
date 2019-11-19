// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/thread_instruction_count.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/base/utils.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_null_delegate.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_writer.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.pb.h"

using TrackEvent = perfetto::protos::TrackEvent;

namespace tracing {

namespace {

constexpr char kTestThread[] = "CrTestMain";
constexpr const char kCategoryGroup[] = "foo";

class MockProducerClient : public ProducerClient {
 public:
  explicit MockProducerClient(
      std::unique_ptr<PerfettoTaskRunner> main_thread_task_runner)
      : ProducerClient(main_thread_task_runner.get()),
        delegate_(perfetto::base::kPageSize),
        stream_(&delegate_),
        main_thread_task_runner_(std::move(main_thread_task_runner)) {
    trace_packet_.Reset(&stream_);
  }

  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::BufferID target_buffer,
      perfetto::BufferExhaustedPolicy =
          perfetto::BufferExhaustedPolicy::kDefault) override;

  void FlushPacketIfPossible() {
    // GetNewBuffer() in ScatteredStreamWriterNullDelegate doesn't
    // actually return a new buffer, but rather lets us access the buffer
    // buffer already used by protozero to write the TracePacket into.
    protozero::ContiguousMemoryRange buffer = delegate_.GetNewBuffer();

    uint32_t message_size = trace_packet_.Finalize();
    if (message_size) {
      EXPECT_GE(buffer.size(), message_size);

      auto proto = std::make_unique<perfetto::protos::TracePacket>();
      EXPECT_TRUE(proto->ParseFromArray(buffer.begin, message_size));
      if (proto->has_chrome_events() &&
                 proto->chrome_events().metadata().size() > 0) {
        legacy_metadata_packets_.push_back(std::move(proto));
      } else if (proto->has_chrome_metadata()) {
        proto_metadata_packets_.push_back(std::move(proto));
      } else {
        finalized_packets_.push_back(std::move(proto));
      }
    }

    stream_.Reset(buffer);
    trace_packet_.Reset(&stream_);
  }

  perfetto::protos::pbzero::TracePacket* NewTracePacket() {
    FlushPacketIfPossible();

    return &trace_packet_;
  }

  size_t GetFinalizedPacketCount() {
    FlushPacketIfPossible();
    return finalized_packets_.size();
  }

  const perfetto::protos::TracePacket* GetFinalizedPacket(
      size_t packet_index = 0) {
    FlushPacketIfPossible();
    EXPECT_GT(finalized_packets_.size(), packet_index);
    return finalized_packets_[packet_index].get();
  }

  const google::protobuf::RepeatedPtrField<perfetto::protos::ChromeMetadata>
  GetChromeMetadata(size_t packet_index = 0) {
    FlushPacketIfPossible();
    if (legacy_metadata_packets_.empty()) {
      return google::protobuf::RepeatedPtrField<
          perfetto::protos::ChromeMetadata>();
    }
    EXPECT_GT(legacy_metadata_packets_.size(), packet_index);

    auto event_bundle = legacy_metadata_packets_[packet_index]->chrome_events();
    return event_bundle.metadata();
  }

  const perfetto::protos::ChromeMetadataPacket* GetProtoChromeMetadata(
      size_t packet_index = 0) {
    FlushPacketIfPossible();
    EXPECT_GT(proto_metadata_packets_.size(), packet_index);
    return &proto_metadata_packets_[packet_index]->chrome_metadata();
  }

  const std::vector<std::unique_ptr<perfetto::protos::TracePacket>>&
  finalized_packets() {
    return finalized_packets_;
  }

 private:
  std::vector<std::unique_ptr<perfetto::protos::TracePacket>>
      finalized_packets_;
  std::vector<std::unique_ptr<perfetto::protos::TracePacket>>
      legacy_metadata_packets_;
  std::vector<std::unique_ptr<perfetto::protos::TracePacket>>
      proto_metadata_packets_;
  perfetto::protos::pbzero::TracePacket trace_packet_;
  protozero::ScatteredStreamWriterNullDelegate delegate_;
  protozero::ScatteredStreamWriter stream_;
  std::unique_ptr<PerfettoTaskRunner> main_thread_task_runner_;
};

// For sequences/threads other than our own, we just want to ignore
// any events coming in.
class DummyTraceWriter : public perfetto::TraceWriter {
 public:
  DummyTraceWriter()
      : delegate_(perfetto::base::kPageSize), stream_(&delegate_) {}

  perfetto::TraceWriter::TracePacketHandle NewTracePacket() override {
    stream_.Reset(delegate_.GetNewBuffer());
    trace_packet_.Reset(&stream_);

    return perfetto::TraceWriter::TracePacketHandle(&trace_packet_);
  }

  void Flush(std::function<void()> callback = {}) override {}

  perfetto::WriterID writer_id() const override {
    return perfetto::WriterID(0);
  }

  uint64_t written() const override { return 0u; }

 private:
  perfetto::protos::pbzero::TracePacket trace_packet_;
  protozero::ScatteredStreamWriterNullDelegate delegate_;
  protozero::ScatteredStreamWriter stream_;
};

class MockTraceWriter : public perfetto::TraceWriter {
 public:
  explicit MockTraceWriter(MockProducerClient* producer_client)
      : producer_client_(producer_client) {}

  perfetto::TraceWriter::TracePacketHandle NewTracePacket() override {
    return perfetto::TraceWriter::TracePacketHandle(
        producer_client_->NewTracePacket());
  }

  void Flush(std::function<void()> callback = {}) override {}

  perfetto::WriterID writer_id() const override {
    return perfetto::WriterID(0);
  }

  uint64_t written() const override { return 0u; }

 private:
  MockProducerClient* producer_client_;
};

std::unique_ptr<perfetto::TraceWriter> MockProducerClient::CreateTraceWriter(
    perfetto::BufferID target_buffer,
    perfetto::BufferExhaustedPolicy) {
  // We attempt to destroy TraceWriters on thread shutdown in
  // ThreadLocalStorage::Slot, by posting them to the ProducerClient taskrunner,
  // but there's no guarantee that this will succeed if that taskrunner is also
  // shut down.
  ANNOTATE_SCOPED_MEMORY_LEAK;
  if (main_thread_task_runner_->GetOrCreateTaskRunner()
          ->RunsTasksInCurrentSequence()) {
    return std::make_unique<MockTraceWriter>(this);
  } else {
    return std::make_unique<DummyTraceWriter>();
  }
}

class TraceEventDataSourceTest : public testing::Test {
 public:
  void SetUp() override {
    PerfettoTracedProcess::ResetTaskRunnerForTesting();
    PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner();
    auto perfetto_wrapper = std::make_unique<PerfettoTaskRunner>(
        task_environment_.GetMainThreadTaskRunner());
    producer_client_ =
        std::make_unique<MockProducerClient>(std::move(perfetto_wrapper));
    base::ThreadIdNameManager::GetInstance()->SetName(kTestThread);
    TraceEventMetadataSource::GetInstance()->ResetForTesting();
  }

  void TearDown() override {
    if (base::trace_event::TraceLog::GetInstance()->IsEnabled()) {
      base::RunLoop wait_for_tracelog_flush;

      TraceEventDataSource::GetInstance()->StopTracing(base::BindRepeating(
          [](const base::RepeatingClosure& quit_closure) {
            quit_closure.Run();
          },
          wait_for_tracelog_flush.QuitClosure()));

      wait_for_tracelog_flush.Run();
    }

    // As MockTraceWriter keeps a pointer to our MockProducerClient,
    // we need to make sure to clean it up from TLS. The other sequences
    // get DummyTraceWriters that we don't care about.
    TraceEventDataSource::GetInstance()->FlushCurrentThread();
    producer_client_.reset();
  }

  void CreateTraceEventDataSource(bool privacy_filtering_enabled = false,
                                  bool start_trace = true) {
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
      TraceEventDataSource::GetInstance()->StartTracing(producer_client(),
                                                        config);
    }
  }

  MockProducerClient* producer_client() { return producer_client_.get(); }

  void ExpectThreadDescriptor(const perfetto::protos::TracePacket* packet,
                              int64_t min_timestamp = 1u,
                              int64_t min_thread_time = 1u,
                              bool filtering_enabled = false) {
    EXPECT_TRUE(packet->has_thread_descriptor());
    EXPECT_NE(packet->thread_descriptor().pid(), 0);
    EXPECT_NE(packet->thread_descriptor().tid(), 0);
    EXPECT_GE(packet->thread_descriptor().reference_timestamp_us(),
              last_timestamp_);
    EXPECT_GE(packet->thread_descriptor().reference_thread_time_us(),
              last_thread_time_);
    EXPECT_LE(packet->thread_descriptor().reference_timestamp_us(),
              TRACE_TIME_TICKS_NOW().since_origin().InMicroseconds());
    if (base::ThreadTicks::IsSupported()) {
      EXPECT_LE(packet->thread_descriptor().reference_thread_time_us(),
                base::ThreadTicks::Now().since_origin().InMicroseconds());
    }
    if (filtering_enabled) {
      EXPECT_FALSE(packet->thread_descriptor().has_thread_name());
      EXPECT_EQ(perfetto::protos::ThreadDescriptor::CHROME_THREAD_MAIN,
                packet->thread_descriptor().chrome_thread_type());
    }

    last_timestamp_ = packet->thread_descriptor().reference_timestamp_us();
    last_thread_time_ = packet->thread_descriptor().reference_thread_time_us();

    EXPECT_EQ(packet->interned_data().event_categories_size(), 0);
    EXPECT_EQ(packet->interned_data().event_names_size(), 0);

    // ThreadDescriptor is only emitted when incremental state was reset, and
    // thus also always serves as indicator for the state reset to the consumer.
    EXPECT_TRUE(packet->incremental_state_cleared());
  }

  void ExpectProcessDescriptor(const perfetto::protos::TracePacket* packet) {
    EXPECT_TRUE(packet->has_process_descriptor());
    EXPECT_NE(packet->process_descriptor().pid(), 0);
    EXPECT_EQ(packet->process_descriptor().chrome_process_type(),
              perfetto::protos::ProcessDescriptor::PROCESS_UNSPECIFIED);
  }

  void ExpectTraceEvent(const perfetto::protos::TracePacket* packet,
                        uint32_t category_iid,
                        uint32_t name_iid,
                        char phase,
                        uint32_t flags = 0,
                        uint64_t id = 0,
                        int64_t absolute_timestamp = 0,
                        int32_t tid_override = 0,
                        int32_t pid_override = 0,
                        int64_t duration = 0) {
    EXPECT_TRUE(packet->has_track_event());

    if (absolute_timestamp > 0) {
      EXPECT_TRUE(packet->track_event().has_timestamp_absolute_us());
      EXPECT_EQ(packet->track_event().timestamp_absolute_us(),
                absolute_timestamp);
    } else {
      EXPECT_TRUE(packet->track_event().has_timestamp_delta_us());
      EXPECT_GE(packet->track_event().timestamp_delta_us(), 0);
      EXPECT_LE(last_timestamp_ + packet->track_event().timestamp_delta_us(),
                TRACE_TIME_TICKS_NOW().since_origin().InMicroseconds());
      last_timestamp_ += packet->track_event().timestamp_delta_us();
    }
    if (packet->track_event().has_thread_time_delta_us()) {
      EXPECT_LE(
          last_thread_time_ + packet->track_event().thread_time_delta_us(),
          TRACE_TIME_TICKS_NOW().since_origin().InMicroseconds());
      last_thread_time_ += packet->track_event().thread_time_delta_us();
    }

    EXPECT_EQ(packet->track_event().category_iids_size(), 1);
    EXPECT_EQ(packet->track_event().category_iids(0), category_iid);
    EXPECT_TRUE(packet->track_event().has_legacy_event());

    const auto& legacy_event = packet->track_event().legacy_event();
    EXPECT_EQ(legacy_event.name_iid(), name_iid);
    EXPECT_EQ(legacy_event.phase(), phase);
    EXPECT_EQ(legacy_event.duration_us(), duration);

    if (phase == TRACE_EVENT_PHASE_INSTANT) {
      switch (flags & TRACE_EVENT_FLAG_SCOPE_MASK) {
        case TRACE_EVENT_SCOPE_GLOBAL:
          EXPECT_EQ(legacy_event.instant_event_scope(),
                    TrackEvent::LegacyEvent::SCOPE_GLOBAL);
          break;

        case TRACE_EVENT_SCOPE_PROCESS:
          EXPECT_EQ(legacy_event.instant_event_scope(),
                    TrackEvent::LegacyEvent::SCOPE_PROCESS);
          break;

        case TRACE_EVENT_SCOPE_THREAD:
          EXPECT_EQ(legacy_event.instant_event_scope(),
                    TrackEvent::LegacyEvent::SCOPE_THREAD);
          break;
      }
    } else {
      EXPECT_EQ(legacy_event.instant_event_scope(),
                TrackEvent::LegacyEvent::SCOPE_UNSPECIFIED);
    }

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

    EXPECT_EQ(legacy_event.tid_override(), tid_override);
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

  std::unique_ptr<MockProducerClient> producer_client_;
  int64_t last_timestamp_ = 0;
  int64_t last_thread_time_ = 0;
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

  auto metadata = producer_client()->GetChromeMetadata();
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

  auto metadata = producer_client()->GetChromeMetadata();
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

  auto metadata = producer_client()->GetChromeMetadata();
  EXPECT_EQ(4, metadata.size());
  MetadataHasNamedValue(metadata, "foo_int", 42);
  MetadataHasNamedValue(metadata, "foo_str", "bar");
  MetadataHasNamedValue(metadata, "foo_bool", true);

  auto child_dict = std::make_unique<base::DictionaryValue>();
  child_dict->SetString("child_str", "child_val");
  MetadataHasNamedValue(metadata, "child_dict", *child_dict);

  metadata = producer_client()->GetChromeMetadata(1);
  EXPECT_EQ(1, metadata.size());
  MetadataHasNamedValue(metadata, "before_int", 42);
}

TEST_F(TraceEventDataSourceTest, BasicTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_BEGIN0(kCategoryGroup, "bar");

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);

  auto* td_packet = producer_client()->GetFinalizedPacket();
  ExpectThreadDescriptor(td_packet);

  auto* e_packet = producer_client()->GetFinalizedPacket(1);
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);

  auto* td_packet = producer_client()->GetFinalizedPacket();
  ExpectThreadDescriptor(td_packet);

  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(
      e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
      TRACE_EVENT_PHASE_ASYNC_BEGIN,
      TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP | TRACE_EVENT_FLAG_HAS_ID, /*id=*/42u,
      /*absolute_timestamp=*/424242, /*tid_override=*/4242);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, InstantTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT0(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD);

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);

  auto* td_packet = producer_client()->GetFinalizedPacket();
  ExpectThreadDescriptor(td_packet);

  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, EventWithStringArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD,
                       "arg1_name", "arg1_val", "arg2_name", "arg2_val");

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);

  auto* td_packet = producer_client()->GetFinalizedPacket();
  ExpectThreadDescriptor(td_packet);

  auto* e_packet = producer_client()->GetFinalizedPacket(1);
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);

  auto* td_packet = producer_client()->GetFinalizedPacket();
  ExpectThreadDescriptor(td_packet);

  auto* e_packet = producer_client()->GetFinalizedPacket(1);
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 3u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
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
  auto* e_packet2 = producer_client()->GetFinalizedPacket(2);
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
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

  auto handle = trace_event_internal::AddTraceEventWithThreadIdAndTimestamp(
      TRACE_EVENT_PHASE_COMPLETE, category_group_enabled, kEventName,
      trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),
      1 /* thread_id */,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(10),
      trace_event_trace_id.id_flags() | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,
      trace_event_internal::kNoId);

  base::trace_event::TraceLog::GetInstance()->UpdateTraceEventDurationExplicit(
      category_group_enabled, kEventName, handle,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(30),
      base::ThreadTicks() + base::TimeDelta::FromMicroseconds(50),
      base::trace_event::ThreadInstructionCount());

  // The call to UpdateTraceEventDurationExplicit should have successfully
  // updated the duration of the event which was added in the
  // AddTraceEventWithThreadIdAndTimestamp call.
  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(
      e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
      TRACE_EVENT_PHASE_COMPLETE,
      TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP | TRACE_EVENT_FLAG_HAS_ID, /*id=*/0u,
      /*absolute_timestamp=*/10, /*tid_override=*/1, /*pid_override=*/0,
      /*duration=*/20);

  // Updating the duration of an invalid event should cause no further events to
  // be emitted.
  handle.event_index = 0;

  base::trace_event::TraceLog::GetInstance()->UpdateTraceEventDurationExplicit(
      category_group_enabled, kEventName, handle,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(30),
      base::ThreadTicks() + base::TimeDelta::FromMicroseconds(50),
      base::trace_event::ThreadInstructionCount());

  // No further packets should have been emitted.
  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
}

// TODO(eseckler): Add a test with multiple events + same strings (cat, name,
// arg names).

// TODO(eseckler): Add a test with multiple events + same strings with reset.

TEST_F(TraceEventDataSourceTest, InternedStrings) {
  CreateTraceEventDataSource();

  for (size_t i = 0; i < 2; i++) {
    TRACE_EVENT_INSTANT1("cat1", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 4);
    TRACE_EVENT_INSTANT1("cat1", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 2);
    TRACE_EVENT_INSTANT1("cat2", "e2", TRACE_EVENT_SCOPE_THREAD, "arg2", 1);

    EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 4u * (i + 1));

    auto* td_packet = producer_client()->GetFinalizedPacket(4 * i);
    ExpectThreadDescriptor(td_packet);

    // First packet needs to emit new interning entries
    auto* e_packet1 = producer_client()->GetFinalizedPacket(1 + (4 * i));
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
    auto* e_packet2 = producer_client()->GetFinalizedPacket(2 + (4 * i));
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
    auto* e_packet3 = producer_client()->GetFinalizedPacket(3 + (4 * i));
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 3u);

  auto* pd_packet = producer_client()->GetFinalizedPacket(0);
  ExpectProcessDescriptor(pd_packet);

  auto* td_packet = producer_client()->GetFinalizedPacket(1);
  ExpectThreadDescriptor(td_packet, 1u, 1u, /*filtering_enabled=*/true);

  auto* e_packet = producer_client()->GetFinalizedPacket(2);
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 3u);
  auto* e_packet = producer_client()->GetFinalizedPacket(2);
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 4u);
  auto* e_packet = producer_client()->GetFinalizedPacket(2);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 0);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "PRIVACY_FILTERED"}});
  ExpectDebugAnnotationNames(e_packet, {});

  e_packet = producer_client()->GetFinalizedPacket(3);
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

  auto metadata = producer_client()->GetChromeMetadata();
  EXPECT_EQ(0, metadata.size());
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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 4u);

  auto* td_packet = producer_client()->GetFinalizedPacket(0);
  ExpectThreadDescriptor(td_packet);

  // First packet needs to emit new interning entries
  auto* e_packet1 = producer_client()->GetFinalizedPacket(1);
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
  auto* e_packet2 = producer_client()->GetFinalizedPacket(2);
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
  auto* e_packet3 = producer_client()->GetFinalizedPacket(3);
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

  // Start startup tracing registry with no timeout. This would cause startup
  // tracing to abort and flush as soon the current thread can run tasks.
  data_source->set_startup_tracing_timeout_for_testing(base::TimeDelta());
  data_source->SetupStartupTracing(true);
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig(),
      base::trace_event::TraceLog::RECORDING_MODE);

  // The trace event will be added to the startup registry since the abort is
  // not run yet.
  TRACE_EVENT_BEGIN0(kCategoryGroup, kStartupTestEvent1);

  // Run task on background thread to add trace events while aborting and
  // starting tracing on the data source. This is to test we do not have any
  // crashes when a background thread is trying to create trace writers when
  // deleting startup registry and setting the producer.
  auto wait_for_start_tracing = std::make_unique<base::WaitableEvent>();
  base::WaitableEvent* wait_ptr = wait_for_start_tracing.get();
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](std::unique_ptr<base::WaitableEvent> wait_for_start_tracing) {
            // This event can be hit anytime before startup registry is
            // destroyed to tracing started using producer.
            TRACE_EVENT_BEGIN0(kCategoryGroup, "maybe_lost");
            base::ScopedAllowBaseSyncPrimitivesForTesting allow;
            wait_for_start_tracing->Wait();
            // This event can be hit while flushing for startup registry or when
            // tracing is started or when already stopped tracing.
            TRACE_EVENT_BEGIN0(kCategoryGroup, "maybe_lost");
          },
          std::move(wait_for_start_tracing)));

  // Let tasks run on this thread, which should abort startup tracing and flush
  // since we have not added a producer to the data source.
  data_source->OnTaskSchedulerAvailable();
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
  data_source->StopTracing(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure) { quit_closure.Run(); },
      wait_for_stop.QuitClosure()));

  wait_for_stop.Run();
}

// TODO(eseckler): Add startup tracing unittests.

}  // namespace

}  // namespace tracing
