// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/devtools/protocol/tracing_handler.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event_impl.h"
#include "base/trace_event/traced_value.h"
#include "base/trace_event/tracing_agent.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/browser/devtools/devtools_stream_file.h"
#include "content/browser/devtools/devtools_traceable_screenshot.h"
#include "content/browser/devtools/devtools_video_consumer.h"
#include "content/browser/devtools/tracing_process_set_monitor.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_service.h"
#include "content/public/browser/web_contents.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_session.h"
#include "services/tracing/public/cpp/perfetto/trace_packet_tokenizer.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/constants.mojom-forward.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/inspector_protocol/crdtp/json.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/compositor_impl_android.h"
#endif

namespace content::protocol {

namespace {

const double kMinimumReportingInterval = 250.0;

const char kRecordModeParam[] = "record_mode";
const char kTraceBufferSizeInKb[] = "trace_buffer_size_in_kb";

const char kTrackEventDataSourceName[] = "track_event";

// Frames need to be at least 1x1, otherwise nothing would be captured.
constexpr gfx::Size kMinFrameSize = gfx::Size(1, 1);

// Frames do not need to be greater than 500x500 for tracing.
constexpr gfx::Size kMaxFrameSize = gfx::Size(500, 500);

// Convert from camel case to separator + lowercase.
std::string ConvertFromCamelCase(const std::string& in_str, char separator) {
  std::string out_str;
  out_str.reserve(in_str.size());
  for (char c : in_str) {
    if (absl::ascii_isupper(static_cast<unsigned char>(c))) {
      out_str.push_back(separator);
      out_str.push_back(absl::ascii_tolower(static_cast<unsigned char>(c)));
    } else {
      out_str.push_back(c);
    }
  }
  return out_str;
}

base::Value ConvertDictKeyStyle(const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (dict) {
    base::Value::Dict out;
    for (auto kv : *dict) {
      out.Set(ConvertFromCamelCase(kv.first, '_'),
              ConvertDictKeyStyle(kv.second));
    }
    return base::Value(std::move(out));
  }

  const base::Value::List* list = value.GetIfList();
  if (list) {
    base::Value::List out;
    for (const auto& v : *list) {
      out.Append(ConvertDictKeyStyle(v));
    }
    return base::Value(std::move(out));
  }

  return value.Clone();
}

class DevToolsTraceEndpointProxy : public TracingController::TraceDataEndpoint {
 public:
  explicit DevToolsTraceEndpointProxy(base::WeakPtr<TracingHandler> handler)
      : tracing_handler_(handler) {}

  void ReceiveTraceChunk(std::unique_ptr<std::string> chunk) override {
    if (TracingHandler* h = tracing_handler_.get())
      h->OnTraceDataCollected(std::move(chunk));
  }

  void ReceivedTraceFinalContents() override {
    if (TracingHandler* h = tracing_handler_.get())
      h->OnTraceComplete();
  }

 private:
  ~DevToolsTraceEndpointProxy() override = default;

  base::WeakPtr<TracingHandler> tracing_handler_;
};

class DevToolsStreamEndpoint : public TracingController::TraceDataEndpoint {
 public:
  explicit DevToolsStreamEndpoint(
      base::WeakPtr<TracingHandler> handler,
      const scoped_refptr<DevToolsStreamFile>& stream)
      : stream_(stream), tracing_handler_(handler) {}

  // CompressedStringEndpoint calls these methods on a background thread.
  void ReceiveTraceChunk(std::unique_ptr<std::string> chunk) override {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&DevToolsStreamEndpoint::ReceiveTraceChunk,
                                    this, std::move(chunk)));
      return;
    }
    stream_->Append(std::move(chunk));
  }

  void ReceivedTraceFinalContents() override {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&DevToolsStreamEndpoint::ReceivedTraceFinalContents,
                         this));
      return;
    }
    if (TracingHandler* h = tracing_handler_.get())
      h->OnTraceToStreamComplete(stream_->handle());
  }

 private:
  ~DevToolsStreamEndpoint() override = default;

  scoped_refptr<DevToolsStreamFile> stream_;
  base::WeakPtr<TracingHandler> tracing_handler_;
};

std::string GetProcessHostHex(RenderProcessHost* host) {
  return base::StringPrintf("0x%" PRIxPTR, reinterpret_cast<uintptr_t>(host));
}

void SendProcessReadyInBrowserEvent(const base::UnguessableToken& frame_token,
                                    RenderProcessHost* host) {
  auto data = std::make_unique<base::trace_event::TracedValue>();
  data->SetString("frame", frame_token.ToString());
  data->SetString("processPseudoId", GetProcessHostHex(host));
  data->SetInteger("processId", static_cast<int>(host->GetProcess().Pid()));
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "ProcessReadyInBrowser", TRACE_EVENT_SCOPE_THREAD,
                       "data", std::move(data));
}

void FillFrameData(base::trace_event::TracedValue* data,
                   RenderFrameHostImpl* frame_host,
                   const GURL& url) {
  CHECK(frame_host);
  GURL::Replacements strip_fragment;
  strip_fragment.ClearRef();
  std::string trimmed_url = url.ReplaceComponents(strip_fragment).spec();
  data->SetString("frame", frame_host->devtools_frame_token().ToString());
  data->SetString("url", std::move(trimmed_url));
  data->SetString("name", frame_host->GetFrameName());
  data->SetBoolean("isOutermostMainFrame", frame_host->IsOutermostMainFrame());
  // Use FrameTree's primary status since the `frame_host` itself might not be
  // the primary main RenderFrameHost yet, if this function is called when
  // `frame_host` is still speculative / pending commit.
  data->SetBoolean("isInPrimaryMainFrame",
                   frame_host->IsOutermostMainFrame() &&
                       frame_host->frame_tree()->is_primary());
  if (frame_host->GetParent()) {
    data->SetString(
        "parent", frame_host->GetParent()->GetDevToolsFrameToken().ToString());
  }
  RenderProcessHost* process_host = frame_host->GetProcess();
  const base::Process& process_handle = process_host->GetProcess();
  if (!process_handle.IsValid()) {
    data->SetString("processPseudoId", GetProcessHostHex(process_host));
    frame_host->GetProcess()->PostTaskWhenProcessIsReady(
        base::BindOnce(&SendProcessReadyInBrowserEvent,
                       frame_host->devtools_frame_token(), process_host));
  } else {
    // Cast process id to int to be compatible with tracing.
    data->SetInteger("processId", static_cast<int>(process_handle.Pid()));
  }
}

std::optional<base::trace_event::MemoryDumpLevelOfDetail>
StringToMemoryDumpLevelOfDetail(const std::string& str) {
  if (str == Tracing::MemoryDumpLevelOfDetailEnum::Detailed)
    return {base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  if (str == Tracing::MemoryDumpLevelOfDetailEnum::Background)
    return {base::trace_event::MemoryDumpLevelOfDetail::kBackground};
  if (str == Tracing::MemoryDumpLevelOfDetailEnum::Light)
    return {base::trace_event::MemoryDumpLevelOfDetail::kLight};
  return {};
}

void AddPidsToProcessFilter(
    const std::unordered_set<base::ProcessId>& included_process_ids,
    perfetto::TraceConfig& trace_config) {
  const std::string kDataSourceName = kTrackEventDataSourceName;
  for (auto& data_source : *(trace_config.mutable_data_sources())) {
    auto* source_config = data_source.mutable_config();
    if (source_config->name() == kDataSourceName) {
      for (auto& enabled_pid : included_process_ids) {
        *data_source.add_producer_name_filter() = base::StrCat(
            {tracing::mojom::kPerfettoProducerNamePrefix,
             base::NumberToString(static_cast<uint32_t>(enabled_pid))});
      }
      break;
    }
  }
}

bool IsChromeDataSource(const std::string& data_source_name) {
  return base::StartsWith(data_source_name, "org.chromium.") ||
         data_source_name == "track_event";
}

std::optional<perfetto::BackendType> GetBackendTypeFromParameters(
    const std::string& tracing_backend,
    perfetto::TraceConfig& perfetto_config) {
  if (tracing_backend == Tracing::TracingBackendEnum::Chrome)
    return perfetto::BackendType::kCustomBackend;
  if (tracing_backend == Tracing::TracingBackendEnum::System)
    return perfetto::BackendType::kSystemBackend;
  if (tracing_backend == Tracing::TracingBackendEnum::Auto) {
    // Use the Chrome backend by default, unless there are non-Chrome data
    // sources specified in the config.
    for (auto& data_source : *(perfetto_config.mutable_data_sources())) {
      auto* source_config = data_source.mutable_config();
      if (!IsChromeDataSource(source_config->name()))
        return perfetto::BackendType::kSystemBackend;
    }
    return perfetto::BackendType::kCustomBackend;
  }
  return std::nullopt;
}

// Perfetto SDK build expects track_event data source to be configured via
// track_event_config. But some devtools users (e.g. Perfetto UI) send
// a chrome_config instead. We build a track_event_config based on the
// chrome_config if no other track_event data sources have been configured.
void ConvertToTrackEventConfigIfNeeded(perfetto::TraceConfig& trace_config) {
  for (const auto& data_source : trace_config.data_sources()) {
    if (!data_source.config().track_event_config_raw().empty()) {
      return;
    }
  }
  for (auto& data_source : *trace_config.mutable_data_sources()) {
    if (data_source.config().name() ==
            tracing::mojom::kTraceEventDataSourceName &&
        data_source.config().has_chrome_config()) {
      data_source.mutable_config()->set_name(kTrackEventDataSourceName);
      base::trace_event::TraceConfig base_config(
          data_source.config().chrome_config().trace_config());
      bool privacy_filtering_enabled =
          data_source.config().chrome_config().privacy_filtering_enabled();
      data_source.mutable_config()->set_track_event_config_raw(
          base_config.ToPerfettoTrackEventConfigRaw(privacy_filtering_enabled));
      return;
    }
  }
}

// We currently don't support concurrent tracing sessions, but are planning to.
// For the time being, we're using this flag as a workaround to prevent devtools
// users from accidentally starting two concurrent sessions.
// TODO(eseckler): Remove once we add support for concurrent sessions to the
// perfetto backend.
static bool g_any_agent_tracing = false;

}  // namespace

class TracingHandler::PerfettoTracingSession {
 public:
  PerfettoTracingSession(bool use_proto, perfetto::BackendType backend_type)
      : use_proto_format_(use_proto), backend_type_(backend_type) {}

  ~PerfettoTracingSession() { g_any_agent_tracing = false; }

  void EnableTracing(const perfetto::TraceConfig& perfetto_config,
                     base::OnceCallback<void(const std::string& /*error_msg*/)>
                         start_callback) {
    DCHECK(!tracing_session_);
    DCHECK(!tracing_active_);

    g_any_agent_tracing = true;
    tracing_active_ = true;
    start_callback_ = std::move(start_callback);

#if DCHECK_IS_ON()
    last_perfetto_config_ = perfetto_config;
    for (auto& data_source : *(last_perfetto_config_.mutable_data_sources())) {
      data_source.clear_producer_name_filter();
    }
#endif

    tracing_session_ = perfetto::Tracing::NewTrace(backend_type_);
    tracing_session_->Setup(perfetto_config);

    auto weak_ptr = weak_factory_.GetWeakPtr();

    tracing_session_->SetOnStartCallback([weak_ptr] {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&PerfettoTracingSession::OnTracingSessionStarted,
                         weak_ptr));
    });

    tracing_session_->SetOnErrorCallback(
        [weak_ptr](perfetto::TracingError error) {
          GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE,
              base::BindOnce(&PerfettoTracingSession::OnTracingSessionFailed,
                             weak_ptr, error.message));
        });

    tracing_session_->Start();
  }

  void AdoptStartupTracingSession(
      const perfetto::TraceConfig& perfetto_config) {
    // Start a perfetto tracing session, which will claim startup tracing data.
    DCHECK(!TracingController::GetInstance()->IsTracing());
    waiting_for_startup_tracing_enabled_ = true;
    EnableTracing(
        perfetto_config,
        base::BindOnce(&PerfettoTracingSession::OnStartupTracingEnabled,
                       base::Unretained(this)));
  }

  void ChangeTraceConfig(const perfetto::TraceConfig& perfetto_config) {
    if (!tracing_session_)
      return;

#if DCHECK_IS_ON()
    // Ensure that the process filter is the only thing that gets changed
    // in a configuration during a tracing session.
    perfetto::TraceConfig config_without_filters = perfetto_config;
    for (auto& data_source : *(config_without_filters.mutable_data_sources())) {
      data_source.clear_producer_name_filter();
    }
    DCHECK(config_without_filters == last_perfetto_config_);
    last_perfetto_config_ = std::move(config_without_filters);
#endif

    tracing_session_->ChangeTraceConfig(perfetto_config);
  }

  void DisableTracing(
      scoped_refptr<TracingController::TraceDataEndpoint> endpoint) {
    DCHECK(endpoint);
    if (waiting_for_startup_tracing_enabled_) {
      pending_disable_tracing_task_ =
          base::BindOnce(&PerfettoTracingSession::DisableTracing,
                         base::Unretained(this), std::move(endpoint));
      return;
    }

    endpoint_ = endpoint;
    tracing_active_ = false;

    if (!tracing_session_) {
      endpoint_->ReceivedTraceFinalContents();
      return;
    }

    auto weak_ptr = weak_factory_.GetWeakPtr();
    tracing_session_->SetOnStopCallback([weak_ptr] {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&PerfettoTracingSession::OnTracingSessionStopped,
                         weak_ptr));
    });

    tracing_session_->Stop();
  }

  void GetBufferUsage(base::OnceCallback<void(bool success,
                                              float percent_full,
                                              size_t approximate_event_count)>
                          on_buffer_usage_callback) {
    DCHECK(on_buffer_usage_callback);
    if (!tracing_session_) {
      std::move(on_buffer_usage_callback).Run(false, 0.0f, 0);
      return;
    }

    if (on_buffer_usage_callback_) {
      // We only support one concurrent buffer usage request.
      std::move(on_buffer_usage_callback).Run(false, 0.0f, 0);
      return;
    }

    on_buffer_usage_callback_ = std::move(on_buffer_usage_callback);
    auto weak_ptr = weak_factory_.GetWeakPtr();

    tracing_session_->GetTraceStats(
        [weak_ptr](perfetto::TracingSession::GetTraceStatsCallbackArgs args) {
          tracing::ReadTraceStats(
              args,
              base::BindOnce(&PerfettoTracingSession::OnBufferUsage, weak_ptr),
              GetUIThreadTaskRunner({}));
        });
  }

  bool HasTracingFailed() { return tracing_active_ && !tracing_session_; }

  bool HasDataLossOccurred() { return data_loss_; }

 private:
  void OnStartupTracingEnabled(const std::string& error_msg) {
    DCHECK(error_msg.empty());
    waiting_for_startup_tracing_enabled_ = false;
    if (pending_disable_tracing_task_)
      std::move(pending_disable_tracing_task_).Run();
  }

  void OnTracingSessionStarted() {
    if (start_callback_)
      std::move(start_callback_).Run(/*error_msg=*/std::string());
  }

  void OnTracingSessionFailed(std::string error_msg) {
    tracing_session_.reset();

    if (start_callback_)
      std::move(start_callback_).Run(error_msg);

    if (pending_disable_tracing_task_) {
      // Will call endpoint_->ReceivedTraceFinalContents() and delete |this|.
      std::move(pending_disable_tracing_task_).Run();
    } else if (endpoint_) {
      // Will delete |this|.
      endpoint_->ReceivedTraceFinalContents();
    }
  }

  void OnTracingSessionStopped() {
    DCHECK(tracing_session_);

    auto weak_ptr = weak_factory_.GetWeakPtr();

    if (use_proto_format_) {
      tracing_session_->ReadTrace(
          [weak_ptr](perfetto::TracingSession::ReadTraceCallbackArgs args) {
            tracing::ReadTraceAsProtobuf(
                args,
                base::BindOnce(&PerfettoTracingSession::OnTraceData, weak_ptr),
                base::BindOnce(&PerfettoTracingSession::OnTraceDataComplete,
                               weak_ptr),
                GetUIThreadTaskRunner({}));
          });
    } else {
      // Ref-counted because it is used within the lambda running on the
      // perfetto SDK's thread.
      auto tokenizer = base::MakeRefCounted<
          base::RefCountedData<std::unique_ptr<tracing::TracePacketTokenizer>>>(
          std::make_unique<tracing::TracePacketTokenizer>());
      tracing_session_->ReadTrace(
          [weak_ptr,
           tokenizer](perfetto::TracingSession::ReadTraceCallbackArgs args) {
            tracing::ReadTraceAsJson(
                args, tokenizer,
                base::BindOnce(&PerfettoTracingSession::OnTraceData, weak_ptr),
                base::BindOnce(&PerfettoTracingSession::OnTraceDataComplete,
                               weak_ptr),
                GetUIThreadTaskRunner({}));
          });
    }
  }

  void OnBufferUsage(bool success, float percent_full, bool data_loss) {
    data_loss_ |= data_loss;
    if (on_buffer_usage_callback_) {
      std::move(on_buffer_usage_callback_).Run(success, percent_full, 0);
    }
  }

  void OnTraceData(std::unique_ptr<std::string> data) {
    endpoint_->ReceiveTraceChunk(std::move(data));
  }

  void OnTraceDataComplete() {
    // Request stats to check if data loss occurred.
    GetBufferUsage(base::BindOnce(&PerfettoTracingSession::OnFinalBufferUsage,
                                  weak_factory_.GetWeakPtr()));
  }

  void OnFinalBufferUsage(bool success,
                          float percent_full,
                          size_t approximate_event_count) {
    // Will delete |this|.
    endpoint_->ReceivedTraceFinalContents();
  }

  std::unique_ptr<perfetto::TracingSession> tracing_session_;
  const bool use_proto_format_;
  perfetto::BackendType backend_type_ =
      perfetto::BackendType::kUnspecifiedBackend;
  base::OnceCallback<void(const std::string&)> start_callback_;
  base::OnceCallback<
      void(bool success, float percent_full, size_t approximate_event_count)>
      on_buffer_usage_callback_;
  base::OnceClosure pending_disable_tracing_task_;
  bool waiting_for_startup_tracing_enabled_ = false;
  scoped_refptr<TracingController::TraceDataEndpoint> endpoint_;
  bool tracing_active_ = false;
  bool data_loss_ = false;

#if DCHECK_IS_ON()
  perfetto::TraceConfig last_perfetto_config_;
#endif

  base::WeakPtrFactory<PerfettoTracingSession> weak_factory_{this};
};

TracingHandler::TracingHandler(DevToolsAgentHostImpl* host,
                               DevToolsIOContext* io_context,
                               DevToolsSession* root_session)
    : DevToolsDomainHandler(Tracing::Metainfo::domainName),
      io_context_(io_context),
      host_(host),
      session_for_process_filter_(root_session),
      did_initiate_recording_(false),
      return_as_stream_(false),
      gzip_compression_(false),
      buffer_usage_reporting_interval_(0) {
  video_consumer_ = std::make_unique<DevToolsVideoConsumer>(base::BindRepeating(
      &TracingHandler::OnFrameFromVideoConsumer, base::Unretained(this)));
}

TracingHandler::~TracingHandler() = default;

// static
std::vector<TracingHandler*> TracingHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<TracingHandler>(Tracing::Metainfo::domainName);
}

void TracingHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Tracing::Frontend>(dispatcher->channel());
  Tracing::Dispatcher::wire(dispatcher, this);
}

Response TracingHandler::Disable() {
  if (session_)
    StopTracing(nullptr);
  return Response::Success();
}

namespace {
class TracingNotification : public crdtp::Serializable {
 public:
  explicit TracingNotification(std::string json) : json_(std::move(json)) {}

  void AppendSerialized(std::vector<uint8_t>* out) const override {
    crdtp::Status status =
        crdtp::json::ConvertJSONToCBOR(crdtp::SpanFrom(json_), out);
    DCHECK(status.ok()) << status.ToASCIIString();
  }

 private:
  std::string json_;
};
}  // namespace

void TracingHandler::OnTraceDataCollected(
    std::unique_ptr<std::string> trace_fragment) {
  const std::string valid_trace_fragment =
      UpdateTraceDataBuffer(*trace_fragment);
  if (valid_trace_fragment.empty())
    return;

  // Hand-craft protocol notification message so we can substitute JSON
  // that we already got as string as a bare object, not a quoted string.
  std::string message(
      "{ \"method\": \"Tracing.dataCollected\", \"params\": { \"value\": [");
  const size_t messageSuffixSize = 10;
  message.reserve(message.size() + valid_trace_fragment.size() +
                  messageSuffixSize - trace_data_buffer_state_.offset);
  message.append(valid_trace_fragment.c_str() +
                 trace_data_buffer_state_.offset);
  message += "] } }";

  frontend_->sendRawNotification(
      std::make_unique<TracingNotification>(std::move(message)));
}

void TracingHandler::OnTraceComplete() {
  if (!trace_data_buffer_state_.data.empty())
    OnTraceDataCollected(std::make_unique<std::string>(""));

  DCHECK(trace_data_buffer_state_.data.empty());
  DCHECK_EQ(0u, trace_data_buffer_state_.pos);
  DCHECK_EQ(0, trace_data_buffer_state_.open_braces);
  DCHECK(!trace_data_buffer_state_.in_string);
  DCHECK(!trace_data_buffer_state_.slashed);

  bool data_loss = session_->HasDataLossOccurred();
  process_set_monitor_.reset();
  session_.reset();
  frontend_->TracingComplete(data_loss);
}

std::string TracingHandler::UpdateTraceDataBuffer(
    const std::string& trace_fragment) {
  size_t end = 0;
  size_t last_open = 0;
  TraceDataBufferState& state = trace_data_buffer_state_;
  state.offset = 0;
  bool update_offset = state.open_braces == 0;
  for (; state.pos < trace_fragment.size(); ++state.pos) {
    char c = trace_fragment[state.pos];
    switch (c) {
      case '{':
        if (!state.in_string && !state.slashed) {
          state.open_braces++;
          if (state.open_braces == 1) {
            last_open = state.data.size() + state.pos;
            if (update_offset) {
              state.offset = last_open;
              update_offset = false;
            }
          }
        }
        break;
      case '}':
        if (!state.in_string && !state.slashed) {
          DCHECK_GT(state.open_braces, 0);
          state.open_braces--;
          if (state.open_braces == 0)
            end = state.data.size() + state.pos + 1;
        }
        break;
      case '"':
        if (!state.slashed)
          state.in_string = !state.in_string;
        break;
      case 'u':
        if (state.slashed)
          state.pos += 4;
        break;
    }

    if (state.in_string && c == '\\') {
      state.slashed = !state.slashed;
    } else {
      state.slashed = false;
    }
  }

  // Next starting position is usually 0 except when we are in the middle of
  // processing a unicode character, i.e. \uxxxx.
  state.pos -= trace_fragment.size();

  std::string complete_str = state.data + trace_fragment;
  state.data = complete_str.substr(std::max(end, last_open));

  complete_str.resize(end);
  return complete_str;
}

void TracingHandler::OnTraceToStreamComplete(const std::string& stream_handle) {
  bool data_loss = session_->HasDataLossOccurred();
  process_set_monitor_.reset();
  session_.reset();
  std::string stream_format = (proto_format_ ? Tracing::StreamFormatEnum::Proto
                                             : Tracing::StreamFormatEnum::Json);
  std::string stream_compression =
      (gzip_compression_ ? Tracing::StreamCompressionEnum::Gzip
                         : Tracing::StreamCompressionEnum::None);
  frontend_->TracingComplete(data_loss, stream_handle, stream_format,
                             stream_compression);
}

void TracingHandler::Start(Maybe<std::string> categories,
                           Maybe<std::string> options,
                           Maybe<double> buffer_usage_reporting_interval,
                           Maybe<std::string> transfer_mode,
                           Maybe<std::string> transfer_format,
                           Maybe<std::string> transfer_compression,
                           Maybe<Tracing::TraceConfig> config,
                           Maybe<Binary> perfetto_config,
                           Maybe<std::string> tracing_backend,
                           std::unique_ptr<StartCallback> callback) {
  bool return_as_stream = transfer_mode.value_or("") ==
                          Tracing::Start::TransferModeEnum::ReturnAsStream;
  bool gzip_compression =
      transfer_compression.value_or("") == Tracing::StreamCompressionEnum::Gzip;
  bool proto_format =
      transfer_format.value_or("") == Tracing::StreamFormatEnum::Proto;

  perfetto::TraceConfig trace_config;
  if (perfetto_config.has_value()) {
    bool parsed = trace_config.ParseFromArray(perfetto_config.value().data(),
                                              perfetto_config.value().size());
    if (!parsed) {
      callback->sendFailure(Response::InvalidParams(
          "Couldn't parse the supplied perfettoConfig."));
      return;
    }

    if (!trace_config.data_sources_size()) {
      callback->sendFailure(Response::InvalidParams(
          "Supplied perfettoConfig doesn't have any data sources specified"));
      return;
    }

    // Default to proto format for perfettoConfig, except if it specifies
    // convert_to_legacy_json in the data source config.
    proto_format = true;
    for (const auto& data_source : trace_config.data_sources()) {
      if (data_source.config().has_chrome_config() &&
          data_source.config().chrome_config().convert_to_legacy_json()) {
        proto_format = false;
        break;
      }
    }

    ConvertToTrackEventConfigIfNeeded(trace_config);
  } else {
    base::trace_event::TraceConfig browser_config =
        base::trace_event::TraceConfig();
    if (config.has_value()) {
      base::Value::Dict dict;
      CHECK(crdtp::ConvertProtocolValue(config.value(), &dict));
      browser_config =
          GetTraceConfigFromDevToolsConfig(base::Value(std::move(dict)));
    } else if (categories.has_value() || options.has_value()) {
      browser_config = base::trace_event::TraceConfig(categories.value_or(""),
                                                      options.value_or(""));
    }
    trace_config = CreatePerfettoConfiguration(browser_config, return_as_stream,
                                               proto_format);
  }

  std::optional<perfetto::BackendType> backend = GetBackendTypeFromParameters(
      tracing_backend.value_or(Tracing::TracingBackendEnum::Auto),
      trace_config);

  if (!backend) {
    callback->sendFailure(Response::InvalidParams(
        "Unsupported value for tracing_backend parameter."));
    return;
  }

  // Check if we should adopt the startup tracing session. Only the first
  // Tracing.start() sent to the browser endpoint can adopt it.
  // TODO(crbug.com/40171330): Add tests for system-controlled startup traces.
  AttemptAdoptStartupSession(return_as_stream, gzip_compression, proto_format,
                             *backend);

  if (IsTracing()) {
    callback->sendFailure(Response::ServerError(
        "Tracing has already been started (possibly in another tab)."));
    return;
  }

  if (did_initiate_recording_) {
    callback->sendFailure(Response::ServerError(
        "Starting trace recording is already in progress"));
    return;
  }

  if (config.has_value() && (categories.has_value() || options.has_value())) {
    callback->sendFailure(Response::InvalidParams(
        "Either trace config (preferred), or categories+options should be "
        "specified, but not both."));
    return;
  }

  if (proto_format && !return_as_stream) {
    callback->sendFailure(Response::InvalidParams(
        "Proto format is only supported when using stream transfer mode."));
    return;
  }

  return_as_stream_ = return_as_stream;
  gzip_compression_ = gzip_compression;
  proto_format_ = proto_format;
  buffer_usage_reporting_interval_ =
      buffer_usage_reporting_interval.value_or(0);
  did_initiate_recording_ = true;
  trace_config_ = std::move(trace_config);

  if (session_for_process_filter_) {
    process_set_monitor_ = TracingProcessSetMonitor::Start(
        *session_for_process_filter_,
        base::BindRepeating(&TracingHandler::AddProcessToFilter,
                            base::Unretained(this)));
    std::unordered_set<base::ProcessId> pids = process_set_monitor_->GetPids();

    base::ProcessId browser_pid = base::Process::Current().Pid();
    pids.insert(browser_pid);
    if (auto* gpu_process_host =
            GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED,
                                /* force_create */ false)) {
      base::ProcessId gpu_pid = gpu_process_host->process_id();
      if (gpu_pid != base::kNullProcessId) {
        pids.insert(gpu_pid);
      }
    }
    AddPidsToProcessFilter(pids, trace_config_);
  }

  session_ = std::make_unique<PerfettoTracingSession>(proto_format_, *backend);
  session_->EnableTracing(
      trace_config_,
      base::BindOnce(&TracingHandler::OnRecordingEnabled,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

perfetto::TraceConfig TracingHandler::CreatePerfettoConfiguration(
    const base::trace_event::TraceConfig& browser_config,
    bool return_as_stream,
    bool proto_format) {
  return tracing::GetDefaultPerfettoConfig(
      browser_config,
      /*privacy_filtering_enabled=*/false,
      /*convert_to_legacy_json=*/!proto_format,
      perfetto::protos::gen::ChromeConfig::USER_INITIATED,
      /*json_agent_label_filter*/
      (proto_format || return_as_stream)
          ? ""
          : tracing::mojom::kChromeTraceEventLabel);
}

void TracingHandler::AddProcessToFilter(base::ProcessId pid) {
  CHECK(did_initiate_recording_);
  CHECK(session_);
  AddPidsToProcessFilter({pid}, trace_config_);
  session_->ChangeTraceConfig(trace_config_);
}

void TracingHandler::AttemptAdoptStartupSession(
    bool return_as_stream,
    bool gzip_compression,
    bool proto_format,
    perfetto::BackendType tracing_backend) {
  // Only adopt startup session for browser-level sessions.
  if (session_for_process_filter_) {
    return;
  }
  auto& startup_config = tracing::TraceStartupConfig::GetInstance();
  if (!startup_config.AttemptAdoptBySessionOwner(
          tracing::TraceStartupConfig::SessionOwner::kDevToolsTracingHandler)) {
    return;
  }

  return_as_stream_ = return_as_stream;
  gzip_compression_ = gzip_compression;
  proto_format_ = proto_format;

  perfetto::TraceConfig perfetto_config =
      tracing::TraceStartupConfig::GetInstance().GetPerfettoConfig();

  session_ =
      std::make_unique<PerfettoTracingSession>(proto_format_, tracing_backend);
  session_->AdoptStartupTracingSession(perfetto_config);
}

Response TracingHandler::End() {
  if (!session_) {
    did_initiate_recording_ = false;
    return Response::ServerError("Tracing is not started");
  }

  if (session_->HasTracingFailed())
    return Response::ServerError("Tracing failed");

  scoped_refptr<TracingController::TraceDataEndpoint> endpoint;
  if (return_as_stream_) {
    endpoint = new DevToolsStreamEndpoint(
        weak_factory_.GetWeakPtr(),
        DevToolsStreamFile::Create(
            io_context_, gzip_compression_ || proto_format_ /* binary */));
    if (gzip_compression_) {
      endpoint = TracingControllerImpl::CreateCompressedStringEndpoint(
          endpoint, true /* compress_with_background_priority */);
    }
  } else {
    // Reset the trace data buffer state.
    trace_data_buffer_state_ = TracingHandler::TraceDataBufferState();
    endpoint = new DevToolsTraceEndpointProxy(weak_factory_.GetWeakPtr());
  }

  StopTracing(endpoint);

  return Response::Success();
}

void TracingHandler::GetCategories(
    std::unique_ptr<GetCategoriesCallback> callback) {
  // TODO(eseckler): Support this via the perfetto service too.
  TracingController::GetInstance()->GetCategories(
      base::BindOnce(&TracingHandler::OnCategoriesReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void TracingHandler::OnRecordingEnabled(std::unique_ptr<StartCallback> callback,
                                        const std::string& error_msg) {
  if (!error_msg.empty()) {
    callback->sendFailure(Response::ServerError(error_msg));
    return;
  }

  if (!did_initiate_recording_) {
    callback->sendFailure(Response::ServerError(
        "Tracing was stopped before start has been completed."));
    return;
  }

  EmitFrameTree();
  callback->sendSuccess();

  SetupTimer(buffer_usage_reporting_interval_);

  bool screenshot_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"), &screenshot_enabled);
  if (screenshot_enabled) {
    // Reset number of screenshots received, each time tracing begins.
    number_of_screenshots_from_video_consumer_ = 0;
    if (WebContents* wc = host_ ? host_->GetWebContents() : nullptr) {
      auto* frame_host =
          static_cast<RenderFrameHostImpl*>(wc->GetPrimaryMainFrame());
      video_consumer_->SetFrameSinkId(
          frame_host->GetRenderWidgetHost()->GetFrameSinkId());
    }
    video_consumer_->SetMinAndMaxFrameSize(kMinFrameSize, kMaxFrameSize);
    video_consumer_->StartCapture();
  }
}

void TracingHandler::OnBufferUsage(bool success,
                                   float percent_full,
                                   size_t approximate_event_count) {
  if (!did_initiate_recording_)
    return;

  if (!success)
    return;

  // TODO(crbug426117): remove set_value once all clients have switched to
  // the new interface of the event.
  frontend_->BufferUsage(percent_full, approximate_event_count, percent_full);
}

void TracingHandler::OnCategoriesReceived(
    std::unique_ptr<GetCategoriesCallback> callback,
    const std::set<std::string>& category_set) {
  auto categories = std::make_unique<protocol::Array<std::string>>(
      category_set.begin(), category_set.end());
  callback->sendSuccess(std::move(categories));
}

void TracingHandler::RequestMemoryDump(
    Maybe<bool> deterministic,
    Maybe<std::string> level_of_detail,
    std::unique_ptr<RequestMemoryDumpCallback> callback) {
  if (!IsTracing()) {
    callback->sendFailure(Response::ServerError("Tracing is not started"));
    return;
  }

  std::optional<base::trace_event::MemoryDumpLevelOfDetail> memory_detail =
      StringToMemoryDumpLevelOfDetail(level_of_detail.value_or(
          Tracing::MemoryDumpLevelOfDetailEnum::Detailed));

  if (!memory_detail) {
    callback->sendFailure(
        Response::ServerError("Invalid levelOfDetail specified."));
    return;
  }

  auto determinism = deterministic.value_or(false)
                         ? base::trace_event::MemoryDumpDeterminism::kForceGc
                         : base::trace_event::MemoryDumpDeterminism::kNone;

  auto on_memory_dump_finished =
      base::BindOnce(&TracingHandler::OnMemoryDumpFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDumpAndAppendToTrace(
          base::trace_event::MemoryDumpType::kExplicitlyTriggered,
          *memory_detail, determinism, std::move(on_memory_dump_finished));
}

void TracingHandler::OnMemoryDumpFinished(
    std::unique_ptr<RequestMemoryDumpCallback> callback,
    bool success,
    uint64_t dump_id) {
  callback->sendSuccess(base::StringPrintf("0x%" PRIx64, dump_id), success);
}

void TracingHandler::OnFrameFromVideoConsumer(
    scoped_refptr<media::VideoFrame> frame) {
  if (!IsTracing()) {
    return;
  }
  const SkBitmap skbitmap = DevToolsVideoConsumer::GetSkBitmapFromFrame(frame);
  uint64_t frame_sequence = *frame->metadata().frame_sequence;

  // This reference_time is an ESTIMATE. It is set by the compositor frame sink
  // from the `expected_display_time`, which is based on a previously known
  // frame start PLUS the vsync interval (eg 16.6ms)
  base::TimeTicks expected_display_time = *frame->metadata().reference_time;

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID_AND_TIMESTAMP(
      TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"), "Screenshot",
      frame_sequence, expected_display_time,
      std::make_unique<DevToolsTraceableScreenshot>(skbitmap));

  ++number_of_screenshots_from_video_consumer_;
  DCHECK(video_consumer_);
  if (number_of_screenshots_from_video_consumer_ >=
      DevToolsTraceableScreenshot::kMaximumNumberOfScreenshots) {
    video_consumer_->StopCapture();
  }
}

Response TracingHandler::RecordClockSyncMarker(const std::string& sync_id) {
  if (!IsTracing())
    return Response::ServerError("Tracing is not started");
  TRACE_EVENT_CLOCK_SYNC_RECEIVER(sync_id);
  return Response::Success();
}

void TracingHandler::SetupTimer(double usage_reporting_interval) {
  if (usage_reporting_interval == 0)
    return;

  if (usage_reporting_interval < kMinimumReportingInterval)
    usage_reporting_interval = kMinimumReportingInterval;

  base::TimeDelta interval =
      base::Milliseconds(std::ceil(usage_reporting_interval));
  buffer_usage_poll_timer_ = std::make_unique<base::RepeatingTimer>();
  buffer_usage_poll_timer_->Start(
      FROM_HERE, interval,
      base::BindRepeating(&TracingHandler::UpdateBufferUsage,
                          weak_factory_.GetWeakPtr()));
}

void TracingHandler::UpdateBufferUsage() {
  session_->GetBufferUsage(base::BindOnce(&TracingHandler::OnBufferUsage,
                                          weak_factory_.GetWeakPtr()));
}

void TracingHandler::StopTracing(
    const scoped_refptr<TracingController::TraceDataEndpoint>& endpoint) {
  DCHECK(session_);
  buffer_usage_poll_timer_.reset();
  process_set_monitor_.reset();
  if (endpoint) {
    // Will delete |session_|.
    session_->DisableTracing(std::move(endpoint));
  } else {
    session_.reset();
  }
  did_initiate_recording_ = false;
  video_consumer_->StopCapture();
}

bool TracingHandler::IsTracing() const {
  return TracingController::GetInstance()->IsTracing() || g_any_agent_tracing;
}

void TracingHandler::EmitFrameTree() {
  auto data = std::make_unique<base::trace_event::TracedValue>();
  if (WebContents* wc = host_ ? host_->GetWebContents() : nullptr) {
    auto* frame_host =
        static_cast<RenderFrameHostImpl*>(wc->GetPrimaryMainFrame());
    CHECK(frame_host);
    data->SetInteger(
        "frameTreeNodeId",
        frame_host->frame_tree_node()->frame_tree_node_id().value());
    data->SetBoolean("persistentIds", true);
    data->BeginArray("frames");
    wc->ForEachRenderFrameHost([&data](RenderFrameHost* rfh) {
      data->BeginDictionary();
      FillFrameData(data.get(), static_cast<RenderFrameHostImpl*>(rfh),
                    rfh->GetLastCommittedURL());
      data->EndDictionary();
    });
    data->EndArray();
  }
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "TracingStartedInBrowser", TRACE_EVENT_SCOPE_THREAD,
                       "data", std::move(data));
}

void TracingHandler::WillInitiatePrerender(FrameTreeNode* frame_tree_node) {
  if (!did_initiate_recording_) {
    return;
  }
  auto data = std::make_unique<base::trace_event::TracedValue>();
  FillFrameData(data.get(), frame_tree_node->current_frame_host(),
                frame_tree_node->current_url());
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "FrameCommittedInBrowser", TRACE_EVENT_SCOPE_THREAD,
                       "data", std::move(data));
}

void TracingHandler::ReadyToCommitNavigation(
    NavigationRequest* navigation_request) {
  if (!did_initiate_recording_)
    return;
  auto data = std::make_unique<base::trace_event::TracedValue>();
  RenderFrameHostImpl* frame_host = navigation_request->GetRenderFrameHost();
  FillFrameData(data.get(), frame_host, navigation_request->GetURL());
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "FrameCommittedInBrowser", TRACE_EVENT_SCOPE_THREAD,
                       "data", std::move(data));
  if (frame_host->IsOutermostMainFrame()) {
    video_consumer_->SetFrameSinkId(navigation_request->GetRenderFrameHost()
                                        ->GetRenderWidgetHost()
                                        ->GetFrameSinkId());
  }
}

void TracingHandler::FrameDeleted(FrameTreeNodeId frame_tree_node_id) {
  if (!did_initiate_recording_)
    return;
  FrameTreeNode* node = FrameTreeNode::GloballyFindByID(frame_tree_node_id);

  if (!node->current_frame_host()) {
    // This might happen on prerendering activation when the prerender tree is
    // shutting how and the RFH is migrated to a different frame tree.
    return;
  }
  auto data = std::make_unique<base::trace_event::TracedValue>();
  data->SetString(
      "frame", node->current_frame_host()->devtools_frame_token().ToString());
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "FrameDeletedInBrowser", TRACE_EVENT_SCOPE_THREAD,
                       "data", std::move(data));
}

// static
bool TracingHandler::IsStartupTracingActive() {
  return ::tracing::TraceStartupConfig::GetInstance().IsEnabled();
}

// static
base::trace_event::TraceConfig TracingHandler::GetTraceConfigFromDevToolsConfig(
    const base::Value& devtools_config) {
  base::Value config = ConvertDictKeyStyle(devtools_config);
  base::Value::Dict& config_dict = config.GetDict();
  if (std::string* mode = config_dict.FindString(kRecordModeParam)) {
    config_dict.Set(kRecordModeParam, ConvertFromCamelCase(*mode, '-'));
  }
  if (std::optional<double> buffer_size =
          config_dict.FindDouble(kTraceBufferSizeInKb)) {
    config_dict.Set(
        kTraceBufferSizeInKb,
        static_cast<int>(base::saturated_cast<size_t>(buffer_size.value())));
  }
  return base::trace_event::TraceConfig(config_dict);
}

}  // namespace content::protocol
