// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/renderer/web_engine_url_loader_throttle_provider.h"

#include "components/url_rewrite/common/url_loader_throttle.h"
#include "components/url_rewrite/mojom/url_request_rewrite.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "fuchsia_web/webengine/common/cors_exempt_headers.h"
#include "fuchsia_web/webengine/renderer/web_engine_content_renderer_client.h"
#include "third_party/blink/public/web/web_local_frame.h"

WebEngineURLLoaderThrottleProvider::WebEngineURLLoaderThrottleProvider(
    WebEngineContentRendererClient* content_renderer_client)
    : content_renderer_client_(content_renderer_client) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WebEngineURLLoaderThrottleProvider::~WebEngineURLLoaderThrottleProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
WebEngineURLLoaderThrottleProvider::Clone() {
  // This should only happen for workers, which we do not support here.
  NOTREACHED();
  return nullptr;
}

blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>>
WebEngineURLLoaderThrottleProvider::CreateThrottles(
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    const blink::WebURLRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(local_frame_token.has_value());
  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
  blink::WebLocalFrame* web_frame =
      blink::WebLocalFrame::FromFrameToken(local_frame_token.value());
  if (!web_frame) {
    return throttles;
  }
  auto rules = content_renderer_client_
                   ->GetWebEngineRenderFrameObserverForFrameToken(
                       local_frame_token.value())
                   ->url_request_rules_receiver()
                   ->GetCachedRules();
  if (rules) {
    throttles.emplace_back(std::make_unique<url_rewrite::URLLoaderThrottle>(
        rules, base::BindRepeating(&IsHeaderCorsExempt)));
  }
  return throttles;
}

void WebEngineURLLoaderThrottleProvider::SetOnline(bool is_online) {}
