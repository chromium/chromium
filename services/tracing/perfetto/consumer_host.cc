// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/consumer_host.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_log.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/wait.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/privacy_filtering_check.h"
#include "services/tracing/public/cpp/perfetto/perfetto_session.h"
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
  static scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner() {
    return base::ThreadPool::CreateSequencedTaskRunner(
        {base::WithBaseSyncPrimitives(), base::TaskPriority::USER_VISIBLE});
  }

  StreamWriter(mojo::ScopedDataPipeProducerHandle stream,
               TracingSession::ReadBuffersCallback callback,
               base::OnceClosure disconnect_callback,
               scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
      : stream_(std::move(stream)),
        read_buffers_callback_(std::move(callback)),
        disconnect_callback_(std::move(disconnect_callback)),
        callback_task_runner_(callback_task_runner) {}

  void WriteToStream(std::string&& slice, bool has_more) {
    DCHECK(stream_.is_valid());

    base::span<const uint8_t> bytes = base::as_byte_span(slice);
    while (!bytes.empty()) {
      size_t actually_written_bytes = 0;
      MojoResult result = stream_->WriteData(bytes, MOJO_WRITE_DATA_FLAG_NONE,
                                             actually_written_bytes);

      if (result == MOJO_RESULT_OK) {
        bytes = bytes.subspan(actually_written_bytes);
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

    if (!has_more && !read_buffers_callback_.is_null()) {
      callback_task_runner_->PostTask(FROM_HERE,
                                      std::move(read_buffers_callback_));
    }
  }

  StreamWriter(const StreamWriter&) = delete;
  StreamWriter& operator=(const StreamWriter&) = delete;

 private:
  mojo::ScopedDataPipeProducerHandle stream_;
  TracingSession::ReadBuffersCallback read_buffers_callback_;
  base::OnceClosure disconnect_callback_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;
};

ConsumerHost::TracingSession::TracingSession(
    ConsumerHost* host,
    mojo::PendingReceiver<mojom::TracingSessionHost> tracing_session_host,
    mojo::PendingRemote<mojom::TracingSessionClient> tracing_session_client,
    bool privacy_filtering_enabled)
    : host_(host),
      tracing_session_client_(std::move(tracing_session_client)),
      receiver_(this, std::move(tracing_session_host)),
      privacy_filtering_enabled_(privacy_filtering_enabled) {
  host_->service()->RegisterTracingSession(this);

  tracing_session_client_.set_disconnect_handler(base::BindOnce(
      &ConsumerHost::DestructTracingSession, base::Unretained(host)));
  receiver_.set_disconnect_handler(base::BindOnce(
      &ConsumerHost::DestructTracingSession, base::Unretained(host)));
}

ConsumerHost::TracingSession::TracingSession(
    ConsumerHost* host,
    mojo::PendingReceiver<mojom::TracingSessionHost> tracing_session_host,
    mojo::PendingRemote<mojom::TracingSessionClient> tracing_session_client,
    const perfetto::TraceConfig& trace_config,
    perfetto::base::ScopedFile output_file)
    : TracingSession(host,
                     std::move(tracing_session_host),
                     std::move(tracing_session_client),
                     false) {
  privacy_filtering_enabled_ = false;
  for (const auto& data_source : trace_config.data_sources()) {
    if (data_source.config().chrome_config().privacy_filtering_enabled()) {
      privacy_filtering_enabled_ = true;
    }
    if (data_source.config().chrome_config().convert_to_legacy_json()) {
      convert_to_legacy_json_ = true;
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

  const std::string kDataSourceName = "track_event";

  filtered_pids_.clear();
  for (const auto& ds_config : trace_config.data_sources()) {
    if (ds_config.config().name() == kDataSourceName) {
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
  std::erase_if(*pending_enable_tracing_ack_pids_,
                [this](base::ProcessId pid) { return !IsExpectedPid(pid); });

  host_->consumer_endpoint()->EnableTracing(trace_config,
                                            std::move(output_file));
  MaybeSendEnableTracingAck();

  if (pending_enable_tracing_ack_pids_) {
    // We can't know for sure whether all processes we request to connect to the
    // tracing service will connect back, or if all the connected services will
    // ACK our EnableTracing request eventually, so we'll add a timeout for that
    // case.
    enable_tracing_ack_timer_.Start(
        FROM_HERE, base::Seconds(kEnableTracingTimeoutSeconds), this,
        &ConsumerHost::TracingSession::OnEnableTracingTimeout);
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
  if (!pending_enable_tracing_ack_pids_ ||
      !events.instance_state_changes_size()) {
    return;
  }

  for (const auto& state_change : events.instance_state_changes()) {
    DataSourceHandle handle(state_change.producer_name(),
                            state_change.data_source_name());
    data_source_states_[handle] =
        state_change.state() ==
        perfetto::ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STARTED;
  }

  // Data sources are first reported as being stopped before starting, so once
  // all the data sources we know about have started we can declare tracing
  // begun.
  bool all_data_sources_started = std::ranges::all_of(
      data_source_states_,
      [](std::pair<DataSourceHandle, bool> state) { return state.second; });
  if (!all_data_sources_started)
    return;

  for (const auto& it : data_source_states_) {
    // Attempt to parse the PID out of the producer name.
    base::ProcessId pid;
    if (!PerfettoService::ParsePidFromProducerName(it.first.producer_name(),
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
  return filtered_pids_.empty() || filtered_pids_.contains(pid);
}

void ConsumerHost::TracingSession::ChangeTraceConfig(
    const perfetto::TraceConfig& trace_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  host_->consumer_endpoint()->ChangeTraceConfig(trace_config);
}

void ConsumerHost::TracingSession::DisableTracing() {
  host_->consumer_endpoint()->DisableTracing();
}

void ConsumerHost::TracingSession::CloneSession(
    const base::UnguessableToken& uuid,
    CloneSessionCallback callback) {
  // Multiple concurrent cloning isn't supported.
  DCHECK(!on_session_cloned_callback_);
  on_session_cloned_callback_ = std::move(callback);
  perfetto::ConsumerEndpoint::CloneSessionArgs args;
  args.unique_session_name = uuid.ToString();
  host_->consumer_endpoint()->CloneSession(std::move(args));
}

void ConsumerHost::TracingSession::OnTracingDisabled(const std::string& error) {
  DCHECK(tracing_session_client_);

  if (enable_tracing_ack_timer_.IsRunning()) {
    enable_tracing_ack_timer_.FireNow();
  }
  DCHECK(!pending_enable_tracing_ack_pids_);

  tracing_session_client_->OnTracingDisabled(
      /*tracing_succeeded=*/error.empty());

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
  DCHECK(!convert_to_legacy_json_);
  read_buffers_stream_writer_ = base::SequenceBound<StreamWriter>(
      StreamWriter::CreateTaskRunner(), std::move(stream), std::move(callback),
      base::BindOnce(&TracingSession::OnConsumerClientDisconnected,
                     weak_factory_.GetWeakPtr()),
      base::SequencedTaskRunner::GetCurrentDefault());

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
    DisableTracingAndEmitJsonCallback callback) {
  DCHECK(!read_buffers_stream_writer_);

  read_buffers_stream_writer_ = base::SequenceBound<StreamWriter>(
      StreamWriter::CreateTaskRunner(), std::move(stream), std::move(callback),
      base::BindOnce(&TracingSession::OnConsumerClientDisconnected,
                     weak_factory_.GetWeakPtr()),
      base::SequencedTaskRunner::GetCurrentDefault());

  json_agent_label_filter_ = agent_label_filter;

  perfetto::trace_processor::Config processor_config;
  trace_processor_ =
      perfetto::trace_processor::TraceProcessorStorage::CreateInstance(
          processor_config);

  if (tracing_enabled_) {
    DisableTracing();
  } else {
    host_->consumer_endpoint()->ReadBuffers();
  }
}

void ConsumerHost::TracingSession::ExportJson() {
  perfetto::trace_processor::json::LabelFilterPredicate label_filter;

  if (!json_agent_label_filter_.empty()) {
    label_filter = [this](const char* label) {
      return UNSAFE_TODO(strcmp(label, json_agent_label_filter_.c_str())) == 0;
    };
  }

  JsonStringOutputWriter output_writer(base::BindRepeating(
      &ConsumerHost::TracingSession::OnJSONTraceData, base::Unretained(this)));
  auto status = perfetto::trace_processor::json::ExportJson(
      trace_processor_.get(), &output_writer, nullptr, nullptr, label_filter);
  DCHECK(status.ok()) << status.message();
}

void ConsumerHost::TracingSession::OnJSONTraceData(std::string json,
                                                   bool has_more) {
  read_buffers_stream_writer_.AsyncCall(&StreamWriter::WriteToStream)
      .WithArgs(std::move(json), has_more);

  if (!has_more) {
    read_buffers_stream_writer_.Reset();
  }
}

void ConsumerHost::TracingSession::OnTraceData(
    std::vector<perfetto::TracePacket> packets,
    bool has_more) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Calculate space needed for trace chunk. Each packet has a preamble and
  // payload size.
  size_t max_size = packets.size() * perfetto::TracePacket::kMaxPreambleBytes;
  for (const auto& packet : packets) {
    max_size += packet.size();
  }

  // Copy packets into a trace slice.
  std::string chunk;
  chunk.reserve(max_size);
  // Copy packets into a trace file chunk.
  for (auto& packet : packets) {
    auto [data, size] = packet.GetProtoPreamble();
    chunk.append(data, size);
    auto& slices = packet.slices();
    for (auto& slice : slices) {
      chunk.append(static_cast<const char*>(slice.start), slice.size);
    }
  }

  if (privacy_filtering_enabled_) {
    tracing::PrivacyFilteringCheck::RemoveBlockedFields(chunk);
  }

  // If |trace_processor_| was initialized, then export trace as JSON.
  if (trace_processor_) {
    auto status =
        trace_processor_->Parse(perfetto::trace_processor::TraceBlobView(
            perfetto::trace_processor::TraceBlob::CopyFrom(
                reinterpret_cast<uint8_t*>(chunk.data()), chunk.size())));
    // TODO(eseckler): There's no way to propagate this error at the moment - If
    // one occurs on production builds, we silently ignore it and will end up
    // producing an empty JSON result.
    DCHECK(status.ok()) << status.message();
    if (!has_more) {
      status = trace_processor_->NotifyEndOfFile();
      DCHECK(status.ok()) << status.message();
      ExportJson();
      trace_processor_.reset();
    }
    return;
  }

  read_buffers_stream_writer_.AsyncCall(&StreamWriter::WriteToStream)
      .WithArgs(std::move(chunk), has_more);
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

  if (!(success && stats.buffer_stats_size())) {
    std::move(request_buffer_usage_callback_).Run(false, 0.0f, false);
    return;
  }
  double percent_full = GetTraceBufferUsage(stats);
  bool data_loss = HasLostData(stats);
  std::move(request_buffer_usage_callback_).Run(true, percent_full, data_loss);
}

void ConsumerHost::TracingSession::OnSessionCloned(
    const OnSessionClonedArgs& args) {
  if (!on_session_cloned_callback_) {
    return;
  }
  std::move(on_session_cloned_callback_)
      .Run(args.success, args.error,
           base::Token(args.uuid.lsb(), args.uuid.msb()));
}

void ConsumerHost::TracingSession::Flush(
    uint32_t timeout,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  flush_callback_ = std::move(callback);
  base::WeakPtr<TracingSession> weak_this = weak_factory_.GetWeakPtr();
  host_->consumer_endpoint()->Flush(
      timeout,
      [weak_this](bool success) {
        if (!weak_this) {
          return;
        }

        if (weak_this->flush_callback_) {
          std::move(weak_this->flush_callback_).Run(success);
        }
      },
      perfetto::FlushFlags(0));
}

// static
void ConsumerHost::BindConsumerReceiver(
    PerfettoService* service,
    mojo::PendingReceiver<mojom::ConsumerHost> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<ConsumerHost>(service),
                              std::move(receiver));
}

ConsumerHost::ConsumerHost(PerfettoService* service) : service_(service) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  consumer_endpoint_ =
      service_->GetService()->ConnectConsumer(this, 0 /*uid_t*/);
  consumer_endpoint_->ObserveEvents(
      perfetto::ObservableEvents::TYPE_DATA_SOURCES_INSTANCES);
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
    base::File output_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!tracing_session_);

#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/40736989): Support writing to a file directly on Windows.
  DCHECK(!output_file.IsValid())
      << "Tracing directly to a file isn't supported yet on Windows";
  perfetto::base::ScopedFile file;
#else
  perfetto::base::ScopedFile file(output_file.TakePlatformFile());
#endif

  tracing_session_ = std::make_unique<TracingSession>(
      this, std::move(tracing_session_host), std::move(tracing_session_client),
      trace_config, std::move(file));
}

void ConsumerHost::CloneSession(
    mojo::PendingReceiver<mojom::TracingSessionHost> tracing_session_host,
    mojo::PendingRemote<mojom::TracingSessionClient> tracing_session_client,
    const base::UnguessableToken& uuid,
    bool privacy_filtering_enabled,
    CloneSessionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!tracing_session_);

  tracing_session_ = std::make_unique<TracingSession>(
      this, std::move(tracing_session_host), std::move(tracing_session_client),
      privacy_filtering_enabled);
  tracing_session_->CloneSession(uuid, std::move(callback));
}

void ConsumerHost::OnConnect() {}

void ConsumerHost::OnDisconnect() {}

void ConsumerHost::OnTracingDisabled(const std::string& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tracing_session_) {
    tracing_session_->OnTracingDisabled(error);
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

void ConsumerHost::OnSessionCloned(const OnSessionClonedArgs& args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tracing_session_) {
    tracing_session_->OnSessionCloned(args);
  }
}

void ConsumerHost::DestructTracingSession() {
  tracing_session_.reset();
}

}  // namespace tracing
