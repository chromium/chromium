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
#include "content/public/browser/certificate_request_result_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"

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
class FileSelectListener;
class NavigationHandle;
class NavigationRequest;
class NavigationThrottle;
class RenderFrameHostImpl;
class RenderProcessHost;
class WebContents;

struct SignedExchangeError;

namespace devtools_instrumentation {

void ApplyNetworkRequestOverrides(FrameTreeNode* frame_tree_node,
                                  mojom::BeginNavigationParams* begin_params,
                                  bool* report_raw_headers);

bool WillCreateURLLoaderFactory(
    RenderFrameHostImpl* rfh,
    bool is_navigation,
    bool is_download,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
        loader_factory_receiver);

bool InterceptFileChooser(
    RenderFrameHostImpl* rfh,
    std::unique_ptr<content::FileSelectListener>* listener,
    const blink::mojom::FileChooserParams& params);

bool WillCreateURLLoaderFactoryForServiceWorker(
    RenderProcessHost* rph,
    int routing_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
        loader_factory_receiver);

bool WillCreateURLLoaderFactory(
    RenderFrameHostImpl* rfh,
    bool is_navigation,
    bool is_download,
    std::unique_ptr<network::mojom::URLLoaderFactory>* factory);

void OnResetNavigationRequest(NavigationRequest* navigation_request);
void OnNavigationRequestWillBeSent(const NavigationRequest& navigation_request);
void OnNavigationResponseReceived(const NavigationRequest& nav_request,
                                  const network::ResourceResponse& response);
void OnNavigationRequestFailed(
    const NavigationRequest& nav_request,
    const network::URLLoaderCompletionStatus& status);

void WillBeginDownload(int render_process_id,
                       int render_frame_id,
                       const GURL& url);

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

void OnRequestWillBeSentExtraInfo(
    int process_id,
    int routing_id,
    const std::string& devtools_request_id,
    const net::CookieStatusList& request_cookie_list,
    const std::vector<network::mojom::HttpRawHeaderPairPtr>& request_headers);
void OnResponseReceivedExtraInfo(
    int process_id,
    int routing_id,
    const std::string& devtools_request_id,
    const net::CookieAndLineStatusList& response_cookie_list,
    const std::vector<network::mojom::HttpRawHeaderPairPtr>& response_headers,
    const base::Optional<std::string>& response_headers_text);

std::vector<std::unique_ptr<NavigationThrottle>> CreateNavigationThrottles(
    NavigationHandle* navigation_handle);

// Asks any interested agents to handle the given certificate error. Returns
// |true| if the error was handled, |false| otherwise.
using CertErrorCallback =
    base::RepeatingCallback<void(content::CertificateRequestResultType)>;
bool HandleCertificateError(WebContents* web_contents,
                            int cert_error,
                            const GURL& request_url,
                            CertErrorCallback callback);

void PortalAttached(RenderFrameHostImpl* render_frame_host_impl);
void PortalDetached(RenderFrameHostImpl* render_frame_host_impl);
void PortalActivated(RenderFrameHostImpl* render_frame_host_impl);

}  // namespace devtools_instrumentation

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_INSTRUMENTATION_H_
