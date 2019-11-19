// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/histograms_internals_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/values.h"
#include "content/browser/histogram_synchronizer.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"

namespace content {
namespace {

const char kHistogramsUIJs[] = "histograms_internals.js";
const char kHistogramsUIRequestHistograms[] = "requestHistograms";

WebUIDataSource* CreateHistogramsHTMLSource() {
  WebUIDataSource* source = WebUIDataSource::Create(kChromeUIHistogramHost);

  source->AddResourcePath(kHistogramsUIJs, IDR_HISTOGRAMS_INTERNALS_JS);
  source->SetDefaultResource(IDR_HISTOGRAMS_INTERNALS_HTML);
  return source;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class HistogramsMessageHandler : public WebUIMessageHandler {
 public:
  HistogramsMessageHandler();
  ~HistogramsMessageHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleRequestHistograms(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(HistogramsMessageHandler);
};

HistogramsMessageHandler::HistogramsMessageHandler() {}

HistogramsMessageHandler::~HistogramsMessageHandler() {}

void HistogramsMessageHandler::HandleRequestHistograms(
    const base::ListValue* args) {
  base::StatisticsRecorder::ImportProvidedHistograms();
  HistogramSynchronizer::FetchHistograms();

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  std::string query;
  args->GetString(1, &query);

  base::ListValue histograms_list;
  for (base::HistogramBase* histogram :
       base::StatisticsRecorder::Sort(base::StatisticsRecorder::WithName(
           base::StatisticsRecorder::GetHistograms(), query))) {
    // TODO(crbug.com/809820): Return the histogram object as a DictionaryValue
    // for better UI that is built client side.
    std::string ascii_output;
    histogram->WriteHTMLGraph(&ascii_output);
    ascii_output += "<br><hr><br>";
    histograms_list.Append(std::move(ascii_output));
  }

  ResolveJavascriptCallback(base::Value(callback_id),
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
}

}  // namespace

HistogramsInternalsUI::HistogramsInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<HistogramsMessageHandler>());

  // Set up the chrome://histograms/ source.
  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, CreateHistogramsHTMLSource());
}

}  // namespace content
