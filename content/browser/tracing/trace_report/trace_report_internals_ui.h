// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_REPORT_INTERNALS_UI_H_
#define CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_REPORT_INTERNALS_UI_H_

#include "content/browser/tracing/trace_report/trace_report.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace content {

class RenderFrameHost;
class TraceReportHandler;
class TraceReportInternalsUI;

// WebUIConfig for the chrome://traces page.
class TraceReportInternalsUIConfig
    : public DefaultWebUIConfig<TraceReportInternalsUI> {
 public:
  TraceReportInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kChromeUITracesInternalsHost) {}
};

// WebUIController for the chrome://traces page.
class CONTENT_EXPORT TraceReportInternalsUI
    : public WebUIController,
      public trace_report::mojom::TraceReportHandlerFactory {
 public:
  explicit TraceReportInternalsUI(content::WebUI* web_ui);
  ~TraceReportInternalsUI() override;

  TraceReportInternalsUI(const TraceReportInternalsUI&) = delete;
  TraceReportInternalsUI& operator=(const TraceReportInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<trace_report::mojom::TraceReportHandlerFactory>
          receiver);

  // WebUIController:
  void WebUIRenderFrameCreated(RenderFrameHost* rfh) override;

 private:
  // trace_report::mojom::PageHandlerFactory:
  void CreatePageHandler(mojo::PendingRemote<trace_report::mojom::Page> page,
                         mojo::PendingReceiver<trace_report::mojom::PageHandler>
                             receiver) override;

  std::unique_ptr<TraceReportHandler> ui_handler_;
  mojo::Receiver<trace_report::mojom::TraceReportHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_REPORT_INTERNALS_UI_H_
