// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/tracing_handler.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event_impl.h"
#include "base/trace_event/tracing_agent.h"
#include "components/tracing/common/trace_startup_config.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_frame_trace_recorder.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/browser/devtools/devtools_stream_file.h"
#include "content/browser/devtools/devtools_traceable_screenshot.h"
#include "content/browser/devtools/devtools_video_consumer.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/tracing/public/mojom/constants.mojom.h"

#ifdef OS_ANDROID
#include "content/browser/renderer_host/compositor_impl_android.h"
#endif

namespace content {
namespace protocol {

namespace {

const double kMinimumReportingInterval = 250.0;

const char kRecordModeParam[] = "record_mode";

// Settings for |video_consumer_|.
// Tracing requires a 10ms minimum capture period.
constexpr base::TimeDelta kMinCapturePeriod =
    base::TimeDelta::FromMilliseconds(10);

// Frames need to be at least 1x1, otherwise nothing would be captured.
constexpr gfx::Size kMinFrameSize = gfx::Size(1, 1);

// Frames do not need to be greater than 500x500 for tracing.
constexpr gfx::Size kMaxFrameSize = gfx::Size(500, 500);

// Convert from camel case to separator + lowercase.
std::string ConvertFromCamelCase(const std::string& in_str, char separator) {
  std::string out_str;
  out_str.reserve(in_str.size());
  for (const char& c : in_str) {
    if (isupper(c)) {
      out_str.push_back(separator);
      out_str.push_back(tolower(c));
    } else {
      out_str.push_back(c);
    }
  }
  return out_str;
}

std::unique_ptr<base::Value> ConvertDictKeyStyle(const base::Value& value) {
  const base::DictionaryValue* dict = nullptr;
  if (value.GetAsDictionary(&dict)) {
    std::unique_ptr<base::DictionaryValue> out_dict(
        new base::DictionaryValue());
    for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd();
         it.Advance()) {
      out_dict->Set(ConvertFromCamelCase(it.key(), '_'),
                    ConvertDictKeyStyle(it.value()));
    }
    return std::move(out_dict);
  }

  const base::ListValue* list = nullptr;
  if (value.GetAsList(&list)) {
    std::unique_ptr<base::ListValue> out_list(new base::ListValue());
    for (const auto& key : *list)
      out_list->Append(ConvertDictKeyStyle(key));
    return std::move(out_list);
  }

  return value.CreateDeepCopy();
}

class DevToolsTraceEndpointProxy : public TracingController::TraceDataEndpoint {
 public:
  explicit DevToolsTraceEndpointProxy(base::WeakPtr<TracingHandler> handler)
      : tracing_handler_(handler) {}

  void ReceiveTraceChunk(std::unique_ptr<std::string> chunk) override {
    if (TracingHandler* h = tracing_handler_.get())
      h->OnTraceDataCollected(std::move(chunk));
  }
  void ReceiveTraceFinalContents(
      std::unique_ptr<const base::DictionaryValue> metadata) override {
    if (TracingHandler* h = tracing_handler_.get())
      h->OnTraceComplete();
  }

 private:
  ~DevToolsTraceEndpointProxy() override {}

  base::WeakPtr<TracingHandler> tracing_handler_;
};

class DevToolsStreamEndpoint : public TracingController::TraceDataEndpoint {
 public:
  explicit DevToolsStreamEndpoint(
      base::WeakPtr<TracingHandler> handler,
      const scoped_refptr<DevToolsStreamFile>& stream)
      : stream_(stream), tracing_handler_(handler) {}

  void ReceiveTraceChunk(std::unique_ptr<std::string> chunk) override {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&DevToolsStreamEndpoint::ReceiveTraceChunk, this,
                         std::move(chunk)));
      return;
    }

    stream_->Append(std::move(chunk));
  }

  void ReceiveTraceFinalContents(
      std::unique_ptr<const base::DictionaryValue> metadata) override {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&DevToolsStreamEndpoint::ReceiveTraceFinalContents,
                         this, std::move(metadata)));
      return;
    }
    if (TracingHandler* h = tracing_handler_.get())
      h->OnTraceToStreamComplete(stream_->handle());
  }

 private:
  ~DevToolsStreamEndpoint() override {}

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
                   FrameTreeNode* node,
                   RenderFrameHostImpl* frame_host,
                   const GURL& url) {
  url::Replacements<char> strip_fragment;
  strip_fragment.ClearRef();
  data->SetString("frame", node->devtools_frame_token().ToString());
  data->SetString("url", url.ReplaceComponents(strip_fragment).spec());
  data->SetString("name", node->frame_name());
  if (node->parent())
    data->SetString("parent",
                    node->parent()->devtools_frame_token().ToString());
  if (frame_host) {
    RenderProcessHost* process_host = frame_host->GetProcess();
    const base::Process& process_handle = process_host->GetProcess();
    if (!process_handle.IsValid()) {
      data->SetString("processPseudoId", GetProcessHostHex(process_host));
      frame_host->GetProcess()->PostTaskWhenProcessIsReady(
          base::BindOnce(&SendProcessReadyInBrowserEvent,
                         node->devtools_frame_token(), process_host));
    } else {
      // Cast process id to int to be compatible with tracing.
      data->SetInteger("processId", static_cast<int>(process_handle.Pid()));
    }
  }
}

}  // namespace

TracingHandler::TracingHandler(FrameTreeNode* frame_tree_node_,
                               DevToolsIOContext* io_context)
    : DevToolsDomainHandler(Tracing::Metainfo::domainName),
      io_context_(io_context),
      frame_tree_node_(frame_tree_node_),
      did_initiate_recording_(false),
      return_as_stream_(false),
      gzip_compression_(false),
      weak_factory_(this) {
  bool use_video_capture_api = true;
#ifdef OS_ANDROID
  // Video capture API cannot be used on Android WebView.
  if (!CompositorImpl::IsInitialized())
    use_video_capture_api = false;
#endif
  if (use_video_capture_api) {
    video_consumer_ =
        std::make_unique<DevToolsVideoConsumer>(base::BindRepeating(
            &TracingHandler::OnFrameFromVideoConsumer, base::Unretained(this)));
  }
}

TracingHandler::~TracingHandler() = default;

// static
std::vector<TracingHandler*> TracingHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<TracingHandler>(Tracing::Metainfo::domainName);
}

void TracingHandler::SetRenderer(int process_host_id,
                                 RenderFrameHostImpl* frame_host) {
  if (!video_consumer_ || !frame_host)
    return;
  video_consumer_->SetFrameSinkId(
      frame_host->GetRenderWidgetHost()->GetFrameSinkId());
}

void TracingHandler::Wire(UberDispatcher* dispatcher) {
  frontend_.reset(new Tracing::Frontend(dispatcher->channel()));
  Tracing::Dispatcher::wire(dispatcher, this);
}

Response TracingHandler::Disable() {
  if (did_initiate_recording_)
    StopTracing(nullptr, "");
  return Response::OK();
}

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
  frontend_->sendRawNotification(message);
}

void TracingHandler::OnTraceComplete() {
  if (!trace_data_buffer_state_.data.empty())
    OnTraceDataCollected(std::make_unique<std::string>(""));

  DCHECK(trace_data_buffer_state_.data.empty());
  DCHECK_EQ(0u, trace_data_buffer_state_.pos);
  DCHECK_EQ(0, trace_data_buffer_state_.open_braces);
  DCHECK(!trace_data_buffer_state_.in_string);
  DCHECK(!trace_data_buffer_state_.slashed);

  frontend_->TracingComplete();
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
  std::string stream_compression =
      (gzip_compression_ ? Tracing::StreamCompressionEnum::Gzip
                         : Tracing::StreamCompressionEnum::None);
  frontend_->TracingComplete(stream_handle, stream_compression);
}

void TracingHandler::Start(Maybe<std::string> categories,
                           Maybe<std::string> options,
                           Maybe<double> buffer_usage_reporting_interval,
                           Maybe<std::string> transfer_mode,
                           Maybe<std::string> transfer_compression,
                           Maybe<Tracing::TraceConfig> config,
                           std::unique_ptr<StartCallback> callback) {
  bool return_as_stream = transfer_mode.fromMaybe("") ==
      Tracing::Start::TransferModeEnum::ReturnAsStream;
  bool gzip_compression = transfer_compression.fromMaybe("") ==
                          Tracing::StreamCompressionEnum::Gzip;
  if (IsTracing()) {
    if (!did_initiate_recording_ && IsStartupTracingActive()) {
      // If tracing is already running because it was initiated by startup
      // tracing, honor the transfer mode update, as that's the only way
      // for the client to communicate it.
      return_as_stream_ = return_as_stream;
      gzip_compression_ = gzip_compression;
    }
    callback->sendFailure(Response::Error("Tracing is already started"));
    return;
  }

  if (config.isJust() && (categories.isJust() || options.isJust())) {
    callback->sendFailure(Response::InvalidParams(
        "Either trace config (preferred), or categories+options should be "
        "specified, but not both."));
    return;
  }

  did_initiate_recording_ = true;
  return_as_stream_ = return_as_stream;
  gzip_compression_ = gzip_compression;
  if (buffer_usage_reporting_interval.isJust())
    SetupTimer(buffer_usage_reporting_interval.fromJust());

  trace_config_ = base::trace_event::TraceConfig();
  if (config.isJust()) {
    std::unique_ptr<base::Value> value =
        protocol::toBaseValue(config.fromJust()->toValue().get(), 1000);
    if (value && value->is_dict()) {
      trace_config_ = GetTraceConfigFromDevToolsConfig(
          *static_cast<base::DictionaryValue*>(value.get()));
    }
  } else if (categories.isJust() || options.isJust()) {
    trace_config_ = base::trace_event::TraceConfig(categories.fromMaybe(""),
                                                   options.fromMaybe(""));
  }

  // GPU process id can only be retrieved on IO thread. Do some thread hopping.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO}, base::BindOnce([]() {
        GpuProcessHost* gpu_process_host =
            GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                                /* force_create */ false);
        return gpu_process_host ? gpu_process_host->process_id()
                                : base::kNullProcessId;
      }),
      base::BindOnce(&TracingHandler::StartTracingWithGpuPid,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void TracingHandler::StartTracingWithGpuPid(
    std::unique_ptr<StartCallback> callback,
    base::ProcessId gpu_pid) {
  // Check if tracing was stopped in mid-air.
  if (!did_initiate_recording_) {
    callback->sendFailure(Response::Error(
        "Tracing was stopped before start has been completed."));
    return;
  }

  SetupProcessFilter(gpu_pid, nullptr);

  TracingController::GetInstance()->StartTracing(
      trace_config_,
      base::BindOnce(&TracingHandler::OnRecordingEnabled,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void TracingHandler::SetupProcessFilter(
    base::ProcessId gpu_pid,
    RenderFrameHost* new_render_frame_host) {
  if (!frame_tree_node_)
    return;

  base::ProcessId browser_pid = base::Process::Current().Pid();
  std::unordered_set<base::ProcessId> included_process_ids({browser_pid});

  if (gpu_pid != base::kNullProcessId)
    included_process_ids.insert(gpu_pid);

  if (new_render_frame_host)
    AppendProcessId(new_render_frame_host, &included_process_ids);

  for (FrameTreeNode* node :
       frame_tree_node_->frame_tree()->SubtreeNodes(frame_tree_node_)) {
    RenderFrameHost* frame_host = node->current_frame_host();
    if (frame_host)
      AppendProcessId(frame_host, &included_process_ids);
  }
  trace_config_.SetProcessFilterConfig(
      base::trace_event::TraceConfig::ProcessFilterConfig(
          included_process_ids));
}

void TracingHandler::AppendProcessId(
    RenderFrameHost* render_frame_host,
    std::unordered_set<base::ProcessId>* process_set) {
  RenderProcessHost* process_host = render_frame_host->GetProcess();
  if (process_host->GetProcess().IsValid()) {
    process_set->insert(process_host->GetProcess().Pid());
  } else {
    process_host->PostTaskWhenProcessIsReady(
        base::BindOnce(&TracingHandler::OnProcessReady,
                       weak_factory_.GetWeakPtr(), process_host));
  }
}

void TracingHandler::OnProcessReady(RenderProcessHost* process_host) {
  if (!did_initiate_recording_)
    return;
  std::unordered_set<base::ProcessId> included_process_ids(
      {process_host->GetProcess().Pid()});
  trace_config_.SetProcessFilterConfig(
      base::trace_event::TraceConfig::ProcessFilterConfig(
          included_process_ids));
  TracingController::GetInstance()->StartTracing(
      trace_config_, TracingController::StartTracingDoneCallback());
}

Response TracingHandler::End() {
  // Startup tracing triggered by --trace-config-file is a special case, where
  // tracing is started automatically upon browser startup and can be stopped
  // via DevTools.
  if (!did_initiate_recording_ && !IsStartupTracingActive())
    return Response::Error("Tracing is not started");

  scoped_refptr<TracingController::TraceDataEndpoint> endpoint;
  if (return_as_stream_) {
    endpoint = new DevToolsStreamEndpoint(
        weak_factory_.GetWeakPtr(),
        DevToolsStreamFile::Create(io_context_,
                                   gzip_compression_ /* binary */));
    if (gzip_compression_) {
      endpoint = TracingControllerImpl::CreateCompressedStringEndpoint(
          endpoint, true /* compress_with_background_priority */);
    }
    StopTracing(endpoint, "");
  } else {
    // Reset the trace data buffer state.
    trace_data_buffer_state_ = TracingHandler::TraceDataBufferState();
    endpoint = new DevToolsTraceEndpointProxy(weak_factory_.GetWeakPtr());
    StopTracing(endpoint, tracing::mojom::kChromeTraceEventLabel);
  }

  return Response::OK();
}

void TracingHandler::GetCategories(
    std::unique_ptr<GetCategoriesCallback> callback) {
  TracingController::GetInstance()->GetCategories(
      base::BindOnce(&TracingHandler::OnCategoriesReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void TracingHandler::OnRecordingEnabled(
    std::unique_ptr<StartCallback> callback) {
  if (!did_initiate_recording_) {
    callback->sendFailure(Response::Error(
        "Tracing was stopped before start has been completed."));
    return;
  }

  EmitFrameTree();
  callback->sendSuccess();

  bool screenshot_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"), &screenshot_enabled);
  if (video_consumer_ && screenshot_enabled) {
    // Reset number of screenshots received, each time tracing begins.
    number_of_screenshots_from_video_consumer_ = 0;
    video_consumer_->SetMinCapturePeriod(kMinCapturePeriod);
    video_consumer_->SetMinAndMaxFrameSize(kMinFrameSize, kMaxFrameSize);
    video_consumer_->StartCapture();
  }
}

void TracingHandler::OnBufferUsage(float percent_full,
                                   size_t approximate_event_count) {
  if (!did_initiate_recording_)
    return;
  // TODO(crbug426117): remove set_value once all clients have switched to
  // the new interface of the event.
  frontend_->BufferUsage(percent_full, approximate_event_count, percent_full);
}

void TracingHandler::OnCategoriesReceived(
    std::unique_ptr<GetCategoriesCallback> callback,
    const std::set<std::string>& category_set) {
  std::unique_ptr<protocol::Array<std::string>> categories =
      protocol::Array<std::string>::create();
  for (const std::string& category : category_set)
    categories->addItem(category);
  callback->sendSuccess(std::move(categories));
}

void TracingHandler::RequestMemoryDump(
    std::unique_ptr<RequestMemoryDumpCallback> callback) {
  if (!IsTracing()) {
    callback->sendFailure(Response::Error("Tracing is not started"));
    return;
  }

  auto on_memory_dump_finished =
      base::BindOnce(&TracingHandler::OnMemoryDumpFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDumpAndAppendToTrace(
          base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
          base::trace_event::MemoryDumpLevelOfDetail::DETAILED,
          std::move(on_memory_dump_finished));
}

void TracingHandler::OnMemoryDumpFinished(
    std::unique_ptr<RequestMemoryDumpCallback> callback,
    bool success,
    uint64_t dump_id) {
  callback->sendSuccess(base::StringPrintf("0x%" PRIx64, dump_id), success);
}

void TracingHandler::OnFrameFromVideoConsumer(
    scoped_refptr<media::VideoFrame> frame) {
  const SkBitmap skbitmap = DevToolsVideoConsumer::GetSkBitmapFromFrame(frame);

  base::TimeTicks reference_time;
  const bool had_reference_time = frame->metadata()->GetTimeTicks(
      media::VideoFrameMetadata::REFERENCE_TIME, &reference_time);
  DCHECK(had_reference_time);

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID_AND_TIMESTAMP(
      TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"), "Screenshot", 1,
      reference_time, std::make_unique<DevToolsTraceableScreenshot>(skbitmap));

  ++number_of_screenshots_from_video_consumer_;
  DCHECK(video_consumer_);
  if (number_of_screenshots_from_video_consumer_ >=
      DevToolsFrameTraceRecorder::kMaximumNumberOfScreenshots) {
    video_consumer_->StopCapture();
  }
}

Response TracingHandler::RecordClockSyncMarker(const std::string& sync_id) {
  if (!IsTracing())
    return Response::Error("Tracing is not started");
  TRACE_EVENT_CLOCK_SYNC_RECEIVER(sync_id);
  return Response::OK();
}

void TracingHandler::SetupTimer(double usage_reporting_interval) {
  if (usage_reporting_interval == 0) return;

  if (usage_reporting_interval < kMinimumReportingInterval)
      usage_reporting_interval = kMinimumReportingInterval;

  base::TimeDelta interval = base::TimeDelta::FromMilliseconds(
      std::ceil(usage_reporting_interval));
  buffer_usage_poll_timer_.reset(new base::RepeatingTimer());
  buffer_usage_poll_timer_->Start(
      FROM_HERE, interval,
      base::Bind(base::IgnoreResult(&TracingController::GetTraceBufferUsage),
                 base::Unretained(TracingController::GetInstance()),
                 base::Bind(&TracingHandler::OnBufferUsage,
                            weak_factory_.GetWeakPtr())));
}

void TracingHandler::StopTracing(
    const scoped_refptr<TracingController::TraceDataEndpoint>& endpoint,
    const std::string& agent_label) {
  buffer_usage_poll_timer_.reset();
  TracingController::GetInstance()->StopTracing(endpoint, agent_label);
  did_initiate_recording_ = false;
  if (video_consumer_)
    video_consumer_->StopCapture();
}

bool TracingHandler::IsTracing() const {
  return TracingController::GetInstance()->IsTracing();
}

void TracingHandler::EmitFrameTree() {
  auto data = std::make_unique<base::trace_event::TracedValue>();
  if (frame_tree_node_) {
    data->SetInteger("frameTreeNodeId", frame_tree_node_->frame_tree_node_id());
    data->SetBoolean("persistentIds", true);
    data->BeginArray("frames");
    FrameTree::NodeRange subtree =
        frame_tree_node_->frame_tree()->SubtreeNodes(frame_tree_node_);
    for (FrameTreeNode* node : subtree) {
      data->BeginDictionary();
      FillFrameData(data.get(), node, node->current_frame_host(),
                    node->current_url());
      data->EndDictionary();
    }
    data->EndArray();
  }
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "TracingStartedInBrowser", TRACE_EVENT_SCOPE_THREAD,
                       "data", std::move(data));
}

void TracingHandler::ReadyToCommitNavigation(
    NavigationHandleImpl* navigation_handle) {
  if (!did_initiate_recording_)
    return;
  auto data = std::make_unique<base::trace_event::TracedValue>();
  FillFrameData(data.get(), navigation_handle->frame_tree_node(),
                navigation_handle->GetRenderFrameHost(),
                navigation_handle->GetURL());
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "FrameCommittedInBrowser", TRACE_EVENT_SCOPE_THREAD,
                       "data", std::move(data));

  SetupProcessFilter(base::kNullProcessId,
                     navigation_handle->GetRenderFrameHost());
  TracingController::GetInstance()->StartTracing(
      trace_config_, TracingController::StartTracingDoneCallback());
}

void TracingHandler::FrameDeleted(RenderFrameHostImpl* frame_host) {
  if (!did_initiate_recording_)
    return;
  auto data = std::make_unique<base::trace_event::TracedValue>();
  data->SetString(
      "frame",
      frame_host->frame_tree_node()->devtools_frame_token().ToString());
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "FrameDeletedInBrowser", TRACE_EVENT_SCOPE_THREAD,
                       "data", std::move(data));
}

// static
bool TracingHandler::IsStartupTracingActive() {
  return ::tracing::TraceStartupConfig::GetInstance()->IsEnabled();
}

// static
base::trace_event::TraceConfig TracingHandler::GetTraceConfigFromDevToolsConfig(
    const base::DictionaryValue& devtools_config) {
  std::unique_ptr<base::Value> value = ConvertDictKeyStyle(devtools_config);
  DCHECK(value && value->is_dict());
  std::unique_ptr<base::DictionaryValue> tracing_dict(
      static_cast<base::DictionaryValue*>(value.release()));

  std::string mode;
  if (tracing_dict->GetString(kRecordModeParam, &mode))
    tracing_dict->SetString(kRecordModeParam, ConvertFromCamelCase(mode, '-'));

  return base::trace_event::TraceConfig(*tracing_dict);
}

}  // namespace tracing
}  // namespace protocol
