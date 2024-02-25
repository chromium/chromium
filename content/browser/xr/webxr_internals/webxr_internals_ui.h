// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_WEBXR_INTERNALS_WEBXR_INTERNALS_UI_H_
#define CONTENT_BROWSER_XR_WEBXR_INTERNALS_WEBXR_INTERNALS_UI_H_

#include "content/browser/xr/webxr_internals/mojom/webxr_internals.mojom.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace content {

class WebXrInternalsUI;

class WebXrInternalsUIConfig : public DefaultWebUIConfig<WebXrInternalsUI> {
 public:
  WebXrInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kChromeUIWebXrInternalsHost) {}
  bool IsWebUIEnabled(BrowserContext* browser_context) override;
};

// The implementation for the chrome://webxr-internals page.
class WebXrInternalsUI : public WebUIController {
 public:
  explicit WebXrInternalsUI(WebUI* web_ui);
  ~WebXrInternalsUI() override;

  WebXrInternalsUI(const WebXrInternalsUI&) = delete;
  WebXrInternalsUI& operator=(const WebXrInternalsUI&) = delete;

  void BindInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<webxr::mojom::WebXrInternalsHandler> receiver);

  // WebUIController overrides:
  void WebUIRenderFrameCreated(RenderFrameHost* render_frame_host) override;

 private:
  std::unique_ptr<webxr::mojom::WebXrInternalsHandler> ui_handler_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_WEBXR_INTERNALS_WEBXR_INTERNALS_UI_H_
