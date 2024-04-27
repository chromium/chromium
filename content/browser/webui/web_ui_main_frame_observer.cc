// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_main_frame_observer.h"

#include <string>
#include <utility>

#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_ui_controller.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"  // nogncheck
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"  // nogncheck
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#endif

namespace content {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Remove the pieces of the URL we don't want to send back with the error
// reports. In particular, do not send query or fragments as those can have
// privacy-sensitive information in them.
std::string RedactURL(const GURL& url) {
  std::string redacted_url = url.DeprecatedGetOriginAsURL().spec();
  // Path will start with / and GetOrigin ends with /. Cut one / to avoid
  // chrome://discards//graph.
  if (!redacted_url.empty() && redacted_url.back() == '/') {
    redacted_url.pop_back();
  }
  base::StrAppend(&redacted_url, {url.path_piece()});
  return redacted_url;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace

WebUIMainFrameObserver::WebUIMainFrameObserver(WebUIImpl* web_ui,
                                               WebContents* contents)
    : WebContentsObserver(contents), web_ui_(web_ui) {}

WebUIMainFrameObserver::~WebUIMainFrameObserver() = default;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void WebUIMainFrameObserver::OnDidAddMessageToConsole(
    RenderFrameHost* source_frame,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id,
    const std::optional<std::u16string>& untrusted_stack_trace) {
  DVLOG(3) << "OnDidAddMessageToConsole called for " << message;
  if (untrusted_stack_trace) {
    DVLOG(3) << "stack is " << *untrusted_stack_trace;
  }

  if (!error_reporting_enabled_) {
    DVLOG(3) << "Message not reported, error reporting disabled for this page "
                "or experiment is off";
    return;
  }

  if (log_level != blink::mojom::ConsoleMessageLevel::kError) {
    DVLOG(3) << "Message not reported, not an error";
    return;
  }

  // Some WebUI pages have another WebUI page in an <iframe>. Both
  // WebUIMainFrameObservers will get a callback when either page gets an error.
  // To avoid duplicates, only report on errors from this page's frame.
  if (source_frame != web_ui_->GetRenderFrameHost()) {
    DVLOG(3) << "Message not reported, different frame";
    return;
  }

  scoped_refptr<JsErrorReportProcessor> processor =
      JsErrorReportProcessor::Get();

  if (!processor) {
    // This usually means we are not on an official Google build, FYI.
    DVLOG(3) << "Message not reported, no processor";
    return;
  }

  // Redact query parameters & fragment. Also the username and password.
  // TODO(crbug.com/40146362) Improve redaction.
  GURL url(source_id);
  if (!url.is_valid()) {
    DVLOG(3) << "Message not reported, invalid URL";
    return;
  }

  // If this is not a chrome:// page, do not report the error. In particular,
  // some WebUIs use chrome-untrusted:// to host pages with some
  // not-controlled-by-Chrome content. The code must not send reports for such
  // content because Chrome cannot control what information is being included in
  // the reports.
  if (!url.SchemeIs(kChromeUIScheme)) {
    DVLOG(3) << "Message not reported, not a chrome:// URL";
    return;
  }

  JavaScriptErrorReport report;
  report.message = base::UTF16ToUTF8(message);
  report.line_number = line_no;
  report.url = RedactURL(url);
  report.source_system = JavaScriptErrorReport::SourceSystem::kWebUIObserver;
  if (untrusted_stack_trace) {
    report.stack_trace = base::UTF16ToUTF8(*untrusted_stack_trace);
  }

  GURL page_url = source_frame->GetLastCommittedURL();
  if (page_url.is_valid()) {
    report.page_url = RedactURL(page_url);
  }

  DVLOG(3) << "Error being sent to Google";
  processor->SendErrorReport(std::move(report), base::DoNothing(),
                             web_contents()->GetBrowserContext());
}

void WebUIMainFrameObserver::MaybeEnableWebUIJavaScriptErrorReporting(
    NavigationHandle* navigation_handle) {
  error_reporting_enabled_ =
      web_ui_->GetController()->IsJavascriptErrorReportingEnabled();

  // If we are collecting error reports, make sure the main frame sends us
  // stacks along with those messages. Warning: Don't call
  // RenderFrameHostImpl::SetWantErrorMessageStackTrace() before the remote
  // frame is created, or it will lock up the communications channel. (See
  // https://crbug.com/1154866).
  if (error_reporting_enabled_) {
    DVLOG(3) << "Enabled";
    static_cast<content::RenderFrameHostImpl*>(web_ui_->GetRenderFrameHost())
        ->SetWantErrorMessageStackTrace();
  } else {
    DVLOG(3) << "Error reporting disabled for this page";
  }
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

void WebUIMainFrameObserver::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  // Navigation didn't occur in the frame associated with this WebUI.
  if (navigation_handle->GetRenderFrameHost() !=
      web_ui_->GetRenderFrameHost()) {
    return;
  }

  web_ui_->GetController()->WebUIReadyToCommitNavigation(
      web_ui_->GetRenderFrameHost());

// TODO(crbug.com/40149439) This is currently disabled due to Windows DLL
// thunking issues. Fix & re-enable.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  MaybeEnableWebUIJavaScriptErrorReporting(navigation_handle);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

void WebUIMainFrameObserver::PrimaryPageChanged(Page& page) {
  web_ui_->DisallowJavascriptOnAllHandlers();
  web_ui_->GetController()->WebUIPrimaryPageChanged(page);
}

}  // namespace content
