// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_COMMON_WEB_ENGINE_URL_LOADER_THROTTLE_H_
#define FUCHSIA_ENGINE_COMMON_WEB_ENGINE_URL_LOADER_THROTTLE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "fuchsia/engine/url_request_rewrite.mojom.h"
#include "fuchsia/engine/web_engine_export.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

// Implements a URLLoaderThrottle for the WebEngine. Applies network request
// rewrites provided through the fuchsia.web.SetUrlRequestRewriteRules FIDL API.
class WEB_ENGINE_EXPORT WebEngineURLLoaderThrottle
    : public blink::URLLoaderThrottle {
 public:
  using UrlRequestRewriteRules =
      base::RefCountedData<std::vector<mojom::UrlRequestRewriteRulePtr>>;

  // An interface to provide rewrite rules to the throttle. Its
  // implementation must outlive the WebEngineURLLoaderThrottle.
  class CachedRulesProvider {
   public:
    virtual ~CachedRulesProvider() = default;

    // Gets cached rules. This call can be made on any sequence, as
    // URLLoaderThrottles are not guaranteed to stay on the same sequence.
    virtual scoped_refptr<UrlRequestRewriteRules> GetCachedRules() = 0;
  };

  explicit WebEngineURLLoaderThrottle(
      CachedRulesProvider* cached_rules_provider);
  ~WebEngineURLLoaderThrottle() override;

  // blink::URLLoaderThrottle implementation.
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  bool makes_unsafe_redirect() override;

 private:
  CachedRulesProvider* const cached_rules_provider_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineURLLoaderThrottle);
};

#endif  // FUCHSIA_ENGINE_COMMON_WEB_ENGINE_URL_LOADER_THROTTLE_H_
