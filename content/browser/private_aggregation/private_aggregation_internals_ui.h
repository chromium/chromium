// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_INTERNALS_UI_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_INTERNALS_UI_H_

#include <memory>

#include "content/browser/private_aggregation/private_aggregation_internals.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class PrivateAggregationInternalsHandlerImpl;
class PrivateAggregationInternalsUI;
class WebUI;

// WebUIConfig for chrome://private-aggregation-internals page
class PrivateAggregationInternalsUIConfig
    : public DefaultWebUIConfig<PrivateAggregationInternalsUI> {
 public:
  PrivateAggregationInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme,
                           kChromeUIPrivateAggregationInternalsHost) {}
};

// WebUI which handles serving the chrome://private-aggregation-internals page.
class PrivateAggregationInternalsUI
    : public WebUIController,
      public private_aggregation_internals::mojom::Factory {
 public:
  explicit PrivateAggregationInternalsUI(WebUI* web_ui);
  PrivateAggregationInternalsUI(const PrivateAggregationInternalsUI&) = delete;
  PrivateAggregationInternalsUI(PrivateAggregationInternalsUI&&) = delete;
  PrivateAggregationInternalsUI& operator=(
      const PrivateAggregationInternalsUI&) = delete;
  PrivateAggregationInternalsUI& operator=(PrivateAggregationInternalsUI&&) =
      delete;
  ~PrivateAggregationInternalsUI() override;

  // WebUIController overrides:
  void WebUIRenderFrameCreated(RenderFrameHost* render_frame_host) override;

  void BindInterface(
      mojo::PendingReceiver<private_aggregation_internals::mojom::Factory>);

 private:
  // private_aggregation_internals::mojom::Factory:
  void Create(
      mojo::PendingRemote<private_aggregation_internals::mojom::Observer>,
      mojo::PendingReceiver<private_aggregation_internals::mojom::Handler>)
      override;

  std::unique_ptr<PrivateAggregationInternalsHandlerImpl> ui_handler_;

  mojo::Receiver<private_aggregation_internals::mojom::Factory> factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_INTERNALS_UI_H_
