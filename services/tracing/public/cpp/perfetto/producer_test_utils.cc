// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/producer_test_utils.h"

#include <deque>
#include <functional>
#include <optional>
#include <utility>

#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/base/utils.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/protozero/root_message.h"

namespace tracing {

namespace {

constexpr size_t kChunkSize = 4096;

// For sequences/threads other than our own, we just want to ignore
// any events coming in.
class DummyTraceWriter : public perfetto::TraceWriter {
 public:
  DummyTraceWriter()
      : delegate_(kChunkSize), stream_(&delegate_) {}

  perfetto::TraceWriter::TracePacketHandle NewTracePacket() override {
    stream_.Reset(delegate_.GetNewBuffer());
    trace_packet_.Reset(&stream_);

    return perfetto::TraceWriter::TracePacketHandle(&trace_packet_);
  }

  void FinishTracePacket() override { trace_packet_.Finalize(); }

  void Flush(std::function<void()> callback = {}) override {}

  perfetto::WriterID writer_id() const override {
    return perfetto::WriterID(0);
  }

  uint64_t written() const override { return 0u; }

 private:
  protozero::RootMessage<perfetto::protos::pbzero::TracePacket> trace_packet_;
  protozero::ScatteredStreamWriterNullDelegate delegate_;
  protozero::ScatteredStreamWriter stream_;
};

}  // namespace

TestProducerClient::TestProducerClient(
    std::unique_ptr<base::tracing::PerfettoTaskRunner> main_thread_task_runner,
    bool log_only_main_thread)
    : ProducerClient(main_thread_task_runner.get()),
      delegate_(kChunkSize),
      stream_(&delegate_),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      log_only_main_thread_(log_only_main_thread) {}

TestProducerClient::~TestProducerClient() = default;

std::unique_ptr<perfetto::TraceWriter> TestProducerClient::CreateTraceWriter(
    perfetto::BufferID target_buffer,
    perfetto::BufferExhaustedPolicy) {
  // We attempt to destroy TraceWriters on thread shutdown in
  // ThreadLocalStorage::Slot, by posting them to the ProducerClient taskrunner,
  // but there's no guarantee that this will succeed if that taskrunner is also
  // shut down.
  ANNOTATE_SCOPED_MEMORY_LEAK;
  if (!log_only_main_thread_ ||
      main_thread_task_runner_->GetOrCreateTaskRunner()
          ->RunsTasksInCurrentSequence()) {
    return std::make_unique<TestTraceWriter>(this);
  } else {
    return std::make_unique<DummyTraceWriter>();
  }
}

void TestProducerClient::FlushPacketIfPossible() {
  // GetNewBuffer() in ScatteredStreamWriterNullDelegate doesn't
  // actually return a new buffer, but rather lets us access the buffer
  // buffer already used by protozero to write the TracePacket into.
  protozero::ContiguousMemoryRange buffer = delegate_.GetNewBuffer();

  if (!trace_packet_)
    return;

  trace_packet_->Finalize();
  uint32_t message_size = stream_.written() - trace_packet_written_start_;
  EXPECT_GE(buffer.size(), message_size);

  auto proto = std::make_unique<perfetto::protos::TracePacket>();
  EXPECT_TRUE(proto->ParseFromArray(buffer.begin, message_size));
  if (proto->has_chrome_events() &&
      proto->chrome_events().metadata().size() > 0) {
    legacy_metadata_packets_.push_back(std::move(proto));
  } else if (proto->has_chrome_metadata()) {
    proto_metadata_packets_.push_back(std::move(proto));
  } else if (message_size > 0) {
    finalized_packets_.push_back(std::move(proto));
  } else {
    ++empty_finalized_packets_count_;
  }

  stream_.Reset(buffer);
  trace_packet_.reset();
}

perfetto::protos::pbzero::TracePacket* TestProducerClient::NewTracePacket() {
  FlushPacketIfPossible();
  trace_packet_.emplace();
  trace_packet_->Reset(&stream_);
  trace_packet_written_start_ = stream_.written();
  return &trace_packet_.value();
}

void TestProducerClient::FinishTracePacket() {
  FlushPacketIfPossible();
}

size_t TestProducerClient::GetFinalizedPacketCount() {
  FlushPacketIfPossible();
  return finalized_packets_.size();
}

const perfetto::protos::TracePacket* TestProducerClient::GetFinalizedPacket(
    size_t packet_index) {
  FlushPacketIfPossible();
  EXPECT_GT(finalized_packets_.size(), packet_index);
  return finalized_packets_[packet_index].get();
}

const google::protobuf::RepeatedPtrField<perfetto::protos::ChromeMetadata>*
TestProducerClient::GetChromeMetadata(size_t packet_index) {
  FlushPacketIfPossible();
  if (legacy_metadata_packets_.empty()) {
    return nullptr;
  }
  EXPECT_GT(legacy_metadata_packets_.size(), packet_index);

  const auto& event_bundle =
      legacy_metadata_packets_[packet_index]->chrome_events();
  return &event_bundle.metadata();
}

const perfetto::protos::ChromeMetadataPacket*
TestProducerClient::GetProtoChromeMetadata(size_t packet_index) {
  FlushPacketIfPossible();
  EXPECT_GT(proto_metadata_packets_.size(), packet_index);
  return &proto_metadata_packets_[packet_index]->chrome_metadata();
}

// static
void TestProducerClient::WriteTraceToFile(
    const base::FilePath::StringType& filename,
    const PacketVector& packets) {
  auto&& raw_trace = TestProducerClient::SerializePacketsAsTrace(packets);
  EXPECT_TRUE(base::WriteFile(base::FilePath(filename), raw_trace));
}

// static
std::string TestProducerClient::SerializePacketsAsTrace(
    const PacketVector& finalized_packets) {
  perfetto::protos::Trace trace;
  for (auto& packet : finalized_packets) {
    *trace.add_packet() = *packet;
  }
  std::string trace_bytes;
  trace.SerializeToString(&trace_bytes);
  return trace_bytes;
}

TestTraceWriter::TestTraceWriter(TestProducerClient* producer_client)
    : producer_client_(producer_client) {}

perfetto::TraceWriter::TracePacketHandle TestTraceWriter::NewTracePacket() {
  return perfetto::TraceWriter::TracePacketHandle(
      producer_client_->NewTracePacket());
}

void TestTraceWriter::FinishTracePacket() {
  producer_client_->FinishTracePacket();
}

perfetto::WriterID TestTraceWriter::writer_id() const {
  return perfetto::WriterID(0);
}

uint64_t TestTraceWriter::written() const {
  return 0u;
}

DataSourceTester::DataSourceTester(
    tracing::PerfettoTracedProcess::DataSourceBase* data_source)
{
  features_.InitAndDisableFeature(features::kEnablePerfettoSystemTracing);
}

DataSourceTester::~DataSourceTester() = default;

void DataSourceTester::BeginTrace(
    const base::trace_event::TraceConfig& trace_config) {
  auto* trace_log = base::trace_event::TraceLog::GetInstance();
  perfetto::TraceConfig perfetto_config(
      tracing::GetDefaultPerfettoConfig(trace_config));
  trace_log->SetEnabled(trace_config, perfetto_config);
  base::RunLoop().RunUntilIdle();
}

void DataSourceTester::EndTracing() {
  auto* trace_log = base::trace_event::TraceLog::GetInstance();
  base::RunLoop wait_for_end;
  trace_log->SetDisabled();
  trace_log->Flush(base::BindRepeating(&DataSourceTester::OnTraceData,
                                       base::Unretained(this),
                                       wait_for_end.QuitClosure()));
  wait_for_end.Run();
}

size_t DataSourceTester::GetFinalizedPacketCount() {
  return finalized_packets_.size();
}

const perfetto::protos::TracePacket* DataSourceTester::GetFinalizedPacket(
    size_t packet_index) {
  return finalized_packets_[packet_index].get();
}

void DataSourceTester::OnTraceData(
    base::RepeatingClosure quit_closure,
    const scoped_refptr<base::RefCountedString>& chunk,
    bool has_more_events) {
  perfetto::protos::Trace trace;
  auto chunk_data = base::span(*chunk);
  bool ok = trace.ParseFromArray(chunk_data.data(), chunk_data.size());
  DCHECK(ok);
  for (const auto& packet : trace.packet()) {
    // Filter out packets from the tracing service.
    if (packet.trusted_packet_sequence_id() == 1)
      continue;
    auto proto = std::make_unique<perfetto::protos::TracePacket>();
    *proto = packet;
    finalized_packets_.push_back(std::move(proto));
  }
  if (!has_more_events)
    std::move(quit_closure).Run();
}

}  // namespace tracing
