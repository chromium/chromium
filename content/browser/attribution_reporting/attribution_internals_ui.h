// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_UI_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_UI_H_

#include <memory>

#include "content/browser/attribution_reporting/attribution_internals.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class AttributionInternalsHandlerImpl;
class AttributionInternalsUI;
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
class AttributionInternalsUI : public WebUIController,
                               public attribution_internals::mojom::Factory {
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
      mojo::PendingReceiver<attribution_internals::mojom::Factory>);

 private:
  // attribution_internals::mojom::Factory:
  void Create(
      mojo::PendingRemote<attribution_internals::mojom::Observer>,
      mojo::PendingReceiver<attribution_internals::mojom::Handler>) override;

  std::unique_ptr<AttributionInternalsHandlerImpl> ui_handler_;

  mojo::Receiver<attribution_internals::mojom::Factory> factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_UI_H_
