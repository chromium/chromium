// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_ui.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "content/browser/tracing/grit/tracing_resources.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_session.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/protos/perfetto/common/trace_stats.gen.h"

namespace content {
namespace {
constexpr char kStreamFormat[] = "stream_format";
constexpr char kStreamFormatProtobuf[] = "protobuf";
constexpr char kStreamFormatJSON[] = "json";
perfetto::TracingSession* g_tracing_session = nullptr;

void OnGotCategories(WebUIDataSource::GotDataCallback callback,
                     const std::set<std::string>& categorySet) {
  base::ListValue category_list;
  for (auto it = categorySet.begin(); it != categorySet.end(); it++) {
    category_list.Append(*it);
  }

  scoped_refptr<base::RefCountedString> res(new base::RefCountedString());
  base::JSONWriter::Write(category_list, &res->data());
  std::move(callback).Run(res);
}

void OnRecordingEnabledAck(WebUIDataSource::GotDataCallback callback);

bool BeginRecording(const std::string& data64,
                    WebUIDataSource::GotDataCallback callback) {
  base::trace_event::TraceConfig trace_config("", "");
  std::string stream_format;
  if (!TracingUI::GetTracingOptions(data64, trace_config, stream_format))
    return false;

  // TODO(skyostil): Migrate all use cases from TracingController to Perfetto.
  if (stream_format == kStreamFormatProtobuf) {
    if (g_tracing_session)
      delete g_tracing_session;
    g_tracing_session =
        perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend)
            .release();
    g_tracing_session->Setup(tracing::GetDefaultPerfettoConfig(trace_config));

    auto shared_callback = base::MakeRefCounted<
        base::RefCountedData<WebUIDataSource::GotDataCallback>>(
        std::move(callback));
    g_tracing_session->SetOnStartCallback([shared_callback] {
      OnRecordingEnabledAck(std::move(shared_callback->data));
    });
    g_tracing_session->Start();
    return true;
  }
  return TracingController::GetInstance()->StartTracing(
      trace_config,
      base::BindOnce(&OnRecordingEnabledAck, std::move(callback)));
}

void OnTraceBufferUsageResult(WebUIDataSource::GotDataCallback callback,
                              float percent_full,
                              size_t approximate_event_count) {
  std::string str = base::NumberToString(percent_full);
  std::move(callback).Run(base::RefCountedString::TakeString(&str));
}

bool GetTraceBufferUsage(WebUIDataSource::GotDataCallback callback) {
  if (g_tracing_session) {
    // |callback| is move-only, so in order to pass it through a copied lambda
    // we need to temporarily move it on the heap.
    auto shared_callback = base::MakeRefCounted<
        base::RefCountedData<WebUIDataSource::GotDataCallback>>(
        std::move(callback));
    g_tracing_session->GetTraceStats(
        [shared_callback](
            perfetto::TracingSession::GetTraceStatsCallbackArgs args) {
          std::string usage;
          perfetto::protos::gen::TraceStats trace_stats;
          if (args.success &&
              trace_stats.ParseFromArray(args.trace_stats_data.data(),
                                         args.trace_stats_data.size())) {
            double percent_full = tracing::GetTraceBufferUsage(trace_stats);
            usage = base::NumberToString(percent_full);
          }
          std::move(shared_callback->data)
              .Run(base::RefCountedString::TakeString(&usage));
        });
    return true;
  }

  return TracingController::GetInstance()->GetTraceBufferUsage(
      base::BindOnce(OnTraceBufferUsageResult, std::move(callback)));
}

void ReadProtobufTraceData(
    scoped_refptr<TracingController::TraceDataEndpoint> endpoint,
    perfetto::TracingSession::ReadTraceCallbackArgs args) {
  if (args.size) {
    auto data_string = std::make_unique<std::string>(args.data, args.size);
    endpoint->ReceiveTraceChunk(std::move(data_string));
  }
  if (!args.has_more)
    endpoint->ReceivedTraceFinalContents();
}

void TracingCallbackWrapperBase64(WebUIDataSource::GotDataCallback callback,
                                  std::unique_ptr<std::string> data) {
  base::RefCountedString* data_base64 = new base::RefCountedString();
  base::Base64Encode(*data, &data_base64->data());
  std::move(callback).Run(data_base64);
}

bool EndRecording(WebUIDataSource::GotDataCallback callback) {
  if (!TracingController::GetInstance()->IsTracing() && !g_tracing_session)
    return false;

  scoped_refptr<TracingController::TraceDataEndpoint> data_endpoint =
      TracingControllerImpl::CreateCompressedStringEndpoint(
          TracingControllerImpl::CreateCallbackEndpoint(base::BindOnce(
              TracingCallbackWrapperBase64, std::move(callback))),
          false /* compress_with_background_priority */);

  if (g_tracing_session) {
    perfetto::TracingSession* session = g_tracing_session;
    g_tracing_session = nullptr;
    session->SetOnStopCallback([session, data_endpoint] {
      session->ReadTrace(
          [session, data_endpoint](
              perfetto::TracingSession::ReadTraceCallbackArgs args) {
            ReadProtobufTraceData(data_endpoint, args);
            if (!args.has_more)
              delete session;
          });
    });
    session->Stop();
    return true;
  }
  return TracingController::GetInstance()->StopTracing(data_endpoint);
}

void OnRecordingEnabledAck(WebUIDataSource::GotDataCallback callback) {
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>());
}

bool OnBeginJSONRequest(const std::string& path,
                        WebUIDataSource::GotDataCallback callback) {
  if (path == "json/categories") {
    return TracingController::GetInstance()->GetCategories(
        base::BindOnce(OnGotCategories, std::move(callback)));
  }

  const char kBeginRecordingPath[] = "json/begin_recording?";
  if (base::StartsWith(path, kBeginRecordingPath,
                       base::CompareCase::SENSITIVE)) {
    std::string data = path.substr(strlen(kBeginRecordingPath));
    return BeginRecording(data, std::move(callback));
  }
  if (path == "json/get_buffer_percent_full") {
    return GetTraceBufferUsage(std::move(callback));
  }
  if (path == "json/end_recording_compressed") {
    return EndRecording(std::move(callback));
  }

  LOG(ERROR) << "Unhandled request to " << path;
  return false;
}

bool OnShouldHandleRequest(const std::string& path) {
  return base::StartsWith(path, "json/", base::CompareCase::SENSITIVE);
}

void OnTracingRequest(const std::string& path,
                      WebUIDataSource::GotDataCallback callback) {
  DCHECK(OnShouldHandleRequest(path));
  // OnBeginJSONRequest() only runs |callback| if it returns true. But it needs
  // to take ownership of |callback| even though it won't call |callback|
  // sometimes, as it binds |callback| into other callbacks before it makes that
  // decision. So we must give it one copy and keep one ourselves.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  if (!OnBeginJSONRequest(path, std::move(split_callback.first))) {
    std::string error("##ERROR##");
    std::move(split_callback.second)
        .Run(base::RefCountedString::TakeString(&error));
  }
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// TracingUI
//
////////////////////////////////////////////////////////////////////////////////

TracingUI::TracingUI(WebUI* web_ui)
    : WebUIController(web_ui),
      delegate_(GetContentClient()->browser()->GetTracingDelegate()) {
  // Set up the chrome://tracing/ source.
  WebUIDataSource* source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUITracingHost);
  source->DisableTrustedTypesCSP();
  source->UseStringsJs();
  source->SetDefaultResource(IDR_TRACING_ABOUT_TRACING_HTML);
  source->AddResourcePath("tracing.js", IDR_TRACING_ABOUT_TRACING_JS);

  source->SetRequestFilter(base::BindRepeating(OnShouldHandleRequest),
                           base::BindRepeating(OnTracingRequest));
}

TracingUI::~TracingUI() = default;

// static
bool TracingUI::GetTracingOptions(const std::string& data64,
                                  base::trace_event::TraceConfig& trace_config,
                                  std::string& out_stream_format) {
  std::string data;
  if (!base::Base64Decode(data64, &data)) {
    LOG(ERROR) << "Options were not base64 encoded.";
    return false;
  }

  absl::optional<base::Value> options = base::JSONReader::Read(data);
  if (!options) {
    LOG(ERROR) << "Options were not valid JSON";
    return false;
  }
  if (!options->is_dict()) {
    LOG(ERROR) << "Options must be dict";
    return false;
  }

  if (const std::string* stream_format =
          options->FindStringKey(kStreamFormat)) {
    out_stream_format = *stream_format;
  } else {
    out_stream_format = kStreamFormatJSON;
  }

  // New style options dictionary.
  trace_config = base::trace_event::TraceConfig(*options);
  return true;
}

}  // namespace content
