// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_UI_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_UI_H_

#include <memory>

#include "content/browser/attribution_reporting/attribution_internals.mojom.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class AttributionInternalsHandlerImpl;
class AttributionManagerProvider;

// WebUI which handles serving the chrome://attribution-internals page.
class CONTENT_EXPORT AttributionInternalsUI : public WebUIController {
 public:
  explicit AttributionInternalsUI(WebUI* web_ui);
  AttributionInternalsUI(const AttributionInternalsUI& other) = delete;
  AttributionInternalsUI& operator=(const AttributionInternalsUI& other) =
      delete;
  AttributionInternalsUI(AttributionInternalsUI&& other) = delete;
  AttributionInternalsUI& operator=(AttributionInternalsUI&& other) = delete;
  ~AttributionInternalsUI() override;

  // WebUIController overrides:
  void WebUIRenderFrameCreated(RenderFrameHost* render_frame_host) override;

  void BindInterface(
      mojo::PendingReceiver<mojom::AttributionInternalsHandler> receiver);

  void SetAttributionManagerProviderForTesting(
      std::unique_ptr<AttributionManagerProvider> manager_provider);

 private:
  std::unique_ptr<AttributionInternalsHandlerImpl> ui_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_UI_H_
