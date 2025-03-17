// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CHROME_URLS_CHROME_URLS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CHROME_URLS_CHROME_URLS_UI_H_

#import "base/memory/raw_ptr.h"
#import "components/webui/chrome_urls/mojom/chrome_urls.mojom.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "mojo/public/cpp/bindings/receiver.h"

class ProfileIOS;

namespace web {
class WebUIIOS;
}

namespace chrome_urls {
class ChromeUrlsHandler;

// The WebUI controller for chrome://chrome-urls
class ChromeUrlsUI : public web::WebUIIOSController,
                     public chrome_urls::mojom::PageHandlerFactory {
 public:
  explicit ChromeUrlsUI(web::WebUIIOS* web_ui, const std::string& host);

  ChromeUrlsUI(const ChromeUrlsUI&) = delete;
  ChromeUrlsUI& operator=(const ChromeUrlsUI&) = delete;

  ~ChromeUrlsUI() override;

  void BindInterface(
      mojo::PendingReceiver<chrome_urls::mojom::PageHandlerFactory> receiver);

 private:
  // chrome_urls::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<chrome_urls::mojom::Page> page,
      mojo::PendingReceiver<chrome_urls::mojom::PageHandler> receiver) override;

  raw_ptr<ProfileIOS> profile_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  std::unique_ptr<ChromeUrlsHandler> page_handler_;
  mojo::Receiver<chrome_urls::mojom::PageHandlerFactory> page_factory_receiver_{
      this};
};

}  // namespace chrome_urls

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CHROME_URLS_CHROME_URLS_UI_H_
