// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_UI_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_UI_H_

#include <memory>

#include "content/browser/attribution_reporting/attribution_internals.mojom-forward.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class AttributionInternalsHandlerImpl;
class AttributionInternalsUI;
class RenderFrameHost;
class WebUI;

// WebUIConfig for chrome://attribution-internals page
class AttributionInternalsUIConfig
    : public DefaultWebUIConfig<AttributionInternalsUI> {
 public:
  AttributionInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kChromeUIAttributionInternalsHost) {
  }
};

// WebUI which handles serving the chrome://attribution-internals page.
class AttributionInternalsUI : public WebUIController {
 public:
  explicit AttributionInternalsUI(WebUI* web_ui);
  AttributionInternalsUI(const AttributionInternalsUI&) = delete;
  AttributionInternalsUI& operator=(const AttributionInternalsUI&) = delete;
  AttributionInternalsUI(AttributionInternalsUI&&) = delete;
  AttributionInternalsUI& operator=(AttributionInternalsUI&&) = delete;
  ~AttributionInternalsUI() override;

  // WebUIController overrides:
  void WebUIRenderFrameCreated(RenderFrameHost* render_frame_host) override;

  void BindInterface(
      mojo::PendingReceiver<attribution_internals::mojom::Handler> receiver);

 private:
  std::unique_ptr<AttributionInternalsHandlerImpl> ui_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_UI_H_
