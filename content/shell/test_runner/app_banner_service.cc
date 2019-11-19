// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/app_banner_service.h"
#include "base/bind.h"

namespace test_runner {

AppBannerService::AppBannerService() = default;

AppBannerService::~AppBannerService() = default;

void AppBannerService::ResolvePromise(const std::string& platform) {
  if (!event_.is_bound())
    return;

  // Empty platform means to resolve as a dismissal.
  if (platform.empty())
    event_->BannerDismissed();
  else
    event_->BannerAccepted(platform);
}

void AppBannerService::SendBannerPromptRequest(
    const std::vector<std::string>& platforms,
    base::OnceCallback<void(bool)> callback) {
  if (!controller_.is_bound())
    return;

  controller_->BannerPromptRequest(
      receiver_.BindNewPipeAndPassRemote(), event_.BindNewPipeAndPassReceiver(),
      platforms,
      base::BindOnce(&AppBannerService::OnBannerPromptReply,
                     base::Unretained(this), std::move(callback)));
}

void AppBannerService::DisplayAppBanner() { /* do nothing */
}

void AppBannerService::OnBannerPromptReply(
    base::OnceCallback<void(bool)> callback,
    blink::mojom::AppBannerPromptReply reply) {
  std::move(callback).Run(reply == blink::mojom::AppBannerPromptReply::CANCEL);
}

}  // namespace test_runner
