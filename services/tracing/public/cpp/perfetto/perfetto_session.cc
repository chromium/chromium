// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_session.h"

#include <map>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "services/tracing/public/cpp/perfetto/trace_packet_tokenizer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/protos/perfetto/common/data_source_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/common/trace_stats.gen.h"
#include "third_party/perfetto/protos/perfetto/common/tracing_service_state.gen.h"
#include "third_party/perfetto/protos/perfetto/common/track_event_descriptor.gen.h"

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

namespace {

std::vector<perfetto::protos::gen::TrackEventCategory>
GetTrackEventCategoriesFromData(base::span<const uint8_t> service_state_data) {
  perfetto::protos::gen::TracingServiceState service_state;
  if (!service_state.ParseFromArray(service_state_data.data(),
                                    service_state_data.size())) {
    return {};
  }
  std::map<std::string, perfetto::protos::gen::TrackEventCategory> category_map;
  for (const auto& ds : service_state.data_sources()) {
    if (ds.ds_descriptor().name() == "track_event" &&
        !ds.ds_descriptor().track_event_descriptor_raw().empty()) {
      perfetto::protos::gen::TrackEventDescriptor ted;
      if (ted.ParseFromString(
              ds.ds_descriptor().track_event_descriptor_raw())) {
        for (const auto& category : ted.available_categories()) {
          category_map.try_emplace(category.name(), category);
        }
      }
    }
  }
  std::vector<perfetto::protos::gen::TrackEventCategory> categories;
  categories.reserve(category_map.size());
  for (auto& [_, category] : category_map) {
    categories.push_back(std::move(category));
  }
  return categories;
}

}  // namespace

void QueryTrackEventCategories(
    std::unique_ptr<perfetto::TracingSession> session,
    base::OnceCallback<
        void(std::vector<perfetto::protos::gen::TrackEventCategory>)> callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  struct QueryContext : public base::RefCountedThreadSafe<QueryContext> {
    std::unique_ptr<perfetto::TracingSession> session;
    base::OnceCallback<void(
        std::vector<perfetto::protos::gen::TrackEventCategory>)>
        callback;

   private:
    friend class base::RefCountedThreadSafe<QueryContext>;
    ~QueryContext() = default;
  };
  auto context = base::MakeRefCounted<QueryContext>();
  context->session = std::move(session);
  context->callback = std::move(callback);
  auto* session_raw = context->session.get();
  session_raw->QueryServiceState(
      [task_runner,
       context](perfetto::TracingSession::QueryServiceStateCallbackArgs args) {
        std::vector<perfetto::protos::gen::TrackEventCategory> categories;
        if (args.success) {
          categories = GetTrackEventCategoriesFromData(args.service_state_data);
        }
        task_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(context->callback),
                                             std::move(categories)));
      });
}

}  // namespace tracing
