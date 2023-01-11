// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_APP_BANNER_SERVICE_H_
#define CONTENT_WEB_TEST_RENDERER_APP_BANNER_SERVICE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/app_banner/app_banner.mojom.h"

namespace content {

// Test app banner service that is registered as a Mojo service for
// BeforeInstallPromptEvents to look up when the test runner is executed.
class AppBannerService : public blink::mojom::AppBannerService {
 public:
  AppBannerService();

  AppBannerService(const AppBannerService&) = delete;
  AppBannerService& operator=(const AppBannerService&) = delete;

  ~AppBannerService() override;

  mojo::Remote<blink::mojom::AppBannerController>& controller() {
    return controller_;
  }
  void ResolvePromise(const std::string& platform);
  void SendBannerPromptRequest(const std::vector<std::string>& platforms,
                               base::OnceCallback<void(bool)> callback);

  // blink::mojom::AppBannerService overrides.
  void DisplayAppBanner() override;

 private:
  void OnBannerPromptReply(base::OnceCallback<void(bool)> callback,
                           blink::mojom::AppBannerPromptReply);

  mojo::Receiver<blink::mojom::AppBannerService> receiver_{this};
  mojo::Remote<blink::mojom::AppBannerEvent> event_;
  mojo::Remote<blink::mojom::AppBannerController> controller_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_APP_BANNER_SERVICE_H_
