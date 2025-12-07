// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_url_loader_throttle.h"

#include "base/memory/scoped_refptr.h"
#include "extensions/renderer/extension_throttle_manager.h"
#include "extensions/renderer/extension_throttle_manager_access.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"

namespace extensions {

namespace {

const char kCancelReason[] = "ExtensionURLLoaderThrottle";

}  // anonymous namespace

ExtensionURLLoaderThrottle::ExtensionURLLoaderThrottle(
    scoped_refptr<ExtensionThrottleManagerAccess> manager_access)
    : manager_access_(std::move(manager_access)) {
  DCHECK(manager_access_);
}

ExtensionURLLoaderThrottle::~ExtensionURLLoaderThrottle() = default;

void ExtensionURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  start_request_url_ = request->url;
  auto [lock, manager] = manager_access_->Get();
  if (manager && manager->ShouldRejectRequest(start_request_url_)) {
    delegate_->CancelWithError(net::ERR_TEMPORARILY_THROTTLED, kCancelReason);
  }
}

void ExtensionURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    /*response_head=*/const network::mojom::URLResponseHead&,
    /*defer=*/bool*,
    /*to_be_removed_request_headers=*/std::vector<std::string>*,
    /*modified_request_headers=*/net::HttpRequestHeaders*,
    /*modified_cors_exempt_request_headers=*/net::HttpRequestHeaders*) {
  auto [lock, manager] = manager_access_->Get();
  if (manager &&
      manager->ShouldRejectRedirect(start_request_url_, *redirect_info)) {
    delegate_->CancelWithError(net::ERR_TEMPORARILY_THROTTLED, kCancelReason);
  }
}

void ExtensionURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  auto [lock, manager] = manager_access_->Get();
  if (manager) {
    manager->WillProcessResponse(response_url, *response_head);
  }
}

void ExtensionURLLoaderThrottle::DetachFromCurrentSequence() {}

}  // namespace extensions
