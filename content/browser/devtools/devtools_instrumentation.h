// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_INSTRUMENTATION_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_INSTRUMENTATION_H_

/*
  The functions in this file are for routing instrumentation signals
  to the relevant set of devtools protocol handlers.
*/

#include <vector>

#include "base/optional.h"
#include "content/common/navigation_params.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

class GURL;

namespace base {
class UnguessableToken;
}

namespace net {
class SSLInfo;
class X509Certificate;
}  // namespace net

namespace network {
struct ResourceResponse;
}

namespace content {
class SignedExchangeEnvelope;
class FrameTreeNode;
class NavigationHandleImpl;
class NavigationRequest;
class NavigationThrottle;
class RenderFrameHostImpl;
struct SignedExchangeError;

namespace devtools_instrumentation {

void ApplyNetworkRequestOverrides(FrameTreeNode* frame_tree_node,
                                  mojom::BeginNavigationParams* begin_params,
                                  bool* report_raw_headers);

bool WillCreateURLLoaderFactory(
    RenderFrameHostImpl* rfh,
    bool is_navigation,
    bool is_download,
    network::mojom::URLLoaderFactoryRequest* loader_factory_request);

void OnResetNavigationRequest(NavigationRequest* navigation_request);
void OnNavigationRequestWillBeSent(const NavigationRequest& navigation_request);
void OnNavigationResponseReceived(const NavigationRequest& nav_request,
                                  const network::ResourceResponse& response);
void OnNavigationRequestFailed(
    const NavigationRequest& nav_request,
    const network::URLLoaderCompletionStatus& status);

void OnSignedExchangeReceived(
    FrameTreeNode* frame_tree_node,
    base::Optional<const base::UnguessableToken> devtools_navigation_token,
    const GURL& outer_request_url,
    const network::ResourceResponseHead& outer_response,
    const base::Optional<SignedExchangeEnvelope>& header,
    const scoped_refptr<net::X509Certificate>& certificate,
    const base::Optional<net::SSLInfo>& ssl_info,
    const std::vector<SignedExchangeError>& errors);
void OnSignedExchangeCertificateRequestSent(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const base::UnguessableToken& loader_id,
    const network::ResourceRequest& request,
    const GURL& signed_exchange_url);
void OnSignedExchangeCertificateResponseReceived(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const base::UnguessableToken& loader_id,
    const GURL& url,
    const network::ResourceResponseHead& head);
void OnSignedExchangeCertificateRequestCompleted(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const network::URLLoaderCompletionStatus& status);

std::vector<std::unique_ptr<NavigationThrottle>> CreateNavigationThrottles(
    NavigationHandleImpl* navigation_handle);

}  // namespace devtools_instrumentation

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_INSTRUMENTATION_H_
