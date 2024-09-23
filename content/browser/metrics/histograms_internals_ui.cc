// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/metrics/histograms_internals_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/values.h"
#include "content/browser/metrics/histogram_synchronizer.h"
#include "content/browser/metrics/histograms_monitor.h"
#include "content/grit/histograms_resources.h"
#include "content/grit/histograms_resources_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace content {
namespace {

const char kHistogramsUIRequestHistograms[] = "requestHistograms";
const char kHistogramsUIStartMonitoring[] = "startMonitoring";
const char kHistogramsUIFetchDiff[] = "fetchDiff";

// Stores the information received from Javascript side.
struct JsParams {
  std::string callback_id;
  std::string query;
  bool include_subprocesses;
};

void CreateAndAddHistogramsHTMLSource(BrowserContext* browser_context) {
  WebUIDataSource* source =
      WebUIDataSource::CreateAndAdd(browser_context, kChromeUIHistogramHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  source->AddResourcePaths(
      base::make_span(kHistogramsResources, kHistogramsResourcesSize));
  source->SetDefaultResource(IDR_HISTOGRAMS_HISTOGRAMS_INTERNALS_HTML);
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class HistogramsMessageHandler : public WebUIMessageHandler {
 public:
  HistogramsMessageHandler();

  HistogramsMessageHandler(const HistogramsMessageHandler&) = delete;
  HistogramsMessageHandler& operator=(const HistogramsMessageHandler&) = delete;

  ~HistogramsMessageHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleRequestHistograms(const base::Value::List& args);
  void HandleStartMoninoring(const base::Value::List& args);
  void HandleFetchDiff(const base::Value::List& args);

  // Calls AllowJavascript() and unpacks the passed params.
  JsParams AllowJavascriptAndUnpackParams(const base::Value::List& args);

  // Import histograms, and those from subprocesses if |include_subprocesses| is
  // true.
  void ImportHistograms(bool include_subprocesses);

  HistogramsMonitor histogram_monitor_;
};

HistogramsMessageHandler::HistogramsMessageHandler() = default;

HistogramsMessageHandler::~HistogramsMessageHandler() = default;

JsParams HistogramsMessageHandler::AllowJavascriptAndUnpackParams(
    const base::Value::List& args_list) {
  AllowJavascript();
  JsParams params;
  if (args_list.size() > 0u && args_list[0].is_string())
    params.callback_id = args_list[0].GetString();
  if (args_list.size() > 1u && args_list[1].is_string())
    params.query = args_list[1].GetString();
  if (args_list.size() > 2u && args_list[2].is_bool())
    params.include_subprocesses = args_list[2].GetBool();
  return params;
}

void HistogramsMessageHandler::ImportHistograms(bool include_subprocesses) {
  if (include_subprocesses) {
    // Synchronously fetch subprocess histograms that live in shared memory.
    base::StatisticsRecorder::ImportProvidedHistogramsSync();

    // Asynchronously fetch subprocess histograms that do not live in shared
    // memory (e.g., they were emitted before the shared memory was set up).
    HistogramSynchronizer::FetchHistograms();
  }
}

void HistogramsMessageHandler::HandleRequestHistograms(
    const base::Value::List& args) {
  JsParams params = AllowJavascriptAndUnpackParams(args);
  ImportHistograms(params.include_subprocesses);
  base::Value::List histograms_list;
  for (base::HistogramBase* histogram :
       base::StatisticsRecorder::Sort(base::StatisticsRecorder::WithName(
           base::StatisticsRecorder::GetHistograms(), params.query,
           /*case_sensitive=*/false))) {
    base::Value::Dict histogram_dict = histogram->ToGraphDict();
    if (!histogram_dict.empty())
      histograms_list.Append(std::move(histogram_dict));
  }

  ResolveJavascriptCallback(base::Value(params.callback_id), histograms_list);
}

void HistogramsMessageHandler::HandleStartMoninoring(
    const base::Value::List& args) {
  JsParams params = AllowJavascriptAndUnpackParams(args);
  ImportHistograms(params.include_subprocesses);
  histogram_monitor_.StartMonitoring(params.query);
  ResolveJavascriptCallback(base::Value(params.callback_id),
                            base::Value("Success"));
}

void HistogramsMessageHandler::HandleFetchDiff(const base::Value::List& args) {
  JsParams params = AllowJavascriptAndUnpackParams(args);
  ImportHistograms(params.include_subprocesses);
  base::Value::List histograms_list = histogram_monitor_.GetDiff();
  ResolveJavascriptCallback(base::Value(params.callback_id),
                            std::move(histograms_list));
}

void HistogramsMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We can use base::Unretained() here, as both the callback and this class are
  // owned by HistogramsInternalsUI.
  web_ui()->RegisterMessageCallback(
      kHistogramsUIRequestHistograms,
      base::BindRepeating(&HistogramsMessageHandler::HandleRequestHistograms,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      kHistogramsUIStartMonitoring,
      base::BindRepeating(&HistogramsMessageHandler::HandleStartMoninoring,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      kHistogramsUIFetchDiff,
      base::BindRepeating(&HistogramsMessageHandler::HandleFetchDiff,
                          base::Unretained(this)));
}

}  // namespace

HistogramsInternalsUI::HistogramsInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<HistogramsMessageHandler>());

  // Set up the chrome://histograms/ source.
  CreateAndAddHistogramsHTMLSource(
      web_ui->GetWebContents()->GetBrowserContext());
}

}  // namespace content
