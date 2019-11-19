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
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
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

namespace content {
namespace {

void OnGotCategories(const WebUIDataSource::GotDataCallback& callback,
                     const std::set<std::string>& categorySet) {
  base::ListValue category_list;
  for (auto it = categorySet.begin(); it != categorySet.end(); it++) {
    category_list.AppendString(*it);
  }

  scoped_refptr<base::RefCountedString> res(new base::RefCountedString());
  base::JSONWriter::Write(category_list, &res->data());
  callback.Run(res);
}

void OnRecordingEnabledAck(const WebUIDataSource::GotDataCallback& callback);

bool BeginRecording(const std::string& data64,
                    const WebUIDataSource::GotDataCallback& callback) {
  base::trace_event::TraceConfig trace_config("", "");
  if (!TracingUI::GetTracingOptions(data64, &trace_config))
    return false;

  return TracingController::GetInstance()->StartTracing(
      trace_config, base::BindOnce(&OnRecordingEnabledAck, callback));
}

void OnRecordingEnabledAck(const WebUIDataSource::GotDataCallback& callback) {
  callback.Run(
      scoped_refptr<base::RefCountedMemory>(new base::RefCountedString()));
}

void OnTraceBufferUsageResult(const WebUIDataSource::GotDataCallback& callback,
                              float percent_full,
                              size_t approximate_event_count) {
  std::string str = base::NumberToString(percent_full);
  callback.Run(base::RefCountedString::TakeString(&str));
}

void OnTraceBufferStatusResult(const WebUIDataSource::GotDataCallback& callback,
                               float percent_full,
                               size_t approximate_event_count) {
  base::DictionaryValue status;
  status.SetDouble("percentFull", percent_full);
  status.SetInteger("approximateEventCount", approximate_event_count);

  std::string status_json;
  base::JSONWriter::Write(status, &status_json);

  base::RefCountedString* status_base64 = new base::RefCountedString();
  base::Base64Encode(status_json, &status_base64->data());
  callback.Run(status_base64);
}

void TracingCallbackWrapperBase64(
    const WebUIDataSource::GotDataCallback& callback,
    std::unique_ptr<std::string> data) {
  base::RefCountedString* data_base64 = new base::RefCountedString();
  base::Base64Encode(*data, &data_base64->data());
  callback.Run(data_base64);
}

bool OnBeginJSONRequest(const std::string& path,
                        const WebUIDataSource::GotDataCallback& callback) {
  if (path == "json/categories") {
    return TracingController::GetInstance()->GetCategories(
        base::BindOnce(OnGotCategories, callback));
  }

  const char kBeginRecordingPath[] = "json/begin_recording?";
  if (base::StartsWith(path, kBeginRecordingPath,
                       base::CompareCase::SENSITIVE)) {
    std::string data = path.substr(strlen(kBeginRecordingPath));
    return BeginRecording(data, callback);
  }
  if (path == "json/get_buffer_percent_full") {
    return TracingController::GetInstance()->GetTraceBufferUsage(
        base::BindOnce(OnTraceBufferUsageResult, callback));
  }
  if (path == "json/get_buffer_status") {
    return TracingController::GetInstance()->GetTraceBufferUsage(
        base::BindOnce(OnTraceBufferStatusResult, callback));
  }
  if (path == "json/end_recording_compressed") {
    if (!TracingController::GetInstance()->IsTracing())
      return false;
    scoped_refptr<TracingController::TraceDataEndpoint> data_endpoint =
        TracingControllerImpl::CreateCompressedStringEndpoint(
            TracingControllerImpl::CreateCallbackEndpoint(
                base::Bind(TracingCallbackWrapperBase64, callback)),
            false /* compress_with_background_priority */);
    return TracingController::GetInstance()->StopTracing(data_endpoint);
  }

  LOG(ERROR) << "Unhandled request to " << path;
  return false;
}

bool OnShouldHandleRequest(const std::string& path) {
  return base::StartsWith(path, "json/", base::CompareCase::SENSITIVE);
}

void OnTracingRequest(const std::string& path,
                      const WebUIDataSource::GotDataCallback& callback) {
  DCHECK(OnShouldHandleRequest(path));
  if (!OnBeginJSONRequest(path, callback)) {
    std::string error("##ERROR##");
    callback.Run(base::RefCountedString::TakeString(&error));
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
  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  WebUIDataSource* source = WebUIDataSource::Create(kChromeUITracingHost);
  source->UseStringsJs();
  source->SetDefaultResource(IDR_TRACING_HTML);
  source->AddResourcePath("tracing.js", IDR_TRACING_JS);
  source->SetRequestFilter(base::BindRepeating(OnShouldHandleRequest),
                           base::BindRepeating(OnTracingRequest));
  WebUIDataSource::Add(browser_context, source);
}

TracingUI::~TracingUI() = default;

// static
bool TracingUI::GetTracingOptions(
    const std::string& data64,
    base::trace_event::TraceConfig* trace_config) {
  std::string data;
  if (!base::Base64Decode(data64, &data)) {
    LOG(ERROR) << "Options were not base64 encoded.";
    return false;
  }

  std::unique_ptr<base::Value> optionsRaw =
      base::JSONReader::ReadDeprecated(data);
  if (!optionsRaw) {
    LOG(ERROR) << "Options were not valid JSON";
    return false;
  }
  base::DictionaryValue* options;
  if (!optionsRaw->GetAsDictionary(&options)) {
    LOG(ERROR) << "Options must be dict";
    return false;
  }

  if (!trace_config) {
    LOG(ERROR) << "trace_config can't be passed as NULL";
    return false;
  }

  // New style options dictionary.
  if (options->HasKey("included_categories")) {
    *trace_config = base::trace_event::TraceConfig(*options);
    return true;
  }

  bool options_ok = true;
  std::string category_filter_string;
  options_ok &= options->GetString("categoryFilter", &category_filter_string);

  std::string record_mode;
  options_ok &= options->GetString("tracingRecordMode", &record_mode);

  *trace_config =
      base::trace_event::TraceConfig(category_filter_string, record_mode);

  bool enable_systrace;
  options_ok &= options->GetBoolean("useSystemTracing", &enable_systrace);
  if (enable_systrace)
    trace_config->EnableSystrace();

  if (!options_ok) {
    LOG(ERROR) << "Malformed options";
    return false;
  }
  return true;
}

}  // namespace content
