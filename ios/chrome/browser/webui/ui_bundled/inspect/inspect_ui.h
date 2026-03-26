// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INSPECT_INSPECT_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INSPECT_INSPECT_UI_H_

#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature_delegate.h"
#import "ios/chrome/browser/webui/ui_bundled/inspect/inspect.mojom.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "mojo/public/cpp/bindings/pending_remote.h"
#import "mojo/public/cpp/bindings/receiver.h"
#import "mojo/public/cpp/bindings/remote.h"

// The WebUI handler for chrome://inspect which displays JavaScript console
// messages.
class InspectUI : public web::WebUIIOSController,
                  public inspect::mojom::PageHandlerFactory {
 public:
  explicit InspectUI(web::WebUIIOS* web_ui, const std::string& host);

  InspectUI(const InspectUI&) = delete;
  InspectUI& operator=(const InspectUI&) = delete;

  ~InspectUI() override;

  // inspect::mojom::PageHandlerFactory implementation.
  void CreatePageHandler(
      mojo::PendingRemote<inspect::mojom::Page> page,
      mojo::PendingReceiver<inspect::mojom::PageHandler> handler) override;

 private:
  void BindInterface(
      mojo::PendingReceiver<inspect::mojom::PageHandlerFactory> receiver);

  // The receiver for the PageHandlerFactory interface.
  mojo::Receiver<inspect::mojom::PageHandlerFactory> factory_receiver_{this};

  // The handler for the PageHandler interface.
  std::unique_ptr<inspect::mojom::PageHandler> handler_;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INSPECT_INSPECT_UI_H_
