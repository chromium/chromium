// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_PRIVATE_AI_INTERNALS_PRIVATE_AI_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_PRIVATE_AI_INTERNALS_PRIVATE_AI_INTERNALS_UI_H_

#import <memory>

#import "components/private_ai/ios/private_ai_network_driver_ios.h"
#import "components/private_ai/ios/private_ai_oak_session_driver_ios.h"
#import "components/private_ai/private_ai_internals/webui/private_ai_internals.mojom.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "services/network/public/mojom/network_context.mojom.h"

namespace private_ai {
class PrivateAiInternalsPageHandler;
}

namespace web {
class WebUIIOS;
}

// The WebUI controller for chrome://private-ai-internals on iOS.
class PrivateAiInternalsUI : public web::WebUIIOSController {
 public:
  PrivateAiInternalsUI(web::WebUIIOS* web_ui, const std::string& host);

  PrivateAiInternalsUI(const PrivateAiInternalsUI&) = delete;
  PrivateAiInternalsUI& operator=(const PrivateAiInternalsUI&) = delete;

  ~PrivateAiInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<
          private_ai_internals::mojom::PrivateAiInternalsPageHandler> receiver);

 private:
  private_ai::PrivateAiOakSessionDriverIOS oak_session_driver_ios_;
  private_ai::PrivateAiNetworkDriverIOS network_driver_ios_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  std::unique_ptr<private_ai::PrivateAiInternalsPageHandler> page_handler_;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_PRIVATE_AI_INTERNALS_PRIVATE_AI_INTERNALS_UI_H_
