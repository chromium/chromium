// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_TEST_UTILS_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_TEST_UTILS_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/perfetto/task_runner.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/protozero/root_message.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_null_delegate.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

// Test producer client for data source tests.
class TestProducerClient : public ProducerClient {
 public:
  explicit TestProducerClient(
      std::unique_ptr<PerfettoTaskRunner> main_thread_task_runner,
      bool log_only_main_thread = true);
  ~TestProducerClient() override;

  // ProducerClient implementation:
  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::BufferID target_buffer,
      perfetto::BufferExhaustedPolicy =
          perfetto::BufferExhaustedPolicy::kDefault) override;

  void FlushPacketIfPossible();

  perfetto::protos::pbzero::TracePacket* NewTracePacket();

  size_t GetFinalizedPacketCount();

  const perfetto::protos::TracePacket* GetFinalizedPacket(
      size_t packet_index = 0);

  const google::protobuf::RepeatedPtrField<perfetto::protos::ChromeMetadata>*
  GetChromeMetadata(size_t packet_index = 0);

  const perfetto::protos::ChromeMetadataPacket* GetProtoChromeMetadata(
      size_t packet_index = 0);

  const std::vector<std::unique_ptr<perfetto::protos::TracePacket>>&
  finalized_packets() const {
    return finalized_packets_;
  }

  TestProducerClient(TestProducerClient&&) = delete;
  TestProducerClient& operator=(TestProducerClient&&) = delete;

 private:
  std::vector<std::unique_ptr<perfetto::protos::TracePacket>>
      finalized_packets_;
  std::vector<std::unique_ptr<perfetto::protos::TracePacket>>
      legacy_metadata_packets_;
  std::vector<std::unique_ptr<perfetto::protos::TracePacket>>
      proto_metadata_packets_;
  protozero::RootMessage<perfetto::protos::pbzero::TracePacket> trace_packet_;
  protozero::ScatteredStreamWriterNullDelegate delegate_;
  protozero::ScatteredStreamWriter stream_;
  std::unique_ptr<PerfettoTaskRunner> main_thread_task_runner_;
  bool log_only_main_thread_;
};

class TestTraceWriter : public perfetto::TraceWriter {
 public:
  using TracePacketCallback = base::RepeatingCallback<void(
      std::unique_ptr<perfetto::protos::TracePacket>)>;
  explicit TestTraceWriter(TestProducerClient* producer_client);

  perfetto::TraceWriter::TracePacketHandle NewTracePacket() override;
  void Flush(std::function<void()> callback = {}) override {}
  perfetto::WriterID writer_id() const override;
  uint64_t written() const override;

  TestTraceWriter(TestTraceWriter&&) = delete;
  TestTraceWriter& operator=(TestTraceWriter&&) = delete;

 private:
  TestProducerClient* producer_client_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_TEST_UTILS_H_
