// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_TEST_UTILS_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/test/scoped_feature_list.h"
#include "base/tracing/perfetto_task_runner.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/protozero/root_message.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_null_delegate.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

// Test producer client for data source tests.
class TestProducerClient : public ProducerClient {
 public:
  using PacketVector =
      std::vector<std::unique_ptr<perfetto::protos::TracePacket>>;
  explicit TestProducerClient(std::unique_ptr<base::tracing::PerfettoTaskRunner>
                                  main_thread_task_runner,
                              bool log_only_main_thread = true);
  ~TestProducerClient() override;

  // ProducerClient implementation:
  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::BufferID target_buffer,
      perfetto::BufferExhaustedPolicy =
          perfetto::BufferExhaustedPolicy::kDefault) override;

  void FlushPacketIfPossible();

  perfetto::protos::pbzero::TracePacket* NewTracePacket();

  void FinishTracePacket();

  size_t GetFinalizedPacketCount();

  const perfetto::protos::TracePacket* GetFinalizedPacket(
      size_t packet_index = 0);

  const google::protobuf::RepeatedPtrField<perfetto::protos::ChromeMetadata>*
  GetChromeMetadata(size_t packet_index = 0);

  const perfetto::protos::ChromeMetadataPacket* GetProtoChromeMetadata(
      size_t packet_index = 0);

  const PacketVector& finalized_packets() {
    FlushPacketIfPossible();
    return finalized_packets_;
  }

  // Serialize given trace packets and write the raw trace to the given file.
  // Very handy for debugging when trace generated in a test needs to be
  // exported, to understand it further with other tools.
  // Sample usage : WriteTraceToFile("/tmp/trace.pb", finalized_packets());
  static void WriteTraceToFile(const base::FilePath::StringType& filename,
                               const PacketVector& packets);

  static std::string SerializePacketsAsTrace(
      const PacketVector& finalized_packets);

  std::string GetSerializedTrace() const;

  int empty_finalized_packets_count() const {
    return empty_finalized_packets_count_;
  }

  TestProducerClient(TestProducerClient&&) = delete;
  TestProducerClient& operator=(TestProducerClient&&) = delete;

 private:
  PacketVector finalized_packets_;
  // A count of finalized packets not added to |finalized_packets_| per being
  // empty.
  int empty_finalized_packets_count_ = 0;
  PacketVector legacy_metadata_packets_;
  PacketVector proto_metadata_packets_;
  std::optional<protozero::RootMessage<perfetto::protos::pbzero::TracePacket>>
      trace_packet_;
  protozero::ScatteredStreamWriterNullDelegate delegate_;
  protozero::ScatteredStreamWriter stream_;
  size_t trace_packet_written_start_ = 0;
  std::unique_ptr<base::tracing::PerfettoTaskRunner> main_thread_task_runner_;
  bool log_only_main_thread_;
};

class TestTraceWriter : public perfetto::TraceWriter {
 public:
  using TracePacketCallback = base::RepeatingCallback<void(
      std::unique_ptr<perfetto::protos::TracePacket>)>;
  explicit TestTraceWriter(TestProducerClient* producer_client);

  perfetto::TraceWriter::TracePacketHandle NewTracePacket() override;
  void FinishTracePacket() override;
  void Flush(std::function<void()> callback = {}) override {}
  perfetto::WriterID writer_id() const override;
  uint64_t written() const override;

  TestTraceWriter(TestTraceWriter&&) = delete;
  TestTraceWriter& operator=(TestTraceWriter&&) = delete;

 private:
  raw_ptr<TestProducerClient, AcrossTasksDanglingUntriaged> producer_client_;
};

// Wrapper class around TestProducerClient useful for testing a trace data
// source.
//
// Usage:
//  DataSourceTester tester(source);
//  tester.BeginTrace();
//  source.WriteTracePackets();
//  tester.EndTracing();
//
//  EXPECT_TRUE(tester.GetFinalizedPacket());
class DataSourceTester {
 public:
  explicit DataSourceTester(
      tracing::PerfettoTracedProcess::DataSourceBase* data_source);
  ~DataSourceTester();

  void BeginTrace(const base::trace_event::TraceConfig& trace_config = {});
  void EndTracing();
  size_t GetFinalizedPacketCount();
  const perfetto::protos::TracePacket* GetFinalizedPacket(
      size_t packet_index = 0);


 private:
  void OnTraceData(base::RepeatingClosure quit_closure,
                   const scoped_refptr<base::RefCountedString>& chunk,
                   bool has_more_events);

  base::test::ScopedFeatureList features_;
  std::vector<std::unique_ptr<perfetto::protos::TracePacket>>
      finalized_packets_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_TEST_UTILS_H_
