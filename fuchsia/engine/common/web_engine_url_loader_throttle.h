// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_COMMON_WEB_ENGINE_URL_LOADER_THROTTLE_H_
#define FUCHSIA_ENGINE_COMMON_WEB_ENGINE_URL_LOADER_THROTTLE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "fuchsia/engine/url_request_rewrite.mojom.h"
#include "fuchsia/engine/web_engine_export.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

// Implements a URLLoaderThrottle for the WebEngine. Applies network request
// rewrites provided through the fuchsia.web.SetUrlRequestRewriteRules FIDL API.
class WEB_ENGINE_EXPORT WebEngineURLLoaderThrottle
    : public blink::URLLoaderThrottle {
 public:
  using UrlRequestRewriteRules =
      base::RefCountedData<std::vector<mojom::UrlRequestRulePtr>>;

  explicit WebEngineURLLoaderThrottle(
      scoped_refptr<UrlRequestRewriteRules> rules);
  ~WebEngineURLLoaderThrottle() override;

  WebEngineURLLoaderThrottle(const WebEngineURLLoaderThrottle&) = delete;
  WebEngineURLLoaderThrottle& operator=(const WebEngineURLLoaderThrottle&) =
      delete;

  // blink::URLLoaderThrottle implementation.
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  bool makes_unsafe_redirect() override;

 private:
  // Applies transformations specified by |rule| to |request|, conditional on
  // the matching criteria of |rule|.
  void ApplyRule(network::ResourceRequest* request,
                 const mojom::UrlRequestRulePtr& rule);

  // Applies |rewrite| transformations to |request|.
  void ApplyRewrite(network::ResourceRequest* request,
                    const mojom::UrlRequestActionPtr& rewrite);

  // Adds HTTP headers from |add_headers| to |request|.
  void ApplyAddHeaders(
      network::ResourceRequest* request,
      const mojom::UrlRequestRewriteAddHeadersPtr& add_headers);

  scoped_refptr<UrlRequestRewriteRules> rules_;
};

#endif  // FUCHSIA_ENGINE_COMMON_WEB_ENGINE_URL_LOADER_THROTTLE_H_
