// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_session.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "services/tracing/public/cpp/perfetto/trace_packet_tokenizer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/protos/perfetto/common/trace_stats.gen.h"

namespace tracing {

double GetTraceBufferUsage(const perfetto::protos::gen::TraceStats& stats) {
  uint64_t total_bytes_written = 0;
  uint64_t total_buffer_size = 0;

  // Sum the stats from all available buffers.
  for (const auto& buf_stats : stats.buffer_stats()) {
    total_bytes_written += buf_stats.bytes_written() - buf_stats.bytes_read() -
                           buf_stats.bytes_overwritten() +
                           buf_stats.padding_bytes_written() -
                           buf_stats.padding_bytes_cleared();
    total_buffer_size += buf_stats.buffer_size();
  }

  // Prevent division by zero if no buffers are configured.
  if (total_buffer_size == 0) {
    return 0.0;
  }

  // Return the calculated usage percentage.
  return static_cast<double>(total_bytes_written) / total_buffer_size;
}

bool HasLostData(const perfetto::protos::gen::TraceStats& stats) {
  bool dataLost = false;
  for (const auto& buf_stats : stats.buffer_stats()) {
    dataLost |= buf_stats.chunks_overwritten() > 0 ||
                buf_stats.chunks_discarded() > 0 ||
                buf_stats.abi_violations() > 0 ||
                buf_stats.trace_writer_packet_loss() > 0;
  }
  return dataLost;
}

void ReadTraceStats(
    const perfetto::TracingSession::GetTraceStatsCallbackArgs& args,
    base::OnceCallback<void(bool success, float percent_full, bool data_loss)>
        on_stats_callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  float percent_full = 0;
  bool data_lost = false;
  perfetto::TraceStats trace_stats;

  if (args.success &&
      trace_stats.ParseFromArray(args.trace_stats_data.data(),
                                 args.trace_stats_data.size())) {
    percent_full = GetTraceBufferUsage(trace_stats);
    data_lost = HasLostData(trace_stats);
  }

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_stats_callback), args.success,
                                percent_full, data_lost));
}

void ReadTraceAsJson(
    const perfetto::TracingSession::ReadTraceCallbackArgs& args,
    const scoped_refptr<
        base::RefCountedData<std::unique_ptr<TracePacketTokenizer>>>& tokenizer,
    base::OnceCallback<void(std::unique_ptr<std::string>)> on_data_callback,
    base::OnceClosure on_data_complete_callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  if (args.size) {
    std::vector<perfetto::TracePacket> packets =
        tokenizer->data->Parse(UNSAFE_TODO(base::span(
            reinterpret_cast<const uint8_t*>(args.data), args.size)));
    size_t total_size = 0;
    for (const auto& packet : packets) {
      for (const auto& slice : packet.slices()) {
        total_size += slice.size;
      }
    }
    if (total_size > 0) {
      auto data_string = std::make_unique<std::string>();
      data_string->reserve(total_size);
      for (const auto& packet : packets) {
        for (const auto& slice : packet.slices()) {
          data_string->append(reinterpret_cast<const char*>(slice.start),
                              slice.size);
        }
      }
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(on_data_callback), std::move(data_string)));
    }
  }
  if (!args.has_more)
    task_runner->PostTask(FROM_HERE, std::move(on_data_complete_callback));
}

void ReadTraceAsProtobuf(
    const perfetto::TracingSession::ReadTraceCallbackArgs& args,
    base::OnceCallback<void(std::unique_ptr<std::string>)> on_data_callback,
    base::OnceClosure on_data_complete_callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  if (args.size) {
    auto data_string = std::make_unique<std::string>(args.data, args.size);
    task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(on_data_callback),
                                                    std::move(data_string)));
  }
  if (!args.has_more)
    task_runner->PostTask(FROM_HERE, std::move(on_data_complete_callback));
}

}  // namespace tracing
