// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_INSTRUMENTATION_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_INSTRUMENTATION_H_

/*
  The functions in this file are for routing instrumentation signals
  to the relevant set of devtools protocol handlers.
*/

#include <optional>
#include <vector>

#include "base/memory/stack_allocated.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_device_request_prompt_info.h"
#include "content/browser/devtools/devtools_throttle_handle.h"
#include "content/browser/interest_group/devtools_enums.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/common/content_export.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/filter/source_stream.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-forward.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-forward.h"

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
struct WebTransportError;
}  // namespace net

namespace network {
class URLLoaderFactoryBuilder;
}  // namespace network

namespace download {
struct DownloadCreateInfo;
class DownloadItem;
class DownloadUrlParameters;
}  // namespace download

namespace content {
class BackForwardCacheCanStoreDocumentResult;
class BrowserContext;
class DevToolsAgentHostImpl;
class FencedFrame;
class FrameTreeNode;
class NavigationHandle;
class NavigationRequest;
class NavigationThrottle;
class RenderFrameHostImpl;
class RenderProcessHost;
class SharedWorkerHost;
class ServiceWorkerContextWrapper;
class SignedExchangeEnvelope;
class StoragePartition;
class WebContents;
struct PrerenderMismatchedHeaders;

struct SignedExchangeError;

namespace protocol::Audits {
class InspectorIssue;
}  // namespace protocol::Audits

namespace devtools_instrumentation {

// Applies network request overrides to the auction worklet's network
// request. Will set `network_instrumentation_enabled` to true if there is a
// network handler listening. Also handles whether cache is disabled or not.
void ApplyAuctionNetworkRequestOverrides(FrameTreeNode* frame_tree_node,
                                         network::ResourceRequest* request,
                                         bool* network_instrumentation_enabled);

// If this function caused the User-Agent header to be overridden,
// `devtools_user_agent_overridden` will be set to true; otherwise, it will be
// set to false. If this function caused the Accept-Language header to be
// overridden, `devtools_accept_language_overridden` will be set to true;
// otherwise, it will be set to false.
void ApplyNetworkRequestOverrides(
    FrameTreeNode* frame_tree_node,
    blink::mojom::BeginNavigationParams* begin_params,
    bool* report_raw_headers,
    std::optional<std::vector<net::SourceStream::SourceType>>*
        devtools_accepted_stream_types,
    bool* devtools_user_agent_overridden,
    bool* devtools_accept_language_overridden);

// Returns true if devtools want |*override_out| to be used.
// (A true return and |*override_out| being nullopt means no user agent client
//  hints should be sent; a false return means devtools doesn't want to affect
//  the behavior).
bool ApplyUserAgentMetadataOverrides(
    FrameTreeNode* frame_tree_node,
    std::optional<blink::UserAgentMetadata>* override_out);

class WillCreateURLLoaderFactoryParams final {
  STACK_ALLOCATED();

 public:
  static WillCreateURLLoaderFactoryParams ForFrame(RenderFrameHostImpl* rfh);

  static WillCreateURLLoaderFactoryParams ForServiceWorker(
      RenderProcessHost& rph,
      int routing_id);

  static std::optional<WillCreateURLLoaderFactoryParams>
  ForServiceWorkerMainScript(const ServiceWorkerContextWrapper* context_wrapper,
                             std::optional<int64_t> version_id);

  static std::optional<WillCreateURLLoaderFactoryParams> ForSharedWorker(
      SharedWorkerHost* host);

  static WillCreateURLLoaderFactoryParams ForWorkerMainScript(
      DevToolsAgentHostImpl* agent_host,
      const base::UnguessableToken& worker_token,
      RenderFrameHostImpl& ancestor_render_frame_host);

  // Calls devtools hooks so that they can add interceptors.
  bool Run(bool is_navigation,
           bool is_download,
           network::URLLoaderFactoryBuilder& factory_builder,
           network::mojom::URLLoaderFactoryOverridePtr* factory_override);

 private:
  WillCreateURLLoaderFactoryParams(DevToolsAgentHostImpl* agent_host,
                                   const base::UnguessableToken& devtools_token,
                                   int process_id,
                                   StoragePartition* storage_partition);

  const raw_ptr<DevToolsAgentHostImpl> agent_host_;
  const base::UnguessableToken devtools_token_;
  const int process_id_;
  const raw_ptr<StoragePartition> storage_partition_;
};

void OnPrefetchRequestWillBeSent(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const GURL& initiator,
    const network::ResourceRequest& request,
    std::optional<std::pair<const GURL&,
                            const network::mojom::URLResponseHeadDevToolsInfo&>>
        redirect_info);
void OnPrefetchResponseReceived(FrameTreeNode* frame_tree_node,
                                const std::string& request_id,
                                const GURL& url,
                                const network::mojom::URLResponseHead& head);
void OnPrefetchRequestComplete(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status);
void OnPrefetchBodyDataReceived(FrameTreeNode* frame_tree_node,
                                const std::string& request_id,
                                const std::string& body,
                                bool is_base64_encoded);

void OnResetNavigationRequest(NavigationRequest* navigation_request);
void MaybeAssignResourceRequestId(FrameTreeNode* ftn,
                                  const std::string& id,
                                  network::ResourceRequest& request);
void OnNavigationRequestWillBeSent(const NavigationRequest& navigation_request);
void OnNavigationResponseReceived(
    const NavigationRequest& nav_request,
    const network::mojom::URLResponseHead& response);
void OnNavigationRequestFailed(
    const NavigationRequest& nav_request,
    const network::URLLoaderCompletionStatus& status);

// Logs fetch keepalive requests proxied via browser to Network panel.
//
// As the implementation requires a RenderFrameHost to locate a
// RenderFrameDevToolsAgentHost to attach the logs to, `frame_free_node` must
// not be nullptr. This doesn't really fit the whole need as such requests may
// be sent after RenderFrameHost unload.
//
// Caller also needs to make sure to avoid duplicated logging that may already
// happens in the request initiator renderer.
void OnFetchKeepAliveRequestWillBeSent(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const network::ResourceRequest& request,
    std::optional<std::pair<const GURL&,
                            const network::mojom::URLResponseHeadDevToolsInfo&>>
        redirect_info = std::nullopt);
void OnFetchKeepAliveResponseReceived(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const GURL& url,
    const network::mojom::URLResponseHead& head);
void OnFetchKeepAliveRequestComplete(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status);

void OnAuctionWorkletNetworkRequestWillBeSent(
    FrameTreeNodeId frame_tree_node_id,
    const network::ResourceRequest& request,
    base::TimeTicks timestamp);

void OnAuctionWorkletNetworkResponseReceived(
    FrameTreeNodeId frame_tree_node_id,
    const std::string& request_id,
    const std::string& loader_id,
    const GURL& request_url,
    const network::mojom::URLResponseHead& headers);

void OnAuctionWorkletNetworkRequestComplete(
    FrameTreeNodeId frame_tree_node_id,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status);

bool NeedInterestGroupAuctionEvents(FrameTreeNodeId frame_tree_node_id);

void OnInterestGroupAuctionEventOccurred(
    FrameTreeNodeId frame_tree_node_id,
    base::Time event_time,
    content::InterestGroupAuctionEventType type,
    const std::string& unique_auction_id,
    base::optional_ref<const std::string> parent_auction_id,
    const base::Value::Dict& auction_config);
void OnInterestGroupAuctionNetworkRequestCreated(
    FrameTreeNodeId frame_tree_node_id,
    content::InterestGroupAuctionFetchType type,
    const std::string& request_id,
    const std::vector<std::string>& devtools_auction_ids);

bool ShouldBypassCSP(const NavigationRequest& nav_request);
bool ShouldBypassCertificateErrors();

void ApplyNetworkOverridesForDownload(
    RenderFrameHostImpl* rfh,
    download::DownloadUrlParameters* parameters);
void WillBeginDownload(download::DownloadCreateInfo* info,
                       download::DownloadItem* item);

void BackForwardCacheNotUsed(
    const NavigationRequest* nav_request,
    const BackForwardCacheCanStoreDocumentResult* result,
    const BackForwardCacheCanStoreTreeResult* tree_result);

void WillSwapFrameTreeNode(FrameTreeNode& old_node, FrameTreeNode& new_node);
void OnFrameTreeNodeDestroyed(FrameTreeNode& frame_tree_node);

bool IsPrerenderAllowed(FrameTree& frame_tree);
void WillInitiatePrerender(FrameTree& frame_tree);
void DidActivatePrerender(const NavigationRequest& nav_request,
                          const std::optional<base::UnguessableToken>&
                              initiator_devtools_navigation_token);

void DidUpdatePolicyContainerHost(FrameTreeNode* ftn);

void DidUpdatePrefetchStatus(
    FrameTreeNode* ftn,
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const GURL& prefetch_url,
    PreloadingTriggeringOutcome status,
    PrefetchStatus prefetch_status,
    const std::string& request_id);

void DidUpdatePrerenderStatus(
    FrameTreeNodeId initiator_frame_tree_node_id,
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const GURL& prerender_url,
    std::optional<blink::mojom::SpeculationTargetHint> target_hint,
    PreloadingTriggeringOutcome status,
    std::optional<PrerenderFinalStatus> prerender_status,
    std::optional<std::string> disallowed_mojo_interface,
    const std::vector<PrerenderMismatchedHeaders>* mismatched_headers);

void DidUpdateSpeculationCandidates(
    RenderFrameHost& rfh,
    const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

void OnSignedExchangeReceived(
    FrameTreeNode* frame_tree_node,
    std::optional<const base::UnguessableToken> devtools_navigation_token,
    const GURL& outer_request_url,
    const network::mojom::URLResponseHead& outer_response,
    const std::optional<SignedExchangeEnvelope>& header,
    const scoped_refptr<net::X509Certificate>& certificate,
    const std::optional<net::SSLInfo>& ssl_info,
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

std::vector<std::unique_ptr<NavigationThrottle>> CreateNavigationThrottles(
    NavigationHandle* navigation_handle);

// When registering a new ServiceWorker with PlzServiceWorker, the main script
// fetch happens before starting the worker. This means that we need to give
// TargetHandlers the opportunity to attach to newly created ServiceWorker
// before the script fetch begins if they specified blocking auto-attach
// properties. The `throttle` controls when the script fetch resumes.
//
// Note on the input parameters:
// - `wrapper` and `version_id` are used to identify an existing newly
//   installing service worker agent. It is expected to exist.
// - `requesting_frame_id` is required, because the auto attacher is the one of
//   the frame registering the worker.
void ThrottleServiceWorkerMainScriptFetch(
    ServiceWorkerContextWrapper* wrapper,
    int64_t version_id,
    const GlobalRenderFrameHostId& requesting_frame_id,
    scoped_refptr<DevToolsThrottleHandle> throttle_handle);

// For PlzDedicatedWorker. When creating a new DedicatedWorker with
// PlzDedicatedWorker, the worker script fetch happens before starting the
// worker. This function is called when DedicatedWorkerHost, which is the
// representation of a worker in the browser process, is created.
// `throttle_handle` controls when the script fetch resumes.
void ThrottleWorkerMainScriptFetch(
    const base::UnguessableToken& devtools_worker_token,
    const GlobalRenderFrameHostId& ancestor_render_frame_host_id,
    scoped_refptr<DevToolsThrottleHandle> throttle_handle);

bool ShouldWaitForDebuggerInWindowOpen();

void WillStartDragging(FrameTreeNode* main_frame_tree_node,
                       const content::DropData& drop_data,
                       const blink::mojom::DragDataPtr drag_data,
                       blink::DragOperationsMask drag_operations_mask,
                       bool* intercepted);

void DragEnded(FrameTreeNode& node);

// Asks any interested agents to handle the given certificate error. Returns
// |true| if the error was handled, |false| otherwise.
using CertErrorCallback =
    base::RepeatingCallback<void(content::CertificateRequestResultType)>;
bool HandleCertificateError(WebContents* web_contents,
                            int cert_error,
                            const GURL& request_url,
                            CertErrorCallback callback);

void FencedFrameCreated(
    base::SafeRef<RenderFrameHostImpl> owner_render_frame_host,
    FencedFrame* fenced_frame);

void ReportCookieIssue(
    RenderFrameHostImpl* render_frame_host_impl,
    const network::mojom::CookieOrLineWithAccessResultPtr& excluded_cookie,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    blink::mojom::CookieOperation operation,
    const std::optional<std::string>& devtools_request_id,
    const std::optional<std::string>& devtools_issue_id);

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

// Produces an inspector issue and sends it to the client with
// |ReportBrowserInitiatedIssue|.
void BuildAndReportBrowserInitiatedIssue(
    RenderFrameHostImpl* frame,
    blink::mojom::InspectorIssueInfoPtr info);

void OnWebTransportHandshakeFailed(
    RenderFrameHostImpl* frame_host,
    const GURL& url,
    const std::optional<net::WebTransportError>& error);

void OnServiceWorkerMainScriptFetchingFailed(
    const GlobalRenderFrameHostId& requesting_frame_id,
    const ServiceWorkerContextWrapper* context_wrapper,
    int64_t version_id,
    const std::string& error,
    const network::URLLoaderCompletionStatus& status,
    const network::mojom::URLResponseHead* response_head,
    const GURL& url);
void OnServiceWorkerMainScriptRequestWillBeSent(
    const GlobalRenderFrameHostId& requesting_frame_id,
    const ServiceWorkerContextWrapper* context_wrapper,
    int64_t version_id,
    network::ResourceRequest& request);

// Fires `Network.onRequestWillBeSent` event for a dedicated worker and shared
// worker main script. Used for PlzDedicatedWorker/PlzSharedWorker.
void OnWorkerMainScriptRequestWillBeSent(
    FrameTreeNode* ftn,
    const base::UnguessableToken& worker_token,
    network::ResourceRequest& request);

// Fires `Network.onLoadingFailed` event for a dedicated worker main script.
// Used for PlzDedicatedWorker.
void OnWorkerMainScriptLoadingFailed(
    const GURL& url,
    const base::UnguessableToken& worker_token,
    FrameTreeNode* ftn,
    RenderFrameHostImpl* ancestor_rfh,
    const network::URLLoaderCompletionStatus& status);

// Fires `Network.onLoadingFinished` event for a dedicated worker main script.
// Used for PlzDedicatedWorker.
void OnWorkerMainScriptLoadingFinished(
    FrameTreeNode* ftn,
    const base::UnguessableToken& worker_token,
    const network::URLLoaderCompletionStatus& status);

// Adds a message from a worklet to the devtools console. This is specific to
// FLEDGE auction worklet and shared storage worklet where the message may
// contain sensitive cross-origin information, and therefore the devtools
// logging needs to bypass the usual path through the renderer.
void CONTENT_EXPORT
LogWorkletMessage(RenderFrameHostImpl& frame_host,
                  blink::mojom::ConsoleMessageLevel log_level,
                  const std::string& message);

void ApplyNetworkContextParamsOverrides(
    BrowserContext* browser_context,
    network::mojom::NetworkContextParams* network_context_params);

void DidRejectCrossOriginPortalMessage(
    RenderFrameHostImpl* render_frame_host_impl);

void UpdateDeviceRequestPrompt(RenderFrameHost* render_frame_host,
                               DevtoolsDeviceRequestPromptInfo* prompt_info);

void CleanUpDeviceRequestPrompt(RenderFrameHost* render_frame_host,
                                DevtoolsDeviceRequestPromptInfo* prompt_info);

// Notifies the active FedCmHandlers that a FedCM request is starting.
// `intercept` should be set to true if the handler is active.
// `disable_delay` should be set to true if the handler wants to disable
// the normal FedCM delay in notifying the renderer of success/failure.
void WillSendFedCmRequest(RenderFrameHost& render_frame_host,
                          bool* intercept,
                          bool* disable_delay);
void WillShowFedCmDialog(RenderFrameHost& render_frame_host, bool* intercept);
void DidShowFedCmDialog(RenderFrameHost& render_frame_host);
void DidCloseFedCmDialog(RenderFrameHost& render_frame_host);

// Handles dev tools integration for fenced frame reporting beacons. Used in
// `FencedFrameReporter`.
void OnFencedFrameReportRequestSent(
    FrameTreeNodeId initiator_frame_tree_node_id,
    const std::string& devtools_request_id,
    network::ResourceRequest& request,
    const std::string& event_data);
void OnFencedFrameReportResponseReceived(
    FrameTreeNodeId initiator_frame_tree_node_id,
    const std::string& devtools_request_id,
    const GURL& final_url,
    scoped_refptr<net::HttpResponseHeaders> headers);

void DidChangeFrameLoadingState(FrameTreeNode& ftn);

}  // namespace devtools_instrumentation

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_INSTRUMENTATION_H_
