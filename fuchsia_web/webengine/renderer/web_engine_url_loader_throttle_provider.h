// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_URL_LOADER_THROTTLE_PROVIDER_H_
#define FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_URL_LOADER_THROTTLE_PROVIDER_H_

#include "base/sequence_checker.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"

class WebEngineContentRendererClient;

// Implements a URLLoaderThrottleProvider for the WebEngine. Creates
// URLLoaderThrottles, implemented as WebEngineURLLoaderThrottles for network
// requests.
class WebEngineURLLoaderThrottleProvider
    : public blink::URLLoaderThrottleProvider {
 public:
  explicit WebEngineURLLoaderThrottleProvider(
      const WebEngineContentRendererClient* const content_renderer_client);

  WebEngineURLLoaderThrottleProvider(
      const WebEngineURLLoaderThrottleProvider&) = delete;
  WebEngineURLLoaderThrottleProvider& operator=(
      const WebEngineURLLoaderThrottleProvider&) = delete;

  ~WebEngineURLLoaderThrottleProvider() override;

  // blink::URLLoaderThrottleProvider implementation.
  std::unique_ptr<blink::URLLoaderThrottleProvider> Clone() override;
  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      base::optional_ref<const blink::LocalFrameToken> local_frame_token,
      const network::ResourceRequest& request) override;
  void SetOnline(bool is_online) override;

 private:
  const WebEngineContentRendererClient* const content_renderer_client_;
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_URL_LOADER_THROTTLE_PROVIDER_H_
