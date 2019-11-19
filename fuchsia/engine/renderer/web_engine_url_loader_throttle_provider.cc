// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/renderer/web_engine_url_loader_throttle_provider.h"

#include "content/public/renderer/render_frame.h"
#include "fuchsia/engine/common/web_engine_url_loader_throttle.h"
#include "fuchsia/engine/renderer/web_engine_content_renderer_client.h"

WebEngineURLLoaderThrottleProvider::WebEngineURLLoaderThrottleProvider(
    WebEngineContentRendererClient* content_renderer_client)
    : content_renderer_client_(content_renderer_client) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WebEngineURLLoaderThrottleProvider::~WebEngineURLLoaderThrottleProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<content::URLLoaderThrottleProvider>
WebEngineURLLoaderThrottleProvider::Clone() {
  // This should only happen for service workers, which we do not support here.
  NOTREACHED();
  return nullptr;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
WebEngineURLLoaderThrottleProvider::CreateThrottles(
    int render_frame_id,
    const blink::WebURLRequest& request,
    content::ResourceType resource_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
  throttles.emplace_back(std::make_unique<WebEngineURLLoaderThrottle>(
      content_renderer_client_->GetUrlRequestRulesReceiverForRenderFrameId(
          render_frame_id)));
  return throttles;
}

void WebEngineURLLoaderThrottleProvider::SetOnline(bool is_online) {}
