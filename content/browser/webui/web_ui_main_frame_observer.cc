// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_main_frame_observer.h"

#include <string>
#include <utility>

#include "content/browser/webui/web_ui_impl.h"
#include "content/public/browser/navigation_handle.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "url/gurl.h"
#endif

namespace content {
WebUIMainFrameObserver::WebUIMainFrameObserver(WebUIImpl* web_ui,
                                               WebContents* contents)
    : WebContentsObserver(contents), web_ui_(web_ui) {}
WebUIMainFrameObserver::~WebUIMainFrameObserver() = default;

void WebUIMainFrameObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // Only disallow JavaScript on cross-document navigations in the main frame.
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  web_ui_->DisallowJavascriptOnAllHandlers();
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
void WebUIMainFrameObserver::OnDidAddMessageToConsole(
    RenderFrameHost* source_frame,
    blink::mojom::ConsoleMessageLevel log_level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  VLOG(3) << "OnDidAddMessageToConsole called for " << message;
  if (log_level != blink::mojom::ConsoleMessageLevel::kError) {
    VLOG(3) << "Message not reported, not an error";
    return;
  }

  if (!base::FeatureList::IsEnabled(
          features::kSendWebUIJavaScriptErrorReports)) {
    VLOG(3) << "Message not reported, error report sending flag off";
    return;
  }

  scoped_refptr<JsErrorReportProcessor> processor =
      JsErrorReportProcessor::Get();

  if (!processor) {
    // This usually means we are not on an official Google build, FYI.
    VLOG(3) << "Message not reported, no processor";
    return;
  }

  // Redact query parameters & fragment. Also the username and password.
  // TODO(https://crbug.com/1121816) Improve redaction.
  GURL url(source_id);
  if (!url.is_valid()) {
    VLOG(3) << "Message not reported, invalid URL";
    return;
  }
  std::string redacted_url = url.GetOrigin().spec();
  // Path will start with / and GetOrigin ends with /. Cut one / to avoid
  // chrome://discards//graph.
  if (!redacted_url.empty() && redacted_url.back() == '/') {
    redacted_url.pop_back();
  }
  base::StrAppend(&redacted_url, {url.path_piece()});

  // TODO(https://crbug.com/1121816): Don't send error reports for subframes in
  // most cases.

  // TODO(https://crbug.com/1121816): Don't send error reports for non-chrome://
  // URLs in most cases.

  JavaScriptErrorReport report;
  report.message = base::UTF16ToUTF8(message);
  report.line_number = line_no;
  report.url = std::move(redacted_url);
  report.send_to_production_servers =
      features::kWebUIJavaScriptErrorReportsSendToProductionParam.Get();

  VLOG(3) << "Error being sent to Google";
  processor->SendErrorReport(std::move(report), base::DoNothing(),
                             web_contents()->GetBrowserContext());
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

}  // namespace content
