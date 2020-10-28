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
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-forward.h"

class GURL;

namespace base {
class UnguessableToken;
}

namespace blink {
struct UserAgentMetadata;
}

namespace net {
class SSLInfo;
class X509Certificate;
struct QuicTransportError;
}  // namespace net

namespace download {
struct DownloadCreateInfo;
class DownloadItem;
}  // namespace download

namespace content {
class BrowserContext;
class DevToolsAgentHostImpl;
class FrameTreeNode;
class NavigationHandle;
class NavigationRequest;
class NavigationThrottle;
class RenderFrameHostImpl;
class RenderProcessHost;
class SharedWorkerHost;
class SignedExchangeEnvelope;
class WebContents;

struct SignedExchangeError;

namespace protocol {
namespace Audits {
class InspectorIssue;
}  // namespace Audits
}  // namespace protocol

namespace devtools_instrumentation {

void ApplyNetworkRequestOverrides(FrameTreeNode* frame_tree_node,
                                  mojom::BeginNavigationParams* begin_params,
                                  bool* report_raw_headers);

// Returns true if devtools want |*override_out| to be used.
// (A true return and |*override_out| being nullopt means no user agent client
//  hints should be sent; a false return means devtools doesn't want to affect
//  the behavior).
bool ApplyUserAgentMetadataOverrides(
    FrameTreeNode* frame_tree_node,
    base::Optional<blink::UserAgentMetadata>* override_out);

bool WillCreateURLLoaderFactory(
    RenderFrameHostImpl* rfh,
    bool is_navigation,
    bool is_download,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
        loader_factory_receiver,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override);

bool WillCreateURLLoaderFactoryForWorker(
    DevToolsAgentHostImpl* host,
    const base::UnguessableToken& worker_token,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override);

bool WillCreateURLLoaderFactoryForServiceWorker(
    RenderProcessHost* rph,
    int routing_id,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override);

bool WillCreateURLLoaderFactoryForSharedWorker(
    SharedWorkerHost* host,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override);

bool WillCreateURLLoaderFactory(
    RenderFrameHostImpl* rfh,
    bool is_navigation,
    bool is_download,
    std::unique_ptr<network::mojom::URLLoaderFactory>* factory);

void OnResetNavigationRequest(NavigationRequest* navigation_request);
void OnNavigationRequestWillBeSent(const NavigationRequest& navigation_request);
void OnNavigationResponseReceived(
    const NavigationRequest& nav_request,
    const network::mojom::URLResponseHead& response);
void OnNavigationRequestFailed(
    const NavigationRequest& nav_request,
    const network::URLLoaderCompletionStatus& status);

void WillBeginDownload(download::DownloadCreateInfo* info,
                       download::DownloadItem* item);

void OnSignedExchangeReceived(
    FrameTreeNode* frame_tree_node,
    base::Optional<const base::UnguessableToken> devtools_navigation_token,
    const GURL& outer_request_url,
    const network::mojom::URLResponseHead& outer_response,
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
    const network::mojom::URLResponseHead& head);
void OnSignedExchangeCertificateRequestCompleted(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const network::URLLoaderCompletionStatus& status);

void OnRequestWillBeSentExtraInfo(
    int process_id,
    int routing_id,
    const std::string& devtools_request_id,
    const net::CookieAccessResultList& request_cookie_list,
    const std::vector<network::mojom::HttpRawHeaderPairPtr>& request_headers);
void OnResponseReceivedExtraInfo(
    int process_id,
    int routing_id,
    const std::string& devtools_request_id,
    const net::CookieAndLineAccessResultList& response_cookie_list,
    const std::vector<network::mojom::HttpRawHeaderPairPtr>& response_headers,
    const base::Optional<std::string>& response_headers_text);
void OnCorsPreflightRequest(int32_t process_id,
                            int32_t render_frame_id,
                            const base::UnguessableToken& devtools_request_id,
                            const network::ResourceRequest& request,
                            const GURL& signed_exchange_url);
void OnCorsPreflightResponse(int32_t process_id,
                             int32_t render_frame_id,
                             const base::UnguessableToken& devtools_request_id,
                             const GURL& url,
                             network::mojom::URLResponseHeadPtr head);
void OnCorsPreflightRequestCompleted(
    int32_t process_id,
    int32_t render_frame_id,
    const base::UnguessableToken& devtools_request_id,
    const network::URLLoaderCompletionStatus& status);

std::vector<std::unique_ptr<NavigationThrottle>> CreateNavigationThrottles(
    NavigationHandle* navigation_handle);

bool ShouldWaitForDebuggerInWindowOpen();

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

void ReportSameSiteCookieIssue(
    RenderFrameHostImpl* render_frame_host_impl,
    const net::CookieWithAccessResult& excluded_cookie,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    blink::mojom::SameSiteCookieOperation operation,
    const base::Optional<std::string>& devtools_request_id);

// This function works similar to RenderFrameHostImpl::AddInspectorIssue, in
// that it reports an InspectorIssue to DevTools clients. The difference is that
// |ReportBrowserInitiatedIssue| sends issues directly to clients instead of
// going through the issue storage in the renderer process. Sending issues
// directly prevents them from being (potentially) lost during navigations.
//
// DevTools must be attached, otherwise issues reported through
// |ReportBrowserInitiatedIssue| are lost.
void CONTENT_EXPORT
ReportBrowserInitiatedIssue(RenderFrameHostImpl* frame,
                            protocol::Audits::InspectorIssue* issue);

// Produces a Heavy Ad Issue based on the parameters passed in.
std::unique_ptr<protocol::Audits::InspectorIssue> GetHeavyAdIssue(
    RenderFrameHostImpl* frame,
    blink::mojom::HeavyAdResolutionStatus resolution,
    blink::mojom::HeavyAdReason reason);

void OnQuicTransportHandshakeFailed(
    RenderFrameHostImpl* frame_host,
    const GURL& url,
    const base::Optional<net::QuicTransportError>& error);

void ApplyNetworkContextParamsOverrides(
    BrowserContext* browser_context,
    network::mojom::NetworkContextParams* network_context_params);

}  // namespace devtools_instrumentation

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_INSTRUMENTATION_H_
