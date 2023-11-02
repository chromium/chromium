// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_QUOTA_QUOTA_INTERNALS_UI_H_
#define CONTENT_BROWSER_QUOTA_QUOTA_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/quota/quota_internals.mojom-forward.h"

namespace content {

class RenderFrameHost;
class QuotaInternalsUI;
class WebUI;

// WebUIConfig for the chrome://quota-internals page.
class QuotaInternalsUIConfig : public DefaultWebUIConfig<QuotaInternalsUI> {
 public:
  QuotaInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kChromeUIQuotaInternalsHost) {}
};

// WebUIController for the chrome://quota-internals page.
class CONTENT_EXPORT QuotaInternalsUI : public WebUIController {
 public:
  explicit QuotaInternalsUI(WebUI* web_ui);
  QuotaInternalsUI(const QuotaInternalsUI& other) = delete;
  QuotaInternalsUI& operator=(const QuotaInternalsUI& other) = delete;
  ~QuotaInternalsUI() override;

  // WebUIController overrides:
  void WebUIRenderFrameCreated(RenderFrameHost* render_frame_host) override;

  void BindInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<storage::mojom::QuotaInternalsHandler> receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_QUOTA_QUOTA_INTERNALS_UI_H_
