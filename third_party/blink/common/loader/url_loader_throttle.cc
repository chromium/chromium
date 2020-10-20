// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/url_loader_throttle.h"

#include "base/notreached.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace blink {

void URLLoaderThrottle::Delegate::SetPriority(net::RequestPriority priority) {}
void URLLoaderThrottle::Delegate::UpdateDeferredRequestHeaders(
    const net::HttpRequestHeaders& modified_request_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_request_headers) {}
void URLLoaderThrottle::Delegate::UpdateDeferredResponseHead(
    network::mojom::URLResponseHeadPtr new_response_head) {}
void URLLoaderThrottle::Delegate::PauseReadingBodyFromNet() {}
void URLLoaderThrottle::Delegate::ResumeReadingBodyFromNet() {}

void URLLoaderThrottle::Delegate::InterceptResponse(
    mojo::PendingRemote<network::mojom::URLLoader> new_loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient> new_client_receiver,
    mojo::PendingRemote<network::mojom::URLLoader>* original_loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>*
        original_client_receiver) {
  NOTIMPLEMENTED();
}

void URLLoaderThrottle::Delegate::RestartWithFlags(int additional_load_flags) {
  NOTIMPLEMENTED();
}

void URLLoaderThrottle::Delegate::RestartWithURLResetAndFlags(
    int additional_load_flags) {
  NOTIMPLEMENTED();
}

void URLLoaderThrottle::Delegate::RestartWithURLResetAndFlagsNow(
    int additional_load_flags) {
  NOTIMPLEMENTED();
}

void URLLoaderThrottle::Delegate::RestartWithModifiedHeadersNow(
    const net::HttpRequestHeaders& modified_headers) {
  NOTIMPLEMENTED();
}

URLLoaderThrottle::Delegate::~Delegate() {}

URLLoaderThrottle::~URLLoaderThrottle() {}

void URLLoaderThrottle::DetachFromCurrentSequence() {
  NOTREACHED();
}

void URLLoaderThrottle::WillStartRequest(network::ResourceRequest* request,
                                         bool* defer) {}

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

void URLLoaderThrottle::BeforeWillProcessResponse(
    const GURL& response_url,
    const network::mojom::URLResponseHead& response_head,
    bool* defer) {}

void URLLoaderThrottle::WillOnCompleteWithError(
    const network::URLLoaderCompletionStatus& status,
    bool* defer) {}

bool URLLoaderThrottle::makes_unsafe_redirect() {
  return false;
}

URLLoaderThrottle::URLLoaderThrottle() {}

}  // namespace blink
