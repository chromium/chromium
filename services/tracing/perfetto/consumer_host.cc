// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/consumer_host.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_log.h"
#include "base/values.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/wait.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/public/cpp/trace_event_args_whitelist.h"
#include "third_party/perfetto/include/perfetto/ext/trace_processor/export_json.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/observable_events.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/slice.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_stats.h"
#include "third_party/perfetto/include/perfetto/trace_processor/basic_types.h"
#include "third_party/perfetto/include/perfetto/trace_processor/trace_processor_storage.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.pb.h"

namespace tracing {

namespace {

const int32_t kEnableTracingTimeoutSeconds = 10;

class JsonStringOutputWriter
    : public perfetto::trace_processor::json::OutputWriter {
 public:
  using FlushCallback =
      base::RepeatingCallback<void(std::string json, bool has_more)>;

  JsonStringOutputWriter(FlushCallback flush_callback)
      : flush_callback_(std::move(flush_callback)) {
    buffer_.reserve(kBufferReserveCapacity);
  }

  ~JsonStringOutputWriter() override {
    flush_callback_.Run(std::move(buffer_), false);
  }

  perfetto::trace_processor::util::Status AppendString(
      const std::string& string) override {
    buffer_ += string;
    if (buffer_.size() > kBufferLimitInBytes) {
      flush_callback_.Run(std::move(buffer_), true);
      // Reset the buffer_ after moving it above.
      buffer_.clear();
      buffer_.reserve(kBufferReserveCapacity);
    }
    return perfetto::trace_processor::util::OkStatus();
  }

 private:
  static constexpr size_t kBufferLimitInBytes = 100 * 1024;
  // Since we write each string before checking the limit, we'll always go
  // slightly over and hence we reserve some extra space to avoid most
  // reallocs.
  static constexpr size_t kBufferReserveCapacity = kBufferLimitInBytes * 5 / 4;

  FlushCallback flush_callback_;
  std::string buffer_;
};

}  // namespace

class ConsumerHost::StreamWriter {
 public:
  using Slices = std::vector<std::string>;

  static scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner() {
    return base::CreateSequencedTaskRunner({base::ThreadPool(),
                                            base::WithBaseSyncPrimitives(),
                                            base::TaskPriority::BEST_EFFORT});
  }

  StreamWriter(mojo::ScopedDataPipeProducerHandle stream,
               TracingSession::ReadBuffersCallback callback,
               base::OnceClosure disconnect_callback,
               scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
      : stream_(std::move(stream)),
        read_buffers_callback_(std::move(callback)),
        disconnect_callback_(std::move(disconnect_callback)),
        callback_task_runner_(callback_task_runner) {}

  void WriteToStream(std::unique_ptr<Slices> slices, bool has_more) {
    DCHECK(stream_.is_valid());
    for (const auto& slice : *slices) {
      uint32_t write_position = 0;

      while (write_position < slice.size()) {
        uint32_t write_bytes = slice.size() - write_position;

        MojoResult result =
            stream_->WriteData(slice.data() + write_position, &write_bytes,
                               MOJO_WRITE_DATA_FLAG_NONE);

        if (result == MOJO_RESULT_OK) {
          write_position += write_bytes;
          continue;
        }

        if (result == MOJO_RESULT_SHOULD_WAIT) {
          result = mojo::Wait(stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE);
        }

        if (result != MOJO_RESULT_OK) {
          if (!disconnect_callback_.is_null()) {
            callback_task_runner_->PostTask(FROM_HERE,
                                            std::move(disconnect_callback_));
          }
          return;
        }
      }
    }

    if (!has_more && !read_buffers_callback_.is_null()) {
      callback_task_runner_->PostTask(FROM_HERE,
                                      std::move(read_buffers_callback_));
    }
  }

 private:
  mojo::ScopedDataPipeProducerHandle stream_;
  TracingSession::ReadBuffersCallback read_buffers_callback_;
  base::OnceClosure disconnect_callback_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(StreamWriter);
};

ConsumerHost::TracingSession::TracingSession(
    ConsumerHost* host,
    mojo::PendingReceiver<mojom::TracingSessionHost> tracing_session_host,
    mojo::PendingRemote<mojom::TracingSessionClient> tracing_session_client,
    const perfetto::TraceConfig& trace_config,
    mojom::TracingClientPriority priority)
    : host_(host),
      tracing_session_client_(std::move(tracing_session_client)),
      receiver_(this, std::move(tracing_session_host)),
      tracing_priority_(priority) {
  host_->service()->RegisterTracingSession(this);

  tracing_session_client_.set_disconnect_handler(base::BindOnce(
      &ConsumerHost::DestructTracingSession, base::Unretained(host)));
  receiver_.set_disconnect_handler(base::BindOnce(
      &ConsumerHost::DestructTracingSession, base::Unretained(host)));

  privacy_filtering_enabled_ = false;
  for (const auto& data_source : trace_config.data_sources()) {
    if (data_source.config().chrome_config().privacy_filtering_enabled()) {
      privacy_filtering_enabled_ = true;
    }
  }
#if DCHECK_IS_ON()
  if (privacy_filtering_enabled_) {
    // If enabled, filtering must be enabled for all data sources.
    for (const auto& data_source : trace_config.data_sources()) {
      DCHECK(data_source.config().chrome_config().privacy_filtering_enabled());
    }
  }
#endif

  filtered_pids_.clear();
  for (const auto& ds_config : trace_config.data_sources()) {
    if (ds_config.config().name() == mojom::kTraceEventDataSourceName) {
      for (const auto& filter : ds_config.producer_name_filter()) {
        base::ProcessId pid;
        if (PerfettoService::ParsePidFromProducerName(filter, &pid)) {
          filtered_pids_.insert(pid);
        }
      }
      break;
    }
  }

  pending_enable_tracing_ack_pids_ = host_->service()->active_service_pids();
  base::EraseIf(*pending_enable_tracing_ack_pids_,
                [this](base::ProcessId pid) { return !IsExpectedPid(pid); });

  host_->consumer_endpoint()->EnableTracing(trace_config);
  MaybeSendEnableTracingAck();

  if (pending_enable_tracing_ack_pids_) {
    // We can't know for sure whether all processes we request to connect to the
    // tracing service will connect back, or if all the connected services will
    // ACK our EnableTracing request eventually, so we'll add a timeout for that
    // case.
    enable_tracing_ack_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kEnableTracingTimeoutSeconds),
        this, &ConsumerHost::TracingSession::OnEnableTracingTimeout);
  }
}

ConsumerHost::TracingSession::~TracingSession() {
  host_->service()->UnregisterTracingSession(this);
  if (host_->consumer_endpoint()) {
    host_->consumer_endpoint()->FreeBuffers();
  }
}

void ConsumerHost::TracingSession::OnPerfettoEvents(
    const perfetto::ObservableEvents& events) {
  if (!pending_enable_tracing_ack_pids_) {
    return;
  }

  for (const auto& state_change : events.instance_state_changes()) {
    if (state_change.state() !=
        perfetto::ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STARTED) {
      continue;
    }

    if (state_change.data_source_name() != mojom::kTraceEventDataSourceName) {
      continue;
    }

    // Attempt to parse the PID out of the producer name.
    base::ProcessId pid;
    if (!PerfettoService::ParsePidFromProducerName(state_change.producer_name(),
                                                   &pid)) {
      continue;
    }

    pending_enable_tracing_ack_pids_->erase(pid);
  }
  MaybeSendEnableTracingAck();
}

void ConsumerHost::TracingSession::OnActiveServicePidAdded(
    base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_enable_tracing_ack_pids_ && IsExpectedPid(pid)) {
    pending_enable_tracing_ack_pids_->insert(pid);
  }
}

void ConsumerHost::TracingSession::OnActiveServicePidRemoved(
    base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_enable_tracing_ack_pids_) {
    pending_enable_tracing_ack_pids_->erase(pid);
    MaybeSendEnableTracingAck();
  }
}

void ConsumerHost::TracingSession::OnActiveServicePidsInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeSendEnableTracingAck();
}

void ConsumerHost::TracingSession::RequestDisableTracing(
    base::OnceClosure on_disabled_callback) {
  DCHECK(!on_disabled_callback_);
  on_disabled_callback_ = std::move(on_disabled_callback);
  DisableTracing();
}

void ConsumerHost::TracingSession::OnEnableTracingTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_enable_tracing_ack_pids_) {
    return;
  }

  std::stringstream error;
  error << "Timed out waiting for processes to ack BeginTracing: ";
  for (auto pid : *pending_enable_tracing_ack_pids_) {
    error << pid << " ";
  }
  LOG(ERROR) << error.rdbuf();

  DCHECK(tracing_session_client_);
  tracing_session_client_->OnTracingEnabled();
  pending_enable_tracing_ack_pids_.reset();
}

void ConsumerHost::TracingSession::MaybeSendEnableTracingAck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_enable_tracing_ack_pids_ ||
      !pending_enable_tracing_ack_pids_->empty() ||
      !host_->service()->active_service_pids_initialized()) {
    return;
  }

  DCHECK(tracing_session_client_);
  tracing_session_client_->OnTracingEnabled();
  pending_enable_tracing_ack_pids_.reset();
  enable_tracing_ack_timer_.Stop();
}

bool ConsumerHost::TracingSession::IsExpectedPid(base::ProcessId pid) const {
  return filtered_pids_.empty() || base::Contains(filtered_pids_, pid);
}

void ConsumerHost::TracingSession::ChangeTraceConfig(
    const perfetto::TraceConfig& trace_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  host_->consumer_endpoint()->ChangeTraceConfig(trace_config);
}

void ConsumerHost::TracingSession::DisableTracing() {
  host_->consumer_endpoint()->DisableTracing();
}

void ConsumerHost::TracingSession::OnTracingDisabled() {
  DCHECK(tracing_session_client_);

  if (enable_tracing_ack_timer_.IsRunning()) {
    enable_tracing_ack_timer_.FireNow();
  }
  DCHECK(!pending_enable_tracing_ack_pids_);

  tracing_session_client_->OnTracingDisabled();

  if (trace_processor_) {
    host_->consumer_endpoint()->ReadBuffers();
  }

  tracing_enabled_ = false;

  if (on_disabled_callback_) {
    std::move(on_disabled_callback_).Run();
  }
}

void ConsumerHost::TracingSession::OnConsumerClientDisconnected() {
  // The TracingSession will be deleted after this point.
  host_->DestructTracingSession();
}

void ConsumerHost::TracingSession::ReadBuffers(
    mojo::ScopedDataPipeProducerHandle stream,
    ReadBuffersCallback callback) {
  read_buffers_stream_writer_ = base::SequenceBound<StreamWriter>(
      StreamWriter::CreateTaskRunner(), std::move(stream), std::move(callback),
      base::BindOnce(&TracingSession::OnConsumerClientDisconnected,
                     weak_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  host_->consumer_endpoint()->ReadBuffers();
}

void ConsumerHost::TracingSession::RequestBufferUsage(
    RequestBufferUsageCallback callback) {
  if (!request_buffer_usage_callback_.is_null()) {
    std::move(callback).Run(false, 0, false);
    return;
  }

  request_buffer_usage_callback_ = std::move(callback);
  host_->consumer_endpoint()->GetTraceStats();
}

void ConsumerHost::TracingSession::DisableTracingAndEmitJson(
    const std::string& agent_label_filter,
    mojo::ScopedDataPipeProducerHandle stream,
    bool privacy_filtering_enabled,
    DisableTracingAndEmitJsonCallback callback) {
  DCHECK(!read_buffers_stream_writer_);

  read_buffers_stream_writer_ = base::SequenceBound<StreamWriter>(
      StreamWriter::CreateTaskRunner(), std::move(stream), std::move(callback),
      base::BindOnce(&TracingSession::OnConsumerClientDisconnected,
                     weak_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  if (privacy_filtering_enabled) {
    // For filtering/whitelisting to be possible at JSON export time,
    // filtering must not have been enabled during proto emission time
    // (or there's nothing to pass through the whitelist).
    DCHECK(!privacy_filtering_enabled_);
    privacy_filtering_enabled_ = true;
  }

  json_agent_label_filter_ = agent_label_filter;

  trace_processor_ =
      perfetto::trace_processor::TraceProcessorStorage::CreateInstance(
          perfetto::trace_processor::Config());

  DisableTracing();
}

void ConsumerHost::TracingSession::ExportJson() {
  // In legacy backend, the trace event agent sets the predicate used by
  // TraceLog. For perfetto backend, ensure that predicate is always set
  // before creating the exporter. The agent can be created later than this
  // point.
  if (base::trace_event::TraceLog::GetInstance()
          ->GetArgumentFilterPredicate()
          .is_null()) {
    base::trace_event::TraceLog::GetInstance()->SetArgumentFilterPredicate(
        base::BindRepeating(&IsTraceEventArgsWhitelisted));
    base::trace_event::TraceLog::GetInstance()->SetMetadataFilterPredicate(
        base::BindRepeating(&IsMetadataWhitelisted));
  }

  perfetto::trace_processor::json::ArgumentFilterPredicate argument_filter;
  perfetto::trace_processor::json::MetadataFilterPredicate metadata_filter;
  perfetto::trace_processor::json::LabelFilterPredicate label_filter;

  if (privacy_filtering_enabled_) {
    auto* trace_log = base::trace_event::TraceLog::GetInstance();
    base::trace_event::ArgumentFilterPredicate argument_filter_predicate =
        trace_log->GetArgumentFilterPredicate();
    argument_filter =
        [argument_filter_predicate](
            const char* category_group_name, const char* event_name,
            perfetto::trace_processor::json::ArgumentNameFilterPredicate*
                name_filter) {
          base::trace_event::ArgumentNameFilterPredicate name_filter_predicate;
          bool result = argument_filter_predicate.Run(
              category_group_name, event_name, &name_filter_predicate);
          if (name_filter_predicate) {
            *name_filter = [name_filter_predicate](const char* arg_name) {
              return name_filter_predicate.Run(arg_name);
            };
          }
          return result;
        };
    base::trace_event::MetadataFilterPredicate metadata_filter_predicate =
        trace_log->GetMetadataFilterPredicate();
    metadata_filter = [metadata_filter_predicate](const char* metadata_name) {
      return metadata_filter_predicate.Run(metadata_name);
    };
  }

  if (!json_agent_label_filter_.empty()) {
    label_filter = [this](const char* label) {
      return strcmp(label, json_agent_label_filter_.c_str()) == 0;
    };
  }

  JsonStringOutputWriter output_writer(base::BindRepeating(
      &ConsumerHost::TracingSession::OnJSONTraceData, base::Unretained(this)));
  auto status = perfetto::trace_processor::json::ExportJson(
      trace_processor_.get(), &output_writer, argument_filter, metadata_filter,
      label_filter);
  DCHECK(status.ok()) << status.message();
}

void ConsumerHost::TracingSession::OnJSONTraceData(std::string json,
                                                   bool has_more) {
  auto slices = std::make_unique<StreamWriter::Slices>();
  slices->push_back(std::string());
  slices->back().swap(json);
  read_buffers_stream_writer_.Post(FROM_HERE, &StreamWriter::WriteToStream,
                                   std::move(slices), has_more);

  if (!has_more) {
    read_buffers_stream_writer_.Reset();
  }
}

void ConsumerHost::TracingSession::OnTraceData(
    std::vector<perfetto::TracePacket> packets,
    bool has_more) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (trace_processor_) {
    // Calculate space needed for trace chunk. Each packet has a preamble and
    // payload size.
    size_t max_size = packets.size() * perfetto::TracePacket::kMaxPreambleBytes;
    for (const auto& packet : packets) {
      max_size += packet.size();
    }

    // Copy packets into a trace file chunk.
    size_t position = 0;
    std::unique_ptr<uint8_t[]> data(new uint8_t[max_size]);
    for (perfetto::TracePacket& packet : packets) {
      char* preamble;
      size_t preamble_size;
      std::tie(preamble, preamble_size) = packet.GetProtoPreamble();
      DCHECK_LT(position + preamble_size, max_size);
      memcpy(&data[position], preamble, preamble_size);
      position += preamble_size;
      for (const perfetto::Slice& slice : packet.slices()) {
        DCHECK_LT(position + slice.size, max_size);
        memcpy(&data[position], slice.start, slice.size);
        position += slice.size;
      }
    }

    auto status = trace_processor_->Parse(std::move(data), position);
    // TODO(eseckler): There's no way to propagate this error at the moment - If
    // one occurs on production builds, we silently ignore it and will end up
    // producing an empty JSON result.
    DCHECK(status.ok()) << status.message();
    if (!has_more) {
      trace_processor_->NotifyEndOfFile();
      ExportJson();
      trace_processor_.reset();
    }
    return;
  }

  auto copy = std::make_unique<StreamWriter::Slices>();
  for (auto& packet : packets) {
    char* data;
    size_t size;
    std::tie(data, size) = packet.GetProtoPreamble();
    copy->emplace_back(data, size);
    auto& slices = packet.slices();
    for (auto& slice : slices) {
      copy->emplace_back(static_cast<const char*>(slice.start), slice.size);
    }
  }
  read_buffers_stream_writer_.Post(FROM_HERE, &StreamWriter::WriteToStream,
                                   std::move(copy), has_more);
  if (!has_more) {
    read_buffers_stream_writer_.Reset();
  }
}

void ConsumerHost::TracingSession::OnTraceStats(
    bool success,
    const perfetto::TraceStats& stats) {
  if (!request_buffer_usage_callback_) {
    return;
  }

  if (!success || stats.buffer_stats_size() != 1) {
    std::move(request_buffer_usage_callback_).Run(false, 0.0f, false);
    return;
  }

  const perfetto::TraceStats::BufferStats& buf_stats = stats.buffer_stats()[0];
  size_t bytes_in_buffer = buf_stats.bytes_written() - buf_stats.bytes_read() -
                           buf_stats.bytes_overwritten() +
                           buf_stats.padding_bytes_written() -
                           buf_stats.padding_bytes_cleared();
  double percent_full =
      bytes_in_buffer / static_cast<double>(buf_stats.buffer_size());
  percent_full = base::ClampToRange(percent_full, 0.0, 1.0);
  bool data_loss = buf_stats.chunks_overwritten() > 0 ||
                   buf_stats.chunks_discarded() > 0 ||
                   buf_stats.abi_violations() > 0 ||
                   buf_stats.trace_writer_packet_loss() > 0;
  std::move(request_buffer_usage_callback_).Run(true, percent_full, data_loss);
}

void ConsumerHost::TracingSession::Flush(
    uint32_t timeout,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  flush_callback_ = std::move(callback);
  base::WeakPtr<TracingSession> weak_this = weak_factory_.GetWeakPtr();
  host_->consumer_endpoint()->Flush(timeout, [weak_this](bool success) {
    if (!weak_this) {
      return;
    }

    if (weak_this->flush_callback_) {
      std::move(weak_this->flush_callback_).Run(success);
    }
  });
}

// static
void ConsumerHost::BindConsumerReceiver(
    PerfettoService* service,
    mojo::PendingReceiver<mojom::ConsumerHost> receiver,
    const service_manager::BindSourceInfo& source_info) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<ConsumerHost>(service),
                              std::move(receiver));
}

ConsumerHost::ConsumerHost(PerfettoService* service) : service_(service) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  consumer_endpoint_ =
      service_->GetService()->ConnectConsumer(this, 0 /*uid_t*/);
  consumer_endpoint_->ObserveEvents(
      perfetto::TracingService::ConsumerEndpoint::ObservableEventType::
          kDataSourceInstances);
}

ConsumerHost::~ConsumerHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Make sure the tracing_session is destroyed first, as it keeps a pointer to
  // the ConsumerHost parent and accesses it on destruction.
  tracing_session_.reset();
}

void ConsumerHost::EnableTracing(
    mojo::PendingReceiver<mojom::TracingSessionHost> tracing_session_host,
    mojo::PendingRemote<mojom::TracingSessionClient> tracing_session_client,
    const perfetto::TraceConfig& trace_config,
    mojom::TracingClientPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!tracing_session_);

  // We create our new TracingSession async, if the PerfettoService allows
  // us to, after it's stopped any currently running lower or equal priority
  // tracing sessions.
  service_->RequestTracingSession(
      priority, base::BindOnce(
                    [](base::WeakPtr<ConsumerHost> weak_this,
                       mojo::PendingReceiver<mojom::TracingSessionHost>
                           tracing_session_host,
                       mojo::PendingRemote<mojom::TracingSessionClient>
                           tracing_session_client,
                       const perfetto::TraceConfig& trace_config,
                       mojom::TracingClientPriority priority) {
                      if (!weak_this) {
                        return;
                      }

                      weak_this->tracing_session_ =
                          std::make_unique<TracingSession>(
                              weak_this.get(), std::move(tracing_session_host),
                              std::move(tracing_session_client), trace_config,
                              priority);
                    },
                    weak_factory_.GetWeakPtr(), std::move(tracing_session_host),
                    std::move(tracing_session_client), trace_config, priority));
}

void ConsumerHost::OnConnect() {}

void ConsumerHost::OnDisconnect() {}

void ConsumerHost::OnTracingDisabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tracing_session_) {
    tracing_session_->OnTracingDisabled();
  }
}

void ConsumerHost::OnTraceData(std::vector<perfetto::TracePacket> packets,
                               bool has_more) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tracing_session_) {
    tracing_session_->OnTraceData(std::move(packets), has_more);
  }
}

void ConsumerHost::OnObservableEvents(
    const perfetto::ObservableEvents& events) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tracing_session_) {
    tracing_session_->OnPerfettoEvents(events);
  }
}

void ConsumerHost::OnTraceStats(bool success,
                                const perfetto::TraceStats& stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tracing_session_) {
    tracing_session_->OnTraceStats(success, stats);
  }
}

void ConsumerHost::DestructTracingSession() {
  tracing_session_.reset();
}

}  // namespace tracing
