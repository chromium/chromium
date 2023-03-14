// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_UI_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_UI_H_

#include <memory>

#include "content/browser/aggregation_service/aggregation_service_internals.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class AggregationServiceInternalsHandlerImpl;
class AggregationServiceInternalsUI;
class WebUI;

// WebUIConfig for chrome://private-aggregation-internals page
class AggregationServiceInternalsUIConfig
    : public DefaultWebUIConfig<AggregationServiceInternalsUI> {
 public:
  AggregationServiceInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme,
                           kChromeUIPrivateAggregationInternalsHost) {}
};

// WebUI which handles serving the chrome://private-aggregation-internals page.
class AggregationServiceInternalsUI
    : public WebUIController,
      public aggregation_service_internals::mojom::Factory {
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
      mojo::PendingReceiver<aggregation_service_internals::mojom::Factory>);

 private:
  // aggregation_service_internals::mojom::Factory:
  void Create(
      mojo::PendingRemote<aggregation_service_internals::mojom::Observer>,
      mojo::PendingReceiver<aggregation_service_internals::mojom::Handler>)
      override;

  std::unique_ptr<AggregationServiceInternalsHandlerImpl> ui_handler_;

  mojo::Receiver<aggregation_service_internals::mojom::Factory> factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_UI_H_
