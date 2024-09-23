// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/network_service_devtools_observer.h"

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/protocol/audits_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/mojom/http_raw_headers.mojom.h"
#include "services/network/public/mojom/shared_dictionary_error.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace content {

namespace {

template <typename Handler, typename... MethodArgs, typename... Args>
void DispatchToAgents(DevToolsAgentHostImpl* agent_host,
                      void (Handler::*method)(MethodArgs...),
                      Args&&... args) {
  for (auto* h : Handler::ForAgentHost(agent_host))
    (h->*method)(std::forward<Args>(args)...);
}

RenderFrameHostImpl* GetRenderFrameHostImplFrom(
    FrameTreeNodeId frame_tree_node_id) {
  auto* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!ftn) {
    return nullptr;
  }

  RenderFrameHostImpl* rfhi = ftn->current_frame_host();
  return rfhi;
}

}  // namespace

NetworkServiceDevToolsObserver::NetworkServiceDevToolsObserver(
    base::PassKey<NetworkServiceDevToolsObserver> pass_key,
    const std::string& id,
    FrameTreeNodeId frame_tree_node_id)
    : devtools_agent_id_(id), frame_tree_node_id_(frame_tree_node_id) {}

NetworkServiceDevToolsObserver::~NetworkServiceDevToolsObserver() = default;

DevToolsAgentHostImpl* NetworkServiceDevToolsObserver::GetDevToolsAgentHost() {
  if (frame_tree_node_id_) {
    auto* frame_tree_node =
        FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
    if (!frame_tree_node)
      return nullptr;
    return RenderFrameDevToolsAgentHost::GetFor(frame_tree_node);
  }
  auto host = DevToolsAgentHostImpl::GetForId(devtools_agent_id_);
  if (!host)
    return nullptr;
  return host.get();
}

void NetworkServiceDevToolsObserver::OnRawRequest(
    const std::string& devtools_request_id,
    const net::CookieAccessResultList& request_cookie_list,
    std::vector<network::mojom::HttpRawHeaderPairPtr> request_headers,
    base::TimeTicks timestamp,
    network::mojom::ClientSecurityStatePtr security_state,
    network::mojom::OtherPartitionInfoPtr other_partition_info) {
  auto* host = GetDevToolsAgentHost();
  if (!host)
    return;
  DispatchToAgents(host,
                   &protocol::NetworkHandler::OnRequestWillBeSentExtraInfo,
                   devtools_request_id, request_cookie_list, request_headers,
                   timestamp, security_state, other_partition_info);
}

void NetworkServiceDevToolsObserver::OnRawResponse(
    const std::string& devtools_request_id,
    const net::CookieAndLineAccessResultList& response_cookie_list,
    std::vector<network::mojom::HttpRawHeaderPairPtr> response_headers,
    const std::optional<std::string>& response_headers_text,
    network::mojom::IPAddressSpace resource_address_space,
    int32_t http_status_code,
    const std::optional<net::CookiePartitionKey>& cookie_partition_key) {
  auto* host = GetDevToolsAgentHost();
  if (!host)
    return;
  DispatchToAgents(host, &protocol::NetworkHandler::OnResponseReceivedExtraInfo,
                   devtools_request_id, response_cookie_list, response_headers,
                   response_headers_text, resource_address_space,
                   http_status_code, cookie_partition_key);
}

void NetworkServiceDevToolsObserver::OnEarlyHintsResponse(
    const std::string& devtools_request_id,
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers) {
  auto* host = GetDevToolsAgentHost();
  if (!host) {
    return;
  }
  DispatchToAgents(host,
                   &protocol::NetworkHandler::OnResponseReceivedEarlyHints,
                   devtools_request_id, headers);
}

void NetworkServiceDevToolsObserver::OnTrustTokenOperationDone(
    const std::string& devtools_request_id,
    network::mojom::TrustTokenOperationResultPtr result) {
  auto* host = GetDevToolsAgentHost();
  if (!host)
    return;
  DispatchToAgents(host, &protocol::NetworkHandler::OnTrustTokenOperationDone,
                   devtools_request_id, *result);
}

void NetworkServiceDevToolsObserver::OnPrivateNetworkRequest(
    const std::optional<std::string>& devtools_request_id,
    const GURL& url,
    bool is_warning,
    network::mojom::IPAddressSpace resource_address_space,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  if (frame_tree_node_id_.is_null()) {
    return;
  }
  auto* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  if (!ftn)
    return;
  auto cors_error_status =
      protocol::Network::CorsErrorStatus::Create()
          .SetCorsError(
              protocol::Network::CorsErrorEnum::InsecurePrivateNetwork)
          .SetFailedParameter("")
          .Build();
  std::unique_ptr<protocol::Audits::AffectedRequest> affected_request =
      protocol::Audits::AffectedRequest::Create()
          .SetRequestId(devtools_request_id.value_or(""))
          .SetUrl(url.spec())
          .Build();
  auto cors_issue_details =
      protocol::Audits::CorsIssueDetails::Create()
          .SetIsWarning(is_warning)
          .SetResourceIPAddressSpace(
              protocol::NetworkHandler::BuildIpAddressSpace(
                  resource_address_space))
          .SetRequest(std::move(affected_request))
          .SetCorsErrorStatus(std::move(cors_error_status))
          .Build();
  if (auto maybe_protocol_security_state =
          protocol::NetworkHandler::MaybeBuildClientSecurityState(
              client_security_state)) {
    cors_issue_details->SetClientSecurityState(
        std::move(maybe_protocol_security_state));
  }
  auto details = protocol::Audits::InspectorIssueDetails::Create()
                     .SetCorsIssueDetails(std::move(cors_issue_details))
                     .Build();

  auto issue = protocol::Audits::InspectorIssue::Create()
                   .SetCode(protocol::Audits::InspectorIssueCodeEnum::CorsIssue)
                   .SetDetails(std::move(details))
                   .Build();
  devtools_instrumentation::ReportBrowserInitiatedIssue(
      ftn->current_frame_host(), issue.get());
}

void NetworkServiceDevToolsObserver::OnCorsPreflightRequest(
    const base::UnguessableToken& devtools_request_id,
    const net::HttpRequestHeaders& request_headers,
    network::mojom::URLRequestDevToolsInfoPtr request_info,
    const GURL& initiator_url,
    const std::string& initiator_devtools_request_id) {
  auto* host = GetDevToolsAgentHost();
  if (!host)
    return;
  auto timestamp = base::TimeTicks::Now();
  auto id = devtools_request_id.ToString();
  DispatchToAgents(host, &protocol::NetworkHandler::RequestSent, id,
                   /* loader_id=*/"", request_headers, *request_info,
                   protocol::Network::Initiator::TypeEnum::Preflight,
                   initiator_url, initiator_devtools_request_id, timestamp);
}

void NetworkServiceDevToolsObserver::OnCorsPreflightResponse(
    const base::UnguessableToken& devtools_request_id,
    const GURL& url,
    network::mojom::URLResponseHeadDevToolsInfoPtr head) {
  auto* host = GetDevToolsAgentHost();
  if (!host)
    return;
  auto id = devtools_request_id.ToString();
  DispatchToAgents(host, &protocol::NetworkHandler::ResponseReceived, id,
                   /* loader_id=*/"", url,
                   protocol::Network::ResourceTypeEnum::Preflight, *head,
                   protocol::Maybe<std::string>());
}

void NetworkServiceDevToolsObserver::OnCorsPreflightRequestCompleted(
    const base::UnguessableToken& devtools_request_id,
    const network::URLLoaderCompletionStatus& status) {
  auto* host = GetDevToolsAgentHost();
  if (!host)
    return;
  auto id = devtools_request_id.ToString();
  DispatchToAgents(host, &protocol::NetworkHandler::LoadingComplete, id,
                   protocol::Network::ResourceTypeEnum::Preflight, status);
}

void NetworkServiceDevToolsObserver::OnCorsError(
    const std::optional<std::string>& devtools_request_id,
    const std::optional<::url::Origin>& initiator_origin,
    network::mojom::ClientSecurityStatePtr client_security_state,
    const GURL& url,
    const network::CorsErrorStatus& cors_error_status,
    bool is_warning) {
  RenderFrameHostImpl* rfhi = GetRenderFrameHostImplFrom(frame_tree_node_id_);
  if (!rfhi)
    return;

  std::unique_ptr<protocol::Audits::AffectedRequest> affected_request =
      protocol::Audits::AffectedRequest::Create()
          .SetRequestId(devtools_request_id ? *devtools_request_id : "")
          .SetUrl(url.spec())
          .Build();

  auto cors_issue_details =
      protocol::Audits::CorsIssueDetails::Create()
          .SetIsWarning(is_warning)
          .SetRequest(std::move(affected_request))
          .SetCorsErrorStatus(
              protocol::NetworkHandler::BuildCorsErrorStatus(cors_error_status))
          .Build();
  if (initiator_origin) {
    cors_issue_details->SetInitiatorOrigin(initiator_origin->GetURL().spec());
  }
  if (auto maybe_protocol_security_state =
          protocol::NetworkHandler::MaybeBuildClientSecurityState(
              client_security_state)) {
    cors_issue_details->SetClientSecurityState(
        std::move(maybe_protocol_security_state));
  }

  auto details = protocol::Audits::InspectorIssueDetails::Create()
                     .SetCorsIssueDetails(std::move(cors_issue_details))
                     .Build();
  auto issue = protocol::Audits::InspectorIssue::Create()
                   .SetCode(protocol::Audits::InspectorIssueCodeEnum::CorsIssue)
                   .SetDetails(std::move(details))
                   .SetIssueId(cors_error_status.issue_id.ToString())
                   .Build();
  devtools_instrumentation::ReportBrowserInitiatedIssue(rfhi, issue.get());
}

void NetworkServiceDevToolsObserver::OnOrbError(
    const std::optional<std::string>& devtools_request_id,
    const GURL& url) {
  RenderFrameHostImpl* rfhi = GetRenderFrameHostImplFrom(frame_tree_node_id_);
  if (!rfhi) {
    return;
  }

  std::unique_ptr<protocol::Audits::AffectedRequest> affected_request =
      protocol::Audits::AffectedRequest::Create()
          .SetRequestId(devtools_request_id.value_or(""))
          .SetUrl(url.spec())
          .Build();
  auto generic_details =
      protocol::Audits::GenericIssueDetails::Create()
          .SetErrorType(protocol::Audits::GenericIssueErrorTypeEnum::
                            ResponseWasBlockedByORB)
          .SetRequest(std::move(affected_request))
          .Build();
  auto details = protocol::Audits::InspectorIssueDetails::Create()
                     .SetGenericIssueDetails(std::move(generic_details))
                     .Build();
  auto issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(protocol::Audits::InspectorIssueCodeEnum::GenericIssue)
          .SetDetails(std::move(details))
          .Build();
  devtools_instrumentation::ReportBrowserInitiatedIssue(rfhi, issue.get());
}

void NetworkServiceDevToolsObserver::OnSubresourceWebBundleMetadata(
    const std::string& devtools_request_id,
    const std::vector<GURL>& urls) {
  auto* host = GetDevToolsAgentHost();
  if (!host)
    return;
  DispatchToAgents(host,
                   &protocol::NetworkHandler::OnSubresourceWebBundleMetadata,
                   devtools_request_id, urls);
}

void NetworkServiceDevToolsObserver::OnSubresourceWebBundleMetadataError(
    const std::string& devtools_request_id,
    const std::string& error_message) {
  auto* host = GetDevToolsAgentHost();
  if (!host)
    return;
  DispatchToAgents(
      host, &protocol::NetworkHandler::OnSubresourceWebBundleMetadataError,
      devtools_request_id, error_message);
}

void NetworkServiceDevToolsObserver::OnSubresourceWebBundleInnerResponse(
    const std::string& inner_request_devtools_id,
    const GURL& url,
    const std::optional<std::string>& bundle_request_devtools_id) {
  auto* host = GetDevToolsAgentHost();
  if (!host)
    return;
  DispatchToAgents(
      host, &protocol::NetworkHandler::OnSubresourceWebBundleInnerResponse,
      inner_request_devtools_id, url, bundle_request_devtools_id);
}

void NetworkServiceDevToolsObserver::OnSubresourceWebBundleInnerResponseError(
    const std::string& inner_request_devtools_id,
    const GURL& url,
    const std::string& error_message,
    const std::optional<std::string>& bundle_request_devtools_id) {
  auto* host = GetDevToolsAgentHost();
  if (!host)
    return;
  DispatchToAgents(
      host, &protocol::NetworkHandler::OnSubresourceWebBundleInnerResponseError,
      inner_request_devtools_id, url, error_message,
      bundle_request_devtools_id);
}

namespace {

protocol::String BuildSharedDictionaryError(
    network::mojom::SharedDictionaryError write_error) {
  using network::mojom::SharedDictionaryError;
  namespace SharedDictionaryErrorEnum =
      protocol::Audits::SharedDictionaryErrorEnum;
  switch (write_error) {
    case SharedDictionaryError::kUseErrorCrossOriginNoCorsRequest:
      return SharedDictionaryErrorEnum::UseErrorCrossOriginNoCorsRequest;
    case SharedDictionaryError::kUseErrorDictionaryLoadFailure:
      return SharedDictionaryErrorEnum::UseErrorDictionaryLoadFailure;
    case SharedDictionaryError::kUseErrorMatchingDictionaryNotUsed:
      return SharedDictionaryErrorEnum::UseErrorMatchingDictionaryNotUsed;
    case SharedDictionaryError::kUseErrorUnexpectedContentDictionaryHeader:
      return SharedDictionaryErrorEnum::
          UseErrorUnexpectedContentDictionaryHeader;
    case SharedDictionaryError::kWriteErrorAlreadyRegistered:
      NOTREACHED();
    case SharedDictionaryError::kWriteErrorCossOriginNoCorsRequest:
      return SharedDictionaryErrorEnum::WriteErrorCossOriginNoCorsRequest;
    case SharedDictionaryError::kWriteErrorDisallowedBySettings:
      return SharedDictionaryErrorEnum::WriteErrorDisallowedBySettings;
    case SharedDictionaryError::kWriteErrorExpiredResponse:
      return SharedDictionaryErrorEnum::WriteErrorExpiredResponse;
    case SharedDictionaryError::kWriteErrorFeatureDisabled:
      return SharedDictionaryErrorEnum::WriteErrorFeatureDisabled;
    case SharedDictionaryError::kWriteErrorInsufficientResources:
      return SharedDictionaryErrorEnum::WriteErrorInsufficientResources;
    case SharedDictionaryError::kWriteErrorInvalidMatchField:
      return SharedDictionaryErrorEnum::WriteErrorInvalidMatchField;
    case SharedDictionaryError::kWriteErrorInvalidStructuredHeader:
      return SharedDictionaryErrorEnum::WriteErrorInvalidStructuredHeader;
    case SharedDictionaryError::kWriteErrorNavigationRequest:
      return SharedDictionaryErrorEnum::WriteErrorNavigationRequest;
    case SharedDictionaryError::kWriteErrorNoMatchField:
      return SharedDictionaryErrorEnum::WriteErrorNoMatchField;
    case SharedDictionaryError::kWriteErrorNonListMatchDestField:
      return SharedDictionaryErrorEnum::WriteErrorNonListMatchDestField;
    case SharedDictionaryError::kWriteErrorNonSecureContext:
      return SharedDictionaryErrorEnum::WriteErrorNonSecureContext;
    case SharedDictionaryError::kWriteErrorNonStringIdField:
      return SharedDictionaryErrorEnum::WriteErrorNonStringIdField;
    case SharedDictionaryError::kWriteErrorNonStringInMatchDestList:
      return SharedDictionaryErrorEnum::WriteErrorNonStringInMatchDestList;
    case SharedDictionaryError::kWriteErrorNonStringMatchField:
      return SharedDictionaryErrorEnum::WriteErrorNonStringMatchField;
    case SharedDictionaryError::kWriteErrorNonTokenTypeField:
      return SharedDictionaryErrorEnum::WriteErrorNonTokenTypeField;
    case SharedDictionaryError::kWriteErrorRequestAborted:
      return SharedDictionaryErrorEnum::WriteErrorRequestAborted;
    case SharedDictionaryError::kWriteErrorShuttingDown:
      return SharedDictionaryErrorEnum::WriteErrorShuttingDown;
    case SharedDictionaryError::kWriteErrorTooLongIdField:
      return SharedDictionaryErrorEnum::WriteErrorTooLongIdField;
    case SharedDictionaryError::kWriteErrorUnsupportedType:
      return SharedDictionaryErrorEnum::WriteErrorUnsupportedType;
  }
}

}  // namespace

void NetworkServiceDevToolsObserver::OnSharedDictionaryError(
    const std::string& devtool_request_id,
    const GURL& url,
    network::mojom::SharedDictionaryError error) {
  RenderFrameHostImpl* rfhi = GetRenderFrameHostImplFrom(frame_tree_node_id_);
  if (!rfhi) {
    return;
  }
  auto affected_request = protocol::Audits::AffectedRequest::Create()
                              .SetRequestId(devtool_request_id)
                              .SetUrl(url.spec())
                              .Build();
  auto shared_dictionary_issue_details =
      protocol::Audits::SharedDictionaryIssueDetails::Create()
          .SetSharedDictionaryError(BuildSharedDictionaryError(error))
          .SetRequest(std::move(affected_request))
          .Build();
  auto details = protocol::Audits::InspectorIssueDetails::Create()
                     .SetSharedDictionaryIssueDetails(
                         std::move(shared_dictionary_issue_details))
                     .Build();
  auto issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(
              protocol::Audits::InspectorIssueCodeEnum::SharedDictionaryIssue)
          .SetDetails(std::move(details))
          .Build();
  devtools_instrumentation::ReportBrowserInitiatedIssue(rfhi, issue.get());
}

void NetworkServiceDevToolsObserver::Clone(
    mojo::PendingReceiver<network::mojom::DevToolsObserver> observer) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NetworkServiceDevToolsObserver>(
          base::PassKey<NetworkServiceDevToolsObserver>(), devtools_agent_id_,
          frame_tree_node_id_),
      std::move(observer));
}

mojo::PendingRemote<network::mojom::DevToolsObserver>
NetworkServiceDevToolsObserver::MakeSelfOwned(const std::string& id) {
  mojo::PendingRemote<network::mojom::DevToolsObserver> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NetworkServiceDevToolsObserver>(
          base::PassKey<NetworkServiceDevToolsObserver>(), id,
          FrameTreeNodeId()),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

mojo::PendingRemote<network::mojom::DevToolsObserver>
NetworkServiceDevToolsObserver::MakeSelfOwned(FrameTreeNode* frame_tree_node) {
  mojo::PendingRemote<network::mojom::DevToolsObserver> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NetworkServiceDevToolsObserver>(
          base::PassKey<NetworkServiceDevToolsObserver>(), std::string(),
          frame_tree_node->frame_tree_node_id()),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

}  // namespace content
