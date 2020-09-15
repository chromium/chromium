// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/devtools/devtools_instrumentation.h"

#include "base/strings/stringprintf.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_item.h"
#include "content/browser/devtools/browser_devtools_agent_host.h"
#include "content/browser/devtools/devtools_issue_storage.h"
#include "content/browser/devtools/devtools_url_loader_interceptor.h"
#include "content/browser/devtools/protocol/audits.h"
#include "content/browser/devtools/protocol/audits_handler.h"
#include "content/browser/devtools/protocol/browser_handler.h"
#include "content/browser/devtools/protocol/emulation_handler.h"
#include "content/browser/devtools/protocol/fetch_handler.h"
#include "content/browser/devtools/protocol/log_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/protocol/security_handler.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/load_flags.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/http/http_request_headers.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/quic/quic_transport_error.h"
#include "net/ssl/ssl_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"

namespace content {
namespace devtools_instrumentation {

namespace {

template <typename Handler, typename... MethodArgs, typename... Args>
void DispatchToAgents(FrameTreeNode* frame_tree_node,
                      void (Handler::*method)(MethodArgs...),
                      Args&&... args) {
  DevToolsAgentHostImpl* agent_host =
      RenderFrameDevToolsAgentHost::GetFor(frame_tree_node);
  if (!agent_host)
    return;
  for (auto* h : Handler::ForAgentHost(agent_host))
    (h->*method)(std::forward<Args>(args)...);
}

template <typename Handler, typename... MethodArgs, typename... Args>
void DispatchToAgents(int frame_tree_node_id,
                      void (Handler::*method)(MethodArgs...),
                      Args&&... args) {
  FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (ftn)
    DispatchToAgents(ftn, method, std::forward<Args>(args)...);
}

template <typename Handler, typename... MethodArgs, typename... Args>
void DispatchToWorkerAgents(int32_t worker_process_id,
                            int32_t worker_route_id,
                            void (Handler::*method)(MethodArgs...),
                            Args&&... args) {
  ServiceWorkerDevToolsAgentHost* service_worker_host =
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForWorker(worker_process_id, worker_route_id);
  if (!service_worker_host)
    return;
  for (auto* h : Handler::ForAgentHost(service_worker_host))
    (h->*method)(std::forward<Args>(args)...);

  // TODO(crbug.com/1004979): Look for shared worker hosts here as well.
}

FrameTreeNode* GetFtnForNetworkRequest(int process_id, int routing_id) {
  // Navigation requests start in the browser, before process_id is assigned, so
  // the id is set to 0. In these situations, the routing_id is the frame tree
  // node id, and can be used directly.
  if (process_id == 0) {
    return FrameTreeNode::GloballyFindByID(routing_id);
  }
  return FrameTreeNode::GloballyFindByID(
      RenderFrameHost::GetFrameTreeNodeIdForRoutingId(process_id, routing_id));
}

std::unique_ptr<protocol::Audits::HeavyAdIssueDetails> GetHeavyAdIssueHelper(
    RenderFrameHostImpl* frame,
    blink::mojom::HeavyAdResolutionStatus resolution,
    blink::mojom::HeavyAdReason reason) {
  protocol::String status =
      (resolution == blink::mojom::HeavyAdResolutionStatus::kHeavyAdBlocked)
          ? protocol::Audits::HeavyAdResolutionStatusEnum::HeavyAdBlocked
          : protocol::Audits::HeavyAdResolutionStatusEnum::HeavyAdWarning;
  protocol::String reason_string;
  switch (reason) {
    case blink::mojom::HeavyAdReason::kNetworkTotalLimit:
      reason_string = protocol::Audits::HeavyAdReasonEnum::NetworkTotalLimit;
      break;
    case blink::mojom::HeavyAdReason::kCpuTotalLimit:
      reason_string = protocol::Audits::HeavyAdReasonEnum::CpuTotalLimit;
      break;
    case blink::mojom::HeavyAdReason::kCpuPeakLimit:
      reason_string = protocol::Audits::HeavyAdReasonEnum::CpuPeakLimit;
      break;
  }
  return protocol::Audits::HeavyAdIssueDetails::Create()
      .SetReason(reason_string)
      .SetResolution(status)
      .SetFrame(protocol::Audits::AffectedFrame::Create()
                    .SetFrameId(frame->GetDevToolsFrameToken().ToString())
                    .Build())
      .Build();
}

}  // namespace

void OnResetNavigationRequest(NavigationRequest* navigation_request) {
  // Traverse frame chain all the way to the top and report to all
  // page handlers that the navigation completed.
  for (FrameTreeNode* node = navigation_request->frame_tree_node(); node;
       node = FrameTreeNode::From(node->parent())) {
    DispatchToAgents(node, &protocol::PageHandler::NavigationReset,
                     navigation_request);
  }
}

void OnNavigationResponseReceived(
    const NavigationRequest& nav_request,
    const network::mojom::URLResponseHead& response) {
  FrameTreeNode* ftn = nav_request.frame_tree_node();
  std::string id = nav_request.devtools_navigation_token().ToString();
  std::string frame_id = ftn->devtools_frame_token().ToString();
  GURL url = nav_request.common_params().url;
  DispatchToAgents(ftn, &protocol::NetworkHandler::ResponseReceived, id, id,
                   url, protocol::Network::ResourceTypeEnum::Document, response,
                   frame_id);
}

namespace {
protocol::String BuildBlockedByResponseReason(
    network::mojom::BlockedByResponseReason reason) {
  switch (reason) {
    case network::mojom::BlockedByResponseReason::
        kCoepFrameResourceNeedsCoepHeader:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CoepFrameResourceNeedsCoepHeader;
    case network::mojom::BlockedByResponseReason::
        kCoopSandboxedIFrameCannotNavigateToCoopPage:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CoopSandboxedIFrameCannotNavigateToCoopPage;
    case network::mojom::BlockedByResponseReason::kCorpNotSameOrigin:
      return protocol::Audits::BlockedByResponseReasonEnum::CorpNotSameOrigin;
    case network::mojom::BlockedByResponseReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoep:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByCoep;
    case network::mojom::BlockedByResponseReason::kCorpNotSameSite:
      return protocol::Audits::BlockedByResponseReasonEnum::CorpNotSameSite;
  }
}
}  // namespace

void OnNavigationRequestFailed(
    const NavigationRequest& nav_request,
    const network::URLLoaderCompletionStatus& status) {
  FrameTreeNode* ftn = nav_request.frame_tree_node();
  std::string id = nav_request.devtools_navigation_token().ToString();

  if (status.blocked_by_response_reason) {
    auto issueDetails = protocol::Audits::InspectorIssueDetails::Create();
    auto request =
        protocol::Audits::AffectedRequest::Create()
            .SetRequestId(id)
            .SetUrl(const_cast<NavigationRequest&>(nav_request).GetURL().spec())
            .Build();
    auto blockedByResponseDetails =
        protocol::Audits::BlockedByResponseIssueDetails::Create()
            .SetRequest(std::move(request))
            .SetReason(BuildBlockedByResponseReason(
                *status.blocked_by_response_reason))
            .Build();

    blockedByResponseDetails->SetBlockedFrame(
        protocol::Audits::AffectedFrame::Create()
            .SetFrameId(ftn->devtools_frame_token().ToString())
            .Build());
    if (ftn->parent()) {
      blockedByResponseDetails->SetParentFrame(
          protocol::Audits::AffectedFrame::Create()
              .SetFrameId(ftn->parent()
                              ->frame_tree_node()
                              ->devtools_frame_token()
                              .ToString())
              .Build());
    }
    issueDetails.SetBlockedByResponseIssueDetails(
        std::move(blockedByResponseDetails));

    auto inspector_issue =
        protocol::Audits::InspectorIssue::Create()
            .SetCode(protocol::Audits::InspectorIssueCodeEnum::
                         BlockedByResponseIssue)
            .SetDetails(issueDetails.Build())
            .Build();

    ReportBrowserInitiatedIssue(ftn->current_frame_host(),
                                inspector_issue.get());
  }

  DispatchToAgents(ftn, &protocol::NetworkHandler::LoadingComplete, id,
                   protocol::Network::ResourceTypeEnum::Document, status);
}

void WillBeginDownload(download::DownloadCreateInfo* info,
                       download::DownloadItem* item) {
  if (!item)
    return;
  auto* rfh = static_cast<RenderFrameHostImpl*>(
      RenderFrameHost::FromID(info->render_process_id, info->render_frame_id));
  FrameTreeNode* ftn =
      rfh ? FrameTreeNode::GloballyFindByID(rfh->GetFrameTreeNodeId())
          : nullptr;
  if (!ftn)
    return;
  DispatchToAgents(ftn, &protocol::PageHandler::DownloadWillBegin, ftn, item);
}

void OnSignedExchangeReceived(
    FrameTreeNode* frame_tree_node,
    base::Optional<const base::UnguessableToken> devtools_navigation_token,
    const GURL& outer_request_url,
    const network::mojom::URLResponseHead& outer_response,
    const base::Optional<SignedExchangeEnvelope>& envelope,
    const scoped_refptr<net::X509Certificate>& certificate,
    const base::Optional<net::SSLInfo>& ssl_info,
    const std::vector<SignedExchangeError>& errors) {
  DispatchToAgents(frame_tree_node,
                   &protocol::NetworkHandler::OnSignedExchangeReceived,
                   devtools_navigation_token, outer_request_url, outer_response,
                   envelope, certificate, ssl_info, errors);
}

namespace inspector_will_send_navigation_request_event {
std::unique_ptr<base::trace_event::TracedValue> Data(
    const base::UnguessableToken& request_id) {
  auto value = std::make_unique<base::trace_event::TracedValue>();
  value->SetString("requestId", request_id.ToString());
  return value;
}
}  // namespace inspector_will_send_navigation_request_event

void OnSignedExchangeCertificateRequestSent(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const base::UnguessableToken& loader_id,
    const network::ResourceRequest& request,
    const GURL& signed_exchange_url) {
  // Make sure both back-ends yield the same timestamp.
  auto timestamp = base::TimeTicks::Now();
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::RequestSent,
                   request_id.ToString(), loader_id.ToString(), request,
                   protocol::Network::Initiator::TypeEnum::SignedExchange,
                   signed_exchange_url, timestamp);

  auto value = std::make_unique<base::trace_event::TracedValue>();
  value->SetString("requestId", request_id.ToString());
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "devtools.timeline", "ResourceWillSendRequest", TRACE_EVENT_SCOPE_PROCESS,
      timestamp, "data",
      inspector_will_send_navigation_request_event::Data(request_id));
}

void OnSignedExchangeCertificateResponseReceived(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const base::UnguessableToken& loader_id,
    const GURL& url,
    const network::mojom::URLResponseHead& head) {
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::ResponseReceived,
                   request_id.ToString(), loader_id.ToString(), url,
                   protocol::Network::ResourceTypeEnum::Other, head,
                   protocol::Maybe<std::string>());
}

void OnSignedExchangeCertificateRequestCompleted(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const network::URLLoaderCompletionStatus& status) {
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::LoadingComplete,
                   request_id.ToString(),
                   protocol::Network::ResourceTypeEnum::Other, status);
}

void CreateThrottlesForAgentHost(
    DevToolsAgentHostImpl* agent_host,
    NavigationHandle* navigation_handle,
    std::vector<std::unique_ptr<NavigationThrottle>>* result) {
  for (auto* target_handler :
       protocol::TargetHandler::ForAgentHost(agent_host)) {
    std::unique_ptr<NavigationThrottle> throttle =
        target_handler->CreateThrottleForNavigation(navigation_handle);
    if (throttle)
      result->push_back(std::move(throttle));
  }
}

std::vector<std::unique_ptr<NavigationThrottle>> CreateNavigationThrottles(
    NavigationHandle* navigation_handle) {
  FrameTreeNode* frame_tree_node =
      NavigationRequest::From(navigation_handle)->frame_tree_node();
  FrameTreeNode* parent = FrameTreeNode::From(frame_tree_node->parent());
  if (!parent) {
    if (WebContentsImpl::FromFrameTreeNode(frame_tree_node)->IsPortal() &&
        WebContentsImpl::FromFrameTreeNode(frame_tree_node)
            ->GetOuterWebContents()) {
      parent = WebContentsImpl::FromFrameTreeNode(frame_tree_node)
                   ->GetOuterWebContents()
                   ->GetFrameTree()
                   ->root();
    }
  }

  std::vector<std::unique_ptr<NavigationThrottle>> result;
  if (parent) {
    DevToolsAgentHostImpl* agent_host =
        RenderFrameDevToolsAgentHost::GetFor(parent);
    if (agent_host)
      CreateThrottlesForAgentHost(agent_host, navigation_handle, &result);
  } else {
    for (auto* browser_agent_host : BrowserDevToolsAgentHost::Instances())
      CreateThrottlesForAgentHost(browser_agent_host, navigation_handle,
                                  &result);
  }

  return result;
}

bool ShouldWaitForDebuggerInWindowOpen() {
  for (auto* browser_agent_host : BrowserDevToolsAgentHost::Instances()) {
    for (auto* target_handler :
         protocol::TargetHandler::ForAgentHost(browser_agent_host)) {
      if (target_handler->ShouldThrottlePopups())
        return true;
    }
  }
  return false;
}

void ApplyNetworkRequestOverrides(FrameTreeNode* frame_tree_node,
                                  mojom::BeginNavigationParams* begin_params,
                                  bool* report_raw_headers) {
  bool disable_cache = false;
  DevToolsAgentHostImpl* agent_host =
      RenderFrameDevToolsAgentHost::GetFor(frame_tree_node);
  if (!agent_host)
    return;
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(begin_params->headers);
  for (auto* network : protocol::NetworkHandler::ForAgentHost(agent_host)) {
    if (!network->enabled())
      continue;
    *report_raw_headers = true;
    network->ApplyOverrides(&headers, &begin_params->skip_service_worker,
                            &disable_cache);
  }

  for (auto* emulation : protocol::EmulationHandler::ForAgentHost(agent_host))
    emulation->ApplyOverrides(&headers);

  if (disable_cache) {
    begin_params->load_flags &=
        ~(net::LOAD_VALIDATE_CACHE | net::LOAD_SKIP_CACHE_VALIDATION |
          net::LOAD_ONLY_FROM_CACHE | net::LOAD_DISABLE_CACHE);
    begin_params->load_flags |= net::LOAD_BYPASS_CACHE;
  }

  begin_params->headers = headers.ToString();
}

bool ApplyUserAgentMetadataOverrides(
    FrameTreeNode* frame_tree_node,
    base::Optional<blink::UserAgentMetadata>* override_out) {
  DevToolsAgentHostImpl* agent_host =
      RenderFrameDevToolsAgentHost::GetFor(frame_tree_node);
  if (!agent_host)
    return false;

  bool result = false;
  for (auto* emulation : protocol::EmulationHandler::ForAgentHost(agent_host))
    result = emulation->ApplyUserAgentMetadataOverrides(override_out) || result;

  return result;
}

namespace {
template <typename HandlerType>
bool MaybeCreateProxyForInterception(
    DevToolsAgentHostImpl* agent_host,
    RenderProcessHost* rph,
    const base::UnguessableToken& frame_token,
    bool is_navigation,
    bool is_download,
    network::mojom::URLLoaderFactoryOverride* agent_override) {
  if (!agent_host)
    return false;
  bool had_interceptors = false;
  const auto& handlers = HandlerType::ForAgentHost(agent_host);
  for (auto it = handlers.rbegin(); it != handlers.rend(); ++it) {
    had_interceptors =
        (*it)->MaybeCreateProxyForInterception(rph, frame_token, is_navigation,
                                               is_download, agent_override) ||
        had_interceptors;
  }
  return had_interceptors;
}

}  // namespace

bool WillCreateURLLoaderFactory(
    RenderFrameHostImpl* rfh,
    bool is_navigation,
    bool is_download,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
        target_factory_receiver,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  DCHECK(!is_download || is_navigation);

  network::mojom::URLLoaderFactoryOverride devtools_override;
  // If caller passed some existing overrides, use those.
  // Otherwise, use our local var, then if handlers actually
  // decide to intercept, move it to |factory_override|.
  network::mojom::URLLoaderFactoryOverride* handler_override =
      factory_override && *factory_override ? factory_override->get()
                                            : &devtools_override;

  // Order of targets and sessions matters -- the latter proxy is created,
  // the closer it is to the network. So start with frame's NetworkHandler,
  // then process frame's FetchHandler and then browser's FetchHandler.
  // Within the target, the agents added earlier are closer to network.
  DevToolsAgentHostImpl* frame_agent_host =
      RenderFrameDevToolsAgentHost::GetFor(rfh);
  RenderProcessHost* rph = rfh->GetProcess();
  const base::UnguessableToken& frame_token = rfh->GetDevToolsFrameToken();

  bool had_interceptors =
      MaybeCreateProxyForInterception<protocol::NetworkHandler>(
          frame_agent_host, rph, frame_token, is_navigation, is_download,
          handler_override);

  had_interceptors = MaybeCreateProxyForInterception<protocol::FetchHandler>(
                         frame_agent_host, rph, frame_token, is_navigation,
                         is_download, handler_override) ||
                     had_interceptors;

  // TODO(caseq): assure deterministic order of browser agents (or sessions).
  for (auto* browser_agent_host : BrowserDevToolsAgentHost::Instances()) {
    had_interceptors = MaybeCreateProxyForInterception<protocol::FetchHandler>(
                           browser_agent_host, rph, frame_token, is_navigation,
                           is_download, handler_override) ||
                       had_interceptors;
  }
  if (!had_interceptors)
    return false;
  DCHECK(handler_override->overriding_factory);
  DCHECK(handler_override->overridden_factory_receiver);
  if (!factory_override) {
    // Not a subresource navigation, so just override the target receiver.
    mojo::FusePipes(std::move(*target_factory_receiver),
                    std::move(devtools_override.overriding_factory));
    *target_factory_receiver =
        std::move(devtools_override.overridden_factory_receiver);
  } else if (!*factory_override) {
    // No other overrides, so just returns ours as is.
    *factory_override = network::mojom::URLLoaderFactoryOverride::New(
        std::move(devtools_override.overriding_factory),
        std::move(devtools_override.overridden_factory_receiver), false);
  }
  // ... else things are already taken care of, as handler_override was pointing
  // to factory override and we've done all magic in-place.
  DCHECK(!devtools_override.overriding_factory);
  DCHECK(!devtools_override.overridden_factory_receiver);

  return true;
}

bool WillCreateURLLoaderFactoryForServiceWorker(
    RenderProcessHost* rph,
    int routing_id,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  DCHECK(factory_override);

  ServiceWorkerDevToolsAgentHost* worker_agent_host =
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForWorker(rph->GetID(), routing_id);
  if (!worker_agent_host) {
    NOTREACHED();
    return false;
  }
  network::mojom::URLLoaderFactoryOverride devtools_override;
  // If caller passed some existing overrides, use those.
  // Otherwise, use our local var, then if handlers actually
  // decide to intercept, move it to |factory_override|.
  network::mojom::URLLoaderFactoryOverride* handler_override =
      *factory_override ? factory_override->get() : &devtools_override;

  const base::UnguessableToken& worker_token =
      worker_agent_host->devtools_worker_token();

  bool had_interceptors =
      MaybeCreateProxyForInterception<protocol::FetchHandler>(
          worker_agent_host, rph, worker_token, false, false, handler_override);

  // TODO(caseq): assure deterministic order of browser agents (or sessions).
  for (auto* browser_agent_host : BrowserDevToolsAgentHost::Instances()) {
    had_interceptors = MaybeCreateProxyForInterception<protocol::FetchHandler>(
                           browser_agent_host, rph, worker_token, false, false,
                           handler_override) ||
                       had_interceptors;
  }
  if (!had_interceptors)
    return false;

  DCHECK(handler_override->overriding_factory);
  DCHECK(handler_override->overridden_factory_receiver);
  if (!*factory_override) {
    *factory_override = network::mojom::URLLoaderFactoryOverride::New(
        std::move(devtools_override.overriding_factory),
        std::move(devtools_override.overridden_factory_receiver), false);
  }
  return true;
}

bool WillCreateURLLoaderFactory(
    RenderFrameHostImpl* rfh,
    bool is_navigation,
    bool is_download,
    std::unique_ptr<network::mojom::URLLoaderFactory>* factory) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> proxied_factory;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver =
      proxied_factory.InitWithNewPipeAndPassReceiver();
  if (!WillCreateURLLoaderFactory(rfh, is_navigation, is_download, &receiver,
                                  nullptr)) {
    return false;
  }
  mojo::MakeSelfOwnedReceiver(std::move(*factory), std::move(receiver));
  *factory = std::make_unique<DevToolsURLLoaderFactoryAdapter>(
      std::move(proxied_factory));
  return true;
}

void OnNavigationRequestWillBeSent(
    const NavigationRequest& navigation_request) {
  // Note this intentionally deviates from the usual instrumentation signal
  // logic and dispatches to all agents upwards from the frame, to make sure
  // the security checks are properly applied even if no DevTools session is
  // established for the navigated frame itself. This is because the page
  // agent may navigate all of its subframes currently.
  for (RenderFrameHostImpl* rfh =
           navigation_request.frame_tree_node()->current_frame_host();
       rfh; rfh = rfh->GetParent()) {
    // Only check frames that qualify as DevTools targets, i.e. (local)? roots.
    if (!RenderFrameDevToolsAgentHost::ShouldCreateDevToolsForHost(rfh))
      continue;
    auto* agent_host = static_cast<RenderFrameDevToolsAgentHost*>(
        RenderFrameDevToolsAgentHost::GetFor(rfh));
    if (!agent_host)
      continue;
    agent_host->OnNavigationRequestWillBeSent(navigation_request);
  }

  // Make sure both back-ends yield the same timestamp.
  auto timestamp = base::TimeTicks::Now();
  DispatchToAgents(navigation_request.frame_tree_node(),
                   &protocol::NetworkHandler::NavigationRequestWillBeSent,
                   navigation_request, timestamp);
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "devtools.timeline", "ResourceWillSendRequest", TRACE_EVENT_SCOPE_PROCESS,
      timestamp, "data",
      inspector_will_send_navigation_request_event::Data(
          navigation_request.devtools_navigation_token()));
}

// Notify the provided agent host of a certificate error. Returns true if one of
// the host's handlers will handle the certificate error.
bool NotifyCertificateError(DevToolsAgentHost* host,
                            int cert_error,
                            const GURL& request_url,
                            const CertErrorCallback& callback) {
  DevToolsAgentHostImpl* host_impl = static_cast<DevToolsAgentHostImpl*>(host);
  for (auto* security_handler :
       protocol::SecurityHandler::ForAgentHost(host_impl)) {
    if (security_handler->NotifyCertificateError(cert_error, request_url,
                                                 callback)) {
      return true;
    }
  }
  return false;
}

bool HandleCertificateError(WebContents* web_contents,
                            int cert_error,
                            const GURL& request_url,
                            CertErrorCallback callback) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetOrCreateFor(web_contents).get();
  if (NotifyCertificateError(agent_host.get(), cert_error, request_url,
                             callback)) {
    // Only allow a single agent host to handle the error.
    callback.Reset();
  }

  for (auto* browser_agent_host : BrowserDevToolsAgentHost::Instances()) {
    if (NotifyCertificateError(browser_agent_host, cert_error, request_url,
                               callback)) {
      // Only allow a single agent host to handle the error.
      callback.Reset();
    }
  }
  return !callback;
}

void PortalAttached(RenderFrameHostImpl* render_frame_host_impl) {
  DispatchToAgents(render_frame_host_impl->frame_tree_node(),
                   &protocol::TargetHandler::UpdatePortals);
}

void PortalDetached(RenderFrameHostImpl* render_frame_host_impl) {
  DispatchToAgents(render_frame_host_impl->frame_tree_node(),
                   &protocol::TargetHandler::UpdatePortals);
}

void PortalActivated(RenderFrameHostImpl* render_frame_host_impl) {
  DispatchToAgents(render_frame_host_impl->frame_tree_node(),
                   &protocol::TargetHandler::UpdatePortals);
}

void OnRequestWillBeSentExtraInfo(
    int process_id,
    int routing_id,
    const std::string& devtools_request_id,
    const net::CookieAccessResultList& request_cookie_list,
    const std::vector<network::mojom::HttpRawHeaderPairPtr>& request_headers) {
  FrameTreeNode* ftn = GetFtnForNetworkRequest(process_id, routing_id);
  if (ftn) {
    DispatchToAgents(ftn,
                     &protocol::NetworkHandler::OnRequestWillBeSentExtraInfo,
                     devtools_request_id, request_cookie_list, request_headers);
    return;
  }

  // In the case of service worker network requests, there is no
  // FrameTreeNode to use so instead we use the "routing_id" created with the
  // worker and sent to the renderer process to send as the render_frame_id in
  // the renderer's network::ResourceRequest which gets plubmed to here as
  // routing_id.
  DispatchToWorkerAgents(
      process_id, routing_id,
      &protocol::NetworkHandler::OnRequestWillBeSentExtraInfo,
      devtools_request_id, request_cookie_list, request_headers);
}

void OnResponseReceivedExtraInfo(
    int process_id,
    int routing_id,
    const std::string& devtools_request_id,
    const net::CookieAndLineAccessResultList& response_cookie_list,
    const std::vector<network::mojom::HttpRawHeaderPairPtr>& response_headers,
    const base::Optional<std::string>& response_headers_text) {
  FrameTreeNode* ftn = GetFtnForNetworkRequest(process_id, routing_id);
  if (ftn) {
    DispatchToAgents(ftn,
                     &protocol::NetworkHandler::OnResponseReceivedExtraInfo,
                     devtools_request_id, response_cookie_list,
                     response_headers, response_headers_text);
    return;
  }

  // See comment on DispatchToWorkerAgents in OnRequestWillBeSentExtraInfo.
  DispatchToWorkerAgents(process_id, routing_id,
                         &protocol::NetworkHandler::OnResponseReceivedExtraInfo,
                         devtools_request_id, response_cookie_list,
                         response_headers, response_headers_text);
}

void OnCorsPreflightRequest(int32_t process_id,
                            int32_t render_frame_id,
                            const base::UnguessableToken& devtools_request_id,
                            const network::ResourceRequest& request,
                            const GURL& initiator_url) {
  FrameTreeNode* ftn = GetFtnForNetworkRequest(process_id, render_frame_id);
  if (!ftn)
    return;
  auto timestamp = base::TimeTicks::Now();
  auto id = devtools_request_id.ToString();
  // TODO(crbug.com/941297): Currently we are using an empty string for
  // |loader_id|. But when we will introduce a better UI for preflight requests,
  // consider using the navigation token which is same as the |loader_id| of the
  // original request or the |devtools_request_id| of the original request, so
  // that we can associate the requests in the DevTools front end.
  DispatchToAgents(ftn, &protocol::NetworkHandler::RequestSent, id,
                   /* loader_id=*/"", request,
                   protocol::Network::Initiator::TypeEnum::Other, initiator_url,
                   timestamp);
}

void OnCorsPreflightResponse(int32_t process_id,
                             int32_t render_frame_id,
                             const base::UnguessableToken& devtools_request_id,
                             const GURL& url,
                             network::mojom::URLResponseHeadPtr head) {
  FrameTreeNode* ftn = GetFtnForNetworkRequest(process_id, render_frame_id);
  if (!ftn)
    return;
  auto id = devtools_request_id.ToString();
  DispatchToAgents(ftn, &protocol::NetworkHandler::ResponseReceived, id,
                   /* loader_id=*/"", url,
                   protocol::Network::ResourceTypeEnum::Other, *head,
                   protocol::Maybe<std::string>());
}

void OnCorsPreflightRequestCompleted(
    int32_t process_id,
    int32_t render_frame_id,
    const base::UnguessableToken& devtools_request_id,
    const network::URLLoaderCompletionStatus& status) {
  FrameTreeNode* ftn = GetFtnForNetworkRequest(process_id, render_frame_id);
  if (!ftn)
    return;
  auto id = devtools_request_id.ToString();
  DispatchToAgents(ftn, &protocol::NetworkHandler::LoadingComplete, id,
                   protocol::Network::ResourceTypeEnum::Other, status);
}

namespace {
std::unique_ptr<protocol::Array<protocol::String>> BuildExclusionReasons(
    net::CookieInclusionStatus status) {
  auto exclusion_reasons =
      std::make_unique<protocol::Array<protocol::String>>();
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::
              EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX)) {
    exclusion_reasons->push_back(
        protocol::Audits::SameSiteCookieExclusionReasonEnum::
            ExcludeSameSiteUnspecifiedTreatedAsLax);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE)) {
    exclusion_reasons->push_back(
        protocol::Audits::SameSiteCookieExclusionReasonEnum::
            ExcludeSameSiteNoneInsecure);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)) {
    exclusion_reasons->push_back(
        protocol::Audits::SameSiteCookieExclusionReasonEnum::
            ExcludeSameSiteLax);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)) {
    exclusion_reasons->push_back(
        protocol::Audits::SameSiteCookieExclusionReasonEnum::
            ExcludeSameSiteStrict);
  }

  return exclusion_reasons;
}

std::unique_ptr<protocol::Array<protocol::String>> BuildWarningReasons(
    net::CookieInclusionStatus status) {
  auto warning_reasons = std::make_unique<protocol::Array<protocol::String>>();
  if (status.HasWarningReason(
          net::CookieInclusionStatus::
              WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT)) {
    warning_reasons->push_back(
        protocol::Audits::SameSiteCookieWarningReasonEnum::
            WarnSameSiteUnspecifiedCrossSiteContext);
  }
  if (status.HasWarningReason(
          net::CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE)) {
    warning_reasons->push_back(
        protocol::Audits::SameSiteCookieWarningReasonEnum::
            WarnSameSiteNoneInsecure);
  }
  if (status.HasWarningReason(net::CookieInclusionStatus::
                                  WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE)) {
    warning_reasons->push_back(
        protocol::Audits::SameSiteCookieWarningReasonEnum::
            WarnSameSiteUnspecifiedLaxAllowUnsafe);
  }

  // If schemeful messages are disabled, don't add a warning for them.
  if (!base::FeatureList::IsEnabled(features::kCookieDeprecationMessages))
    return warning_reasons;

  // There can only be one of the following warnings.
  if (status.HasWarningReason(net::CookieInclusionStatus::
                                  WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE)) {
    warning_reasons->push_back(
        protocol::Audits::SameSiteCookieWarningReasonEnum::
            WarnSameSiteStrictLaxDowngradeStrict);
  } else if (status.HasWarningReason(
                 net::CookieInclusionStatus::
                     WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE)) {
    warning_reasons->push_back(
        protocol::Audits::SameSiteCookieWarningReasonEnum::
            WarnSameSiteStrictCrossDowngradeStrict);
  } else if (status.HasWarningReason(
                 net::CookieInclusionStatus::
                     WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE)) {
    warning_reasons->push_back(
        protocol::Audits::SameSiteCookieWarningReasonEnum::
            WarnSameSiteStrictCrossDowngradeLax);
  } else if (status.HasWarningReason(
                 net::CookieInclusionStatus::
                     WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE)) {
    warning_reasons->push_back(
        protocol::Audits::SameSiteCookieWarningReasonEnum::
            WarnSameSiteLaxCrossDowngradeStrict);
  } else if (status.HasWarningReason(
                 net::CookieInclusionStatus::
                     WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE)) {
    warning_reasons->push_back(
        protocol::Audits::SameSiteCookieWarningReasonEnum::
            WarnSameSiteLaxCrossDowngradeLax);
  }

  return warning_reasons;
}

protocol::String BuildCookieOperation(
    blink::mojom::SameSiteCookieOperation operation) {
  switch (operation) {
    case blink::mojom::SameSiteCookieOperation::kReadCookie:
      return protocol::Audits::SameSiteCookieOperationEnum::ReadCookie;
    case blink::mojom::SameSiteCookieOperation::kSetCookie:
      return protocol::Audits::SameSiteCookieOperationEnum::SetCookie;
  }
}

}  // namespace

void ReportSameSiteCookieIssue(
    RenderFrameHostImpl* render_frame_host_impl,
    const net::CookieWithAccessResult& excluded_cookie,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    blink::mojom::SameSiteCookieOperation operation,
    const base::Optional<std::string>& devtools_request_id) {
  std::unique_ptr<protocol::Audits::AffectedRequest> affected_request;
  if (devtools_request_id) {
    // We can report the url here, because if devtools_request_id is set, the
    // url is the url of the request.
    affected_request = protocol::Audits::AffectedRequest::Create()
                           .SetRequestId(*devtools_request_id)
                           .SetUrl(url.spec())
                           .Build();
  }

  auto affected_cookie = protocol::Audits::AffectedCookie::Create()
                             .SetName(excluded_cookie.cookie.Name())
                             .SetPath(excluded_cookie.cookie.Path())
                             .SetDomain(excluded_cookie.cookie.Domain())
                             .Build();

  auto same_site_details =
      protocol::Audits::SameSiteCookieIssueDetails::Create()
          .SetCookie(std::move(affected_cookie))
          .SetCookieExclusionReasons(
              BuildExclusionReasons(excluded_cookie.access_result.status))
          .SetCookieWarningReasons(
              BuildWarningReasons(excluded_cookie.access_result.status))
          .SetOperation(BuildCookieOperation(operation))
          .SetCookieUrl(url.spec())
          .SetRequest(std::move(affected_request))
          .Build();

  if (!site_for_cookies.IsNull()) {
    same_site_details->SetSiteForCookies(
        site_for_cookies.RepresentativeUrl().spec());
  }

  auto details =
      protocol::Audits::InspectorIssueDetails::Create()
          .SetSameSiteCookieIssueDetails(std::move(same_site_details))
          .Build();

  auto issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(
              protocol::Audits::InspectorIssueCodeEnum::SameSiteCookieIssue)
          .SetDetails(std::move(details))
          .Build();

  ReportBrowserInitiatedIssue(render_frame_host_impl, issue.get());
}

namespace {

void AddIssueToIssueStorage(
    RenderFrameHost* frame,
    std::unique_ptr<protocol::Audits::InspectorIssue> issue) {
  // We only utilize a central storage on the main frame. Each issue is
  // still associated with the originating |RenderFrameHost| though.
  DevToolsIssueStorage* issue_storage =
      DevToolsIssueStorage::GetOrCreateForCurrentDocument(
          frame->GetMainFrame());

  issue_storage->AddInspectorIssue(frame->GetFrameTreeNodeId(),
                                   std::move(issue));
}

}  // namespace

void ReportBrowserInitiatedIssue(RenderFrameHostImpl* frame,
                                 protocol::Audits::InspectorIssue* issue) {
  FrameTreeNode* ftn = frame->frame_tree_node();
  if (!ftn)
    return;

  AddIssueToIssueStorage(frame, issue->clone());
  DispatchToAgents(ftn, &protocol::AuditsHandler::OnIssueAdded, issue);
}

std::unique_ptr<protocol::Audits::InspectorIssue> GetHeavyAdIssue(
    RenderFrameHostImpl* frame,
    blink::mojom::HeavyAdResolutionStatus resolution,
    blink::mojom::HeavyAdReason reason) {
  auto issue_details = protocol::Audits::InspectorIssueDetails::Create();
  issue_details.SetHeavyAdIssueDetails(
      GetHeavyAdIssueHelper(frame, resolution, reason));
  return protocol::Audits::InspectorIssue::Create()
      .SetCode(protocol::Audits::InspectorIssueCodeEnum::HeavyAdIssue)
      .SetDetails(issue_details.Build())
      .Build();
}

void OnQuicTransportHandshakeFailed(
    RenderFrameHostImpl* frame,
    const GURL& url,
    const base::Optional<net::QuicTransportError>& error) {
  FrameTreeNode* ftn = frame->frame_tree_node();
  if (!ftn)
    return;
  std::string text = base::StringPrintf(
      "Failed to establish a connection to %s", url.spec().c_str());
  if (error) {
    text += ": ";
    text += net::QuicTransportErrorToString(*error);
  }
  text += ".";
  auto entry = protocol::Log::LogEntry::Create()
                   .SetSource(protocol::Log::LogEntry::SourceEnum::Network)
                   .SetLevel(protocol::Log::LogEntry::LevelEnum::Error)
                   .SetText(text)
                   .SetTimestamp(base::Time::Now().ToDoubleT() * 1000.0)
                   .Build();
  DispatchToAgents(ftn, &protocol::LogHandler::EntryAdded, entry.get());
}

void ApplyNetworkContextParamsOverrides(
    BrowserContext* browser_context,
    network::mojom::NetworkContextParams* context_params) {
  for (auto* agent_host : BrowserDevToolsAgentHost::Instances()) {
    for (auto* target_handler :
         protocol::TargetHandler::ForAgentHost(agent_host)) {
      target_handler->ApplyNetworkContextParamsOverrides(browser_context,
                                                         context_params);
    }
  }
}

}  // namespace devtools_instrumentation

}  // namespace content
