// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/producer_test_utils.h"

#include <deque>
#include <functional>
#include <optional>
#include <utility>

#include "base/debug/leak_annotations.h"
#include "base/run_loop.h"
#include "base/trace_event/trace_log.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/base/utils.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/protozero/root_message.h"

namespace tracing {

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

DummyTraceWriter::DummyTraceWriter()
    : delegate_(kChunkSize), stream_(&delegate_) {}

DummyTraceWriter::~DummyTraceWriter() = default;

perfetto::TraceWriter::TracePacketHandle DummyTraceWriter::NewTracePacket() {
  stream_.Reset(delegate_.GetNewBuffer());
  trace_packet_.Reset(&stream_);

  return perfetto::TraceWriter::TracePacketHandle(&trace_packet_);
}

void DummyTraceWriter::FinishTracePacket() {
  trace_packet_.Finalize();
}

void DummyTraceWriter::Flush(std::function<void()> callback) {}

perfetto::WriterID DummyTraceWriter::writer_id() const {
  return perfetto::WriterID(0);
}

uint64_t DummyTraceWriter::written() const {
  return 0u;
}

uint64_t DummyTraceWriter::drop_count() const {
  return 0u;
}

}  // namespace tracing
