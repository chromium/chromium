// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_url_loader_throttle.h"

#include "extensions/renderer/extension_throttle_manager.h"

namespace extensions {

namespace {

const char kCancelReason[] = "ExtensionURLLoaderThrottle";

}  // anonymous namespace

ExtensionURLLoaderThrottle::ExtensionURLLoaderThrottle(
    ExtensionThrottleManager* manager)
    : manager_(manager) {
  DCHECK(manager_);
}

ExtensionURLLoaderThrottle::~ExtensionURLLoaderThrottle() = default;

void ExtensionURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  start_request_url_ = request->url;
  if (manager_->ShouldRejectRequest(start_request_url_))
    delegate_->CancelWithError(net::ERR_TEMPORARILY_THROTTLED, kCancelReason);
}

void ExtensionURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& /* response_head */,
    bool* /* defer */,
    std::vector<std::string>* /* to_be_removed_request_headers */,
    net::HttpRequestHeaders* /* modified_request_headers */) {
  if (manager_->ShouldRejectRedirect(start_request_url_, *redirect_info)) {
    delegate_->CancelWithError(net::ERR_TEMPORARILY_THROTTLED, kCancelReason);
  }
}

void ExtensionURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  manager_->WillProcessResponse(response_url, *response_head);
}

void ExtensionURLLoaderThrottle::DetachFromCurrentSequence() {}

}  // namespace extensions
