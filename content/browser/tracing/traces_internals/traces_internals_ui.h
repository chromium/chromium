// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACES_INTERNALS_TRACES_INTERNALS_UI_H_
#define CONTENT_BROWSER_TRACING_TRACES_INTERNALS_TRACES_INTERNALS_UI_H_

#include "content/browser/tracing/traces_internals/traces_internals.mojom.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class RenderFrameHost;
class TracesInternalsHandler;
class TracesInternalsUI;

// WebUIConfig for the chrome://traces page.
class TracesInternalsUIConfig
    : public DefaultInternalWebUIConfig<TracesInternalsUI> {
 public:
  TracesInternalsUIConfig()
      : DefaultInternalWebUIConfig(kChromeUITracesInternalsHost) {}
};

// Temporary WebUIConfig to also register legacy chrome://traces-internals URL.
class TracesInternalsLegacyUIConfig
    : public DefaultInternalWebUIConfig<TracesInternalsUI> {
 public:
  TracesInternalsLegacyUIConfig()
      : DefaultInternalWebUIConfig("traces-internals") {}
};

// WebUIController for the chrome://traces page.
class CONTENT_EXPORT TracesInternalsUI
    : public WebUIController,
      public traces_internals::mojom::TracesInternalsHandlerFactory {
 public:
  explicit TracesInternalsUI(content::WebUI* web_ui, const GURL& url);
  ~TracesInternalsUI() override;

  TracesInternalsUI(const TracesInternalsUI&) = delete;
  TracesInternalsUI& operator=(const TracesInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<
          traces_internals::mojom::TracesInternalsHandlerFactory> receiver);

  // WebUIController:
  void WebUIRenderFrameCreated(RenderFrameHost* rfh) override;

 private:
  // traces_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<traces_internals::mojom::Page> page,
      mojo::PendingReceiver<traces_internals::mojom::PageHandler> receiver)
      override;

  std::unique_ptr<TracesInternalsHandler> ui_handler_;
  mojo::Receiver<traces_internals::mojom::TracesInternalsHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACES_INTERNALS_TRACES_INTERNALS_UI_H_
