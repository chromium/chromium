// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/devtools/devtools_instrumentation.h"

#include "content/browser/devtools/protocol/emulation_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/common/navigation_params.mojom.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

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

}  // namespace

void OnResetNavigationRequest(NavigationRequest* navigation_request) {
  // Traverse frame chain all the way to the top and report to all
  // page handlers that the navigation completed.
  for (FrameTreeNode* node = navigation_request->frame_tree_node(); node;
       node = node->parent()) {
    DispatchToAgents(node, &protocol::PageHandler::NavigationReset,
                     navigation_request);
  }
}

void OnNavigationResponseReceived(const NavigationRequest& nav_request,
                                  const network::ResourceResponse& response) {
  FrameTreeNode* ftn = nav_request.frame_tree_node();
  std::string id = nav_request.devtools_navigation_token().ToString();
  std::string frame_id = ftn->devtools_frame_token().ToString();
  GURL url = nav_request.common_params().url;
  DispatchToAgents(ftn, &protocol::NetworkHandler::ResponseReceived, id, id,
                   url, protocol::Network::ResourceTypeEnum::Document,
                   response.head, frame_id);
}

void OnNavigationRequestFailed(
    const NavigationRequest& nav_request,
    const network::URLLoaderCompletionStatus& status) {
  FrameTreeNode* ftn = nav_request.frame_tree_node();
  std::string id = nav_request.devtools_navigation_token().ToString();
  DispatchToAgents(ftn, &protocol::NetworkHandler::LoadingComplete, id,
                   protocol::Network::ResourceTypeEnum::Document, status);
}

void OnSignedExchangeReceived(
    FrameTreeNode* frame_tree_node,
    base::Optional<const base::UnguessableToken> devtools_navigation_token,
    const GURL& outer_request_url,
    const network::ResourceResponseHead& outer_response,
    const base::Optional<SignedExchangeEnvelope>& envelope,
    const scoped_refptr<net::X509Certificate>& certificate,
    const base::Optional<net::SSLInfo>& ssl_info,
    const std::vector<SignedExchangeError>& errors) {
  DispatchToAgents(frame_tree_node,
                   &protocol::NetworkHandler::OnSignedExchangeReceived,
                   devtools_navigation_token, outer_request_url, outer_response,
                   envelope, certificate, ssl_info, errors);
}

void OnSignedExchangeCertificateRequestSent(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const base::UnguessableToken& loader_id,
    const network::ResourceRequest& request,
    const GURL& signed_exchange_url) {
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::RequestSent,
                   request_id.ToString(), loader_id.ToString(), request,
                   protocol::Network::Initiator::TypeEnum::SignedExchange,
                   signed_exchange_url);
}

void OnSignedExchangeCertificateResponseReceived(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const base::UnguessableToken& loader_id,
    const GURL& url,
    const network::ResourceResponseHead& head) {
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

std::vector<std::unique_ptr<NavigationThrottle>> CreateNavigationThrottles(
    NavigationHandleImpl* navigation_handle) {
  std::vector<std::unique_ptr<NavigationThrottle>> result;
  FrameTreeNode* frame_tree_node = navigation_handle->frame_tree_node();

  DevToolsAgentHostImpl* agent_host =
      RenderFrameDevToolsAgentHost::GetFor(frame_tree_node);
  if (agent_host) {
    // Interception might throttle navigations in inspected frames.
    for (auto* network_handler :
         protocol::NetworkHandler::ForAgentHost(agent_host)) {
      std::unique_ptr<NavigationThrottle> throttle =
          network_handler->CreateThrottleForNavigation(navigation_handle);
      if (throttle)
        result.push_back(std::move(throttle));
    }
  }
  if (!frame_tree_node->parent())
    return result;
  agent_host = RenderFrameDevToolsAgentHost::GetFor(frame_tree_node->parent());
  if (agent_host) {
    for (auto* target_handler :
         protocol::TargetHandler::ForAgentHost(agent_host)) {
      std::unique_ptr<NavigationThrottle> throttle =
          target_handler->CreateThrottleForNavigation(navigation_handle);
      if (throttle)
        result.push_back(std::move(throttle));
    }
  }

  return result;
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

bool WillCreateURLLoaderFactory(
    RenderFrameHostImpl* rfh,
    bool is_navigation,
    bool is_download,
    network::mojom::URLLoaderFactoryRequest* target_factory_request) {
  DevToolsAgentHostImpl* agent_host =
      RenderFrameDevToolsAgentHost::GetFor(rfh->frame_tree_node());
  if (!agent_host)
    return false;
  DCHECK(!is_download || is_navigation);
  bool had_interceptors = false;
  const auto& network_handlers =
      protocol::NetworkHandler::ForAgentHost(agent_host);
  for (auto it = network_handlers.rbegin(); it != network_handlers.rend();
       ++it) {
    had_interceptors =
        (*it)->MaybeCreateProxyForInterception(rfh, is_navigation, is_download,
                                               target_factory_request) ||
        had_interceptors;
  }
  return had_interceptors;
}

void OnNavigationRequestWillBeSent(
    const NavigationRequest& navigation_request) {
  DispatchToAgents(navigation_request.frame_tree_node(),
                   &protocol::NetworkHandler::NavigationRequestWillBeSent,
                   navigation_request);
}

}  // namespace devtools_instrumentation

}  // namespace content