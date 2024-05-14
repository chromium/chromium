// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/url_loader_throttle.h"

#include "base/notreached.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace blink {

void URLLoaderThrottle::Delegate::UpdateDeferredResponseHead(
    network::mojom::URLResponseHeadPtr new_response_head,
    mojo::ScopedDataPipeConsumerHandle body) {}

void URLLoaderThrottle::Delegate::InterceptResponse(
    mojo::PendingRemote<network::mojom::URLLoader> new_loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient> new_client_receiver,
    mojo::PendingRemote<network::mojom::URLLoader>* original_loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>*
        original_client_receiver,
    mojo::ScopedDataPipeConsumerHandle* body) {
  NOTIMPLEMENTED();
}

URLLoaderThrottle::Delegate::~Delegate() {}

URLLoaderThrottle::~URLLoaderThrottle() {}

void URLLoaderThrottle::DetachFromCurrentSequence() {
  NOTREACHED_IN_MIGRATION();
}

void URLLoaderThrottle::WillStartRequest(network::ResourceRequest* request,
                                         bool* defer) {}

const char* URLLoaderThrottle::NameForLoggingWillStartRequest() {
  return nullptr;
}

void URLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {}

void URLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {}

const char* URLLoaderThrottle::NameForLoggingWillProcessResponse() {
  return nullptr;
}

void URLLoaderThrottle::BeforeWillProcessResponse(
    const GURL& response_url,
    const network::mojom::URLResponseHead& response_head,
    RestartWithURLReset* restart_with_url_reset) {}

void URLLoaderThrottle::BeforeWillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    RestartWithURLReset* restart_with_url_reset,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {}

void URLLoaderThrottle::WillOnCompleteWithError(
    const network::URLLoaderCompletionStatus& status) {}

URLLoaderThrottle::URLLoaderThrottle() {}

}  // namespace blink
