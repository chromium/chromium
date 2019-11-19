// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_URL_LOADER_THROTTLE_PROVIDER_H_
#define FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_URL_LOADER_THROTTLE_PROVIDER_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "content/public/renderer/url_loader_throttle_provider.h"

class WebEngineContentRendererClient;

// Implements a URLLoaderThrottleProvider for the WebEngine. Creates
// URLLoaderThrottles, implemented as WebEngineURLLoaderThrottles for network
// requests.
class WebEngineURLLoaderThrottleProvider
    : public content::URLLoaderThrottleProvider {
 public:
  explicit WebEngineURLLoaderThrottleProvider(
      WebEngineContentRendererClient* content_renderer_client);
  ~WebEngineURLLoaderThrottleProvider() override;

  // content::URLLoaderThrottleProvider implementation.
  std::unique_ptr<content::URLLoaderThrottleProvider> Clone() override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request,
      content::ResourceType resource_type) override;
  void SetOnline(bool is_online) override;

 private:
  const WebEngineContentRendererClient* const content_renderer_client_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(WebEngineURLLoaderThrottleProvider);
};

#endif  // FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_URL_LOADER_THROTTLE_PROVIDER_H_
