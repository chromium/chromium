// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CHROME_URLS_CHROME_URLS_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CHROME_URLS_CHROME_URLS_HANDLER_H_

#import "base/memory/raw_ptr.h"
#import "components/webui/chrome_urls/mojom/chrome_urls.mojom.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "mojo/public/cpp/bindings/pending_remote.h"
#import "mojo/public/cpp/bindings/receiver.h"
#import "mojo/public/cpp/bindings/remote.h"

class ProfileIOS;

namespace chrome_urls {

// Page handler for chrome://chrome-urls
class ChromeUrlsHandler : public chrome_urls::mojom::PageHandler {
 public:
  ChromeUrlsHandler(
      mojo::PendingReceiver<chrome_urls::mojom::PageHandler> receiver,
      mojo::PendingRemote<chrome_urls::mojom::Page> page,
      raw_ptr<ProfileIOS> profile);
  ~ChromeUrlsHandler() override;
  ChromeUrlsHandler(const ChromeUrlsHandler&) = delete;
  ChromeUrlsHandler& operator=(const ChromeUrlsHandler&) = delete;

 private:
  // chrome_urls::mojom::PageHandler
  void GetUrls(GetUrlsCallback callback) override;
  void SetDebugPagesEnabled(bool enabled,
                            SetDebugPagesEnabledCallback callback) override;

  raw_ptr<ProfileIOS> profile_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Receiver<chrome_urls::mojom::PageHandler> receiver_;
  mojo::Remote<chrome_urls::mojom::Page> page_;
};

}  // namespace chrome_urls

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CHROME_URLS_CHROME_URLS_HANDLER_H_
