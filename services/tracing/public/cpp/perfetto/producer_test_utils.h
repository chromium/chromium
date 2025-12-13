// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_TEST_UTILS_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted_memory.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/trace_test_utils.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/protozero/root_message.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_null_delegate.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

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
  base::test::TracingEnvironment tracing_environment_;
  std::vector<std::unique_ptr<perfetto::protos::TracePacket>>
      finalized_packets_;
};

// For sequences/threads other than our own, we just want to ignore
// any events coming in.
class DummyTraceWriter : public perfetto::TraceWriter {
 public:
  static constexpr size_t kChunkSize = 4096;

  DummyTraceWriter();
  ~DummyTraceWriter() override;

  perfetto::TraceWriter::TracePacketHandle NewTracePacket() override;
  void FinishTracePacket() override;
  void Flush(std::function<void()> callback) override;
  perfetto::WriterID writer_id() const override;
  uint64_t written() const override;
  uint64_t drop_count() const override;

 private:
  protozero::RootMessage<perfetto::protos::pbzero::TracePacket> trace_packet_;
  protozero::ScatteredStreamWriterNullDelegate delegate_;
  protozero::ScatteredStreamWriter stream_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_TEST_UTILS_H_
