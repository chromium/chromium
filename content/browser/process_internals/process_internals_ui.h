// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_UI_H_
#define CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_UI_H_

#include <memory>
#include <utility>

#include "content/browser/process_internals/process_internals.mojom.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class ProcessInternalsUI;

// WebUIConfig for chrome://process-internals.
class ProcessInternalsUIConfig : public DefaultWebUIConfig<ProcessInternalsUI> {
 public:
  ProcessInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kChromeUIProcessInternalsHost) {}
};

// WebUI which handles serving the chrome://process-internals page.
// TODO(nasko): Change the inheritance of this class to be from
// MojoWebUIController, so the registry_ can be removed and properly
// inherited from common base class for Mojo WebUIs.
class ProcessInternalsUI : public WebUIController {
 public:
  explicit ProcessInternalsUI(WebUI* web_ui);
  ~ProcessInternalsUI() override;
  ProcessInternalsUI(const ProcessInternalsUI&) = delete;
  ProcessInternalsUI& operator=(const ProcessInternalsUI&) = delete;

  // WebUIController overrides:
  void WebUIRenderFrameCreated(RenderFrameHost* render_frame_host) override;

  void BindInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<::mojom::ProcessInternalsHandler> receiver);

 private:
  std::unique_ptr<::mojom::ProcessInternalsHandler> ui_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_UI_H_
