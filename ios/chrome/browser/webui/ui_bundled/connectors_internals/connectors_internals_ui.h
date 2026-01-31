// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_UI_H_

#import "components/enterprise/connectors/connectors_internals.mojom.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"

class ConnectorsInternalsPageHandler;

class ConnectorsInternalsUI : public web::WebUIIOSController {
 public:
  ConnectorsInternalsUI(web::WebUIIOS* web_ui, const std::string& host);
  ~ConnectorsInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver);

 private:
  std::unique_ptr<ConnectorsInternalsPageHandler> page_handler_;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_UI_H_
