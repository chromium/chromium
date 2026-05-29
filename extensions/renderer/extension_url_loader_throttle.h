// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_URL_LOADER_THROTTLE_H_
#define EXTENSIONS_RENDERER_EXTENSION_URL_LOADER_THROTTLE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace extensions {

class ExtensionThrottleManagerAccess;

// This class monitors requests issued by extensions and throttles the request
// if there are too many requests made within a short time to urls with the same
// scheme, host, port and path. For the exact criteria for throttling, please
// also see extension_throttle_manager.cc.
class ExtensionURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  explicit ExtensionURLLoaderThrottle(
      scoped_refptr<ExtensionThrottleManagerAccess> manager_access);

  ExtensionURLLoaderThrottle(const ExtensionURLLoaderThrottle&) = delete;
  ExtensionURLLoaderThrottle& operator=(const ExtensionURLLoaderThrottle&) =
      delete;

  ~ExtensionURLLoaderThrottle() override;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      network::HttpRequestHeadersUpdateParams* headers_update_params) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;

 private:
  // blink::URLLoaderThrottle:
  void DetachFromCurrentSequence() override;

  scoped_refptr<ExtensionThrottleManagerAccess> manager_access_;
  GURL start_request_url_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_URL_LOADER_THROTTLE_H_
