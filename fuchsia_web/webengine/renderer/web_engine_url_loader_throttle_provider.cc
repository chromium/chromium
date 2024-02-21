// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/renderer/web_engine_url_loader_throttle_provider.h"

#include <functional>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "components/url_rewrite/common/url_loader_throttle.h"
#include "components/url_rewrite/mojom/url_request_rewrite.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "fuchsia_web/webengine/common/cors_exempt_headers.h"
#include "fuchsia_web/webengine/renderer/web_engine_content_renderer_client.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

WebEngineURLLoaderThrottleProvider::WebEngineURLLoaderThrottleProvider(
    const WebEngineContentRendererClient* const content_renderer_client)
    : content_renderer_client_(content_renderer_client) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WebEngineURLLoaderThrottleProvider::~WebEngineURLLoaderThrottleProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
WebEngineURLLoaderThrottleProvider::Clone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<WebEngineURLLoaderThrottleProvider> cloned_provider =
      std::make_unique<WebEngineURLLoaderThrottleProvider>(
          content_renderer_client_);
  DETACH_FROM_SEQUENCE(cloned_provider->sequence_checker_);
  return cloned_provider;
}

blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>>
WebEngineURLLoaderThrottleProvider::CreateThrottles(
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    const network::ResourceRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!local_frame_token.has_value()) {
    // `local_frame_token` is only set for Dedicated Workers. We do not support
    // other types of workers.
    return {};
  }

  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
  auto rules = content_renderer_client_->GetRewriteRulesForFrameToken(
      local_frame_token.value());
  if (rules) {
    throttles.emplace_back(std::make_unique<url_rewrite::URLLoaderThrottle>(
        rules, base::BindRepeating(&IsHeaderCorsExempt)));
  }
  return throttles;
}

void WebEngineURLLoaderThrottleProvider::SetOnline(bool is_online) {}
