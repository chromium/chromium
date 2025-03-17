// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_UI_H_

#import "components/data_sharing/data_sharing_internals/webui/data_sharing_internals.mojom.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"
#import "mojo/public/cpp/bindings/receiver.h"

class DataSharingInternalsPageHandlerImpl;

// The WebUI handler for chrome://data-sharing-internals.
class DataSharingInternalsUI
    : public web::WebUIIOSController,
      public data_sharing_internals::mojom::PageHandlerFactory {
 public:
  explicit DataSharingInternalsUI(web::WebUIIOS* web_ui,
                                  const std::string& host);

  DataSharingInternalsUI(const DataSharingInternalsUI&) = delete;
  DataSharingInternalsUI& operator=(const DataSharingInternalsUI&) = delete;

  ~DataSharingInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<data_sharing_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // data_sharing_internals::mojom::PageHandlerFactory impls.
  void CreatePageHandler(
      mojo::PendingRemote<data_sharing_internals::mojom::Page> page,
      mojo::PendingReceiver<data_sharing_internals::mojom::PageHandler>
          receiver) override;

  std::unique_ptr<DataSharingInternalsPageHandlerImpl>
      data_sharing_internals_page_handler_;
  mojo::Receiver<data_sharing_internals::mojom::PageHandlerFactory>
      data_sharing_internals_page_factory_receiver_{this};
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_UI_H_
