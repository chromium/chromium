// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_UI_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_UI_H_

#include <memory>

#include "content/browser/aggregation_service/aggregation_service_internals.mojom-forward.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class AggregationServiceInternalsHandlerImpl;
class AggregationServiceInternalsUI;
class WebUI;

// WebUIConfig for chrome://aggregation-service-internals page
class AggregationServiceInternalsUIConfig
    : public DefaultWebUIConfig<AggregationServiceInternalsUI> {
 public:
  AggregationServiceInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme,
                           kChromeUIPrivateAggregationInternalsHost) {}
};

// WebUI which handles serving the chrome://aggregation-service-internals page.
class AggregationServiceInternalsUI : public WebUIController {
 public:
  explicit AggregationServiceInternalsUI(WebUI* web_ui);
  AggregationServiceInternalsUI(const AggregationServiceInternalsUI&) = delete;
  AggregationServiceInternalsUI(AggregationServiceInternalsUI&&) = delete;
  AggregationServiceInternalsUI& operator=(
      const AggregationServiceInternalsUI&) = delete;
  AggregationServiceInternalsUI& operator=(AggregationServiceInternalsUI&&) =
      delete;
  ~AggregationServiceInternalsUI() override;

  // WebUIController overrides:
  void WebUIRenderFrameCreated(RenderFrameHost* render_frame_host) override;

  void BindInterface(
      mojo::PendingReceiver<aggregation_service_internals::mojom::Handler>
          receiver);

 private:
  std::unique_ptr<AggregationServiceInternalsHandlerImpl> ui_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_UI_H_
