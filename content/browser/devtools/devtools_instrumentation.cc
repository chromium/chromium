// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_instrumentation.h"

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/traced_value.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/devtools/browser_devtools_agent_host.h"
#include "content/browser/devtools/dedicated_worker_devtools_agent_host.h"
#include "content/browser/devtools/devtools_issue_storage.h"
#include "content/browser/devtools/devtools_preload_storage.h"
#include "content/browser/devtools/protocol/audits.h"
#include "content/browser/devtools/protocol/audits_handler.h"
#include "content/browser/devtools/protocol/browser_handler.h"
#include "content/browser/devtools/protocol/device_access_handler.h"
#include "content/browser/devtools/protocol/emulation_handler.h"
#include "content/browser/devtools/protocol/fedcm_handler.h"
#include "content/browser/devtools/protocol/fetch_handler.h"
#include "content/browser/devtools/protocol/input_handler.h"
#include "content/browser/devtools/protocol/log_handler.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/protocol/preload_handler.h"
#include "content/browser/devtools/protocol/security_handler.h"
#include "content/browser/devtools/protocol/storage_handler.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/web_contents_devtools_agent_host.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/public/browser/browser_context.h"
#include "devtools_agent_host_impl.h"
#include "devtools_instrumentation.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/http/http_request_headers.h"
#include "net/quic/web_transport_error.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"

namespace content {
namespace devtools_instrumentation {

namespace {

namespace AttributionReportingIssueTypeEnum =
    protocol::Audits::AttributionReportingIssueTypeEnum;

const char kPrivacySandboxExtensionsAPI[] = "PrivacySandboxExtensionsAPI";

template <typename Handler, typename... MethodArgs, typename... Args>
void DispatchToAgents(DevToolsAgentHostImpl* host,
                      void (Handler::*method)(MethodArgs...),
                      Args&&... args) {
  if (!host) {
    return;
  }
  for (auto* h : Handler::ForAgentHost(host)) {
    (h->*method)(std::forward<Args>(args)...);
  }
}

template <typename Handler, typename... MethodArgs, typename... Args>
void DispatchToAgents(FrameTreeNode* frame_tree_node,
                      void (Handler::*method)(MethodArgs...),
                      Args&&... args) {
  DevToolsAgentHostImpl* agent_host =
      RenderFrameDevToolsAgentHost::GetFor(frame_tree_node);
  DispatchToAgents(agent_host, method, std::forward<Args>(args)...);
}

template <typename Handler, typename... MethodArgs, typename... Args>
void DispatchToAgents(FrameTreeNodeId frame_tree_node_id,
                      void (Handler::*method)(MethodArgs...),
                      Args&&... args) {
  FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (ftn) {
    DispatchToAgents(ftn, method, std::forward<Args>(args)...);
  }
}

template <typename Handler, typename... MethodArgs, typename... Args>
void DispatchToAgents(WebContents* web_contents,
                      void (Handler::*method)(MethodArgs...),
                      Args&&... args) {
  auto agent_host = DevToolsAgentHost::GetForTab(web_contents);
  if (agent_host) {
    DispatchToAgents(static_cast<DevToolsAgentHostImpl*>(agent_host.get()),
                     method, std::forward<Args>(args)...);
  }
  if (content::DevToolsAgentHost::HasFor(web_contents)) {
    DispatchToAgents(
        static_cast<DevToolsAgentHostImpl*>(
            content::DevToolsAgentHost::GetOrCreateFor(web_contents).get()),
        method, std::forward<Args>(args)...);
  }
}

std::unique_ptr<protocol::Audits::InspectorIssue> BuildHeavyAdIssue(
    const blink::mojom::HeavyAdIssueDetailsPtr& issue_details) {
  protocol::String status =
      (issue_details->resolution ==
       blink::mojom::HeavyAdResolutionStatus::kHeavyAdBlocked)
          ? protocol::Audits::HeavyAdResolutionStatusEnum::HeavyAdBlocked
          : protocol::Audits::HeavyAdResolutionStatusEnum::HeavyAdWarning;
  protocol::String reason_string;
  switch (issue_details->reason) {
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
  auto heavy_ad_details =
      protocol::Audits::HeavyAdIssueDetails::Create()
          .SetReason(reason_string)
          .SetResolution(status)
          .SetFrame(protocol::Audits::AffectedFrame::Create()
                        .SetFrameId(issue_details->frame->frame_id)
                        .Build())
          .Build();

  auto protocol_issue_details =
      protocol::Audits::InspectorIssueDetails::Create()
          .SetHeavyAdIssueDetails(std::move(heavy_ad_details))
          .Build();
  auto issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(protocol::Audits::InspectorIssueCodeEnum::HeavyAdIssue)
          .SetDetails(std::move(protocol_issue_details))
          .Build();
  return issue;
}

protocol::Audits::AttributionReportingIssueType
BuildAttributionReportingIssueViolationType(
    blink::mojom::AttributionReportingIssueType type) {
  switch (type) {
    case blink::mojom::AttributionReportingIssueType::kPermissionPolicyDisabled:
      return AttributionReportingIssueTypeEnum::PermissionPolicyDisabled;
    case blink::mojom::AttributionReportingIssueType::
        kUntrustworthyReportingOrigin:
      return AttributionReportingIssueTypeEnum::UntrustworthyReportingOrigin;
    case blink::mojom::AttributionReportingIssueType::kInsecureContext:
      return AttributionReportingIssueTypeEnum::InsecureContext;
    case blink::mojom::AttributionReportingIssueType::
        kInvalidRegisterSourceHeader:
      return AttributionReportingIssueTypeEnum::InvalidHeader;
    case blink::mojom::AttributionReportingIssueType::
        kInvalidRegisterTriggerHeader:
      return AttributionReportingIssueTypeEnum::InvalidRegisterTriggerHeader;
    case blink::mojom::AttributionReportingIssueType::kSourceAndTriggerHeaders:
      return AttributionReportingIssueTypeEnum::SourceAndTriggerHeaders;
    case blink::mojom::AttributionReportingIssueType::kSourceIgnored:
      return AttributionReportingIssueTypeEnum::SourceIgnored;
    case blink::mojom::AttributionReportingIssueType::kTriggerIgnored:
      return AttributionReportingIssueTypeEnum::TriggerIgnored;
    case blink::mojom::AttributionReportingIssueType::kOsSourceIgnored:
      return AttributionReportingIssueTypeEnum::OsSourceIgnored;
    case blink::mojom::AttributionReportingIssueType::kOsTriggerIgnored:
      return AttributionReportingIssueTypeEnum::OsTriggerIgnored;
    case blink::mojom::AttributionReportingIssueType::
        kInvalidRegisterOsSourceHeader:
      return AttributionReportingIssueTypeEnum::InvalidRegisterOsSourceHeader;
    case blink::mojom::AttributionReportingIssueType::
        kInvalidRegisterOsTriggerHeader:
      return AttributionReportingIssueTypeEnum::InvalidRegisterOsTriggerHeader;
    case blink::mojom::AttributionReportingIssueType::kWebAndOsHeaders:
      return AttributionReportingIssueTypeEnum::WebAndOsHeaders;
    case blink::mojom::AttributionReportingIssueType::kNoWebOrOsSupport:
      return AttributionReportingIssueTypeEnum::NoWebOrOsSupport;
    case blink::mojom::AttributionReportingIssueType::
        kNavigationRegistrationWithoutTransientUserActivation:
      // This issue is not reported from the browser.
      NOTREACHED();
    case blink::mojom::AttributionReportingIssueType::kInvalidInfoHeader:
      return AttributionReportingIssueTypeEnum::InvalidInfoHeader;
    case blink::mojom::AttributionReportingIssueType::kNoRegisterSourceHeader:
      return AttributionReportingIssueTypeEnum::NoRegisterSourceHeader;
    case blink::mojom::AttributionReportingIssueType::kNoRegisterTriggerHeader:
      return AttributionReportingIssueTypeEnum::NoRegisterTriggerHeader;
    case blink::mojom::AttributionReportingIssueType::kNoRegisterOsSourceHeader:
      return AttributionReportingIssueTypeEnum::NoRegisterOsSourceHeader;
    case blink::mojom::AttributionReportingIssueType::
        kNoRegisterOsTriggerHeader:
      return AttributionReportingIssueTypeEnum::NoRegisterOsTriggerHeader;
    case blink::mojom::AttributionReportingIssueType::
        kNavigationRegistrationUniqueScopeAlreadySet:
      return AttributionReportingIssueTypeEnum::
          NavigationRegistrationUniqueScopeAlreadySet;
  }
}

std::unique_ptr<protocol::Audits::InspectorIssue>
BuildAttributionReportingIssue(
    const blink::mojom::AttributionReportingIssueDetailsPtr& issue_details) {
  protocol::String violation_type = BuildAttributionReportingIssueViolationType(
      issue_details->violation_type);

  CHECK(issue_details->request->url.has_value());
  auto request = protocol::Audits::AffectedRequest::Create()
                     .SetRequestId(issue_details->request->request_id)
                     .SetUrl(issue_details->request->url.value())
                     .Build();
  auto attribution_reporting_issue_details =
      protocol::Audits::AttributionReportingIssueDetails::Create()
          .SetViolationType(violation_type)
          .SetRequest(std::move(request))
          .Build();
  if (issue_details->invalid_parameter.has_value()) {
    attribution_reporting_issue_details->SetInvalidParameter(
        issue_details->invalid_parameter.value());
  }

  auto protocol_issue_details =
      protocol::Audits::InspectorIssueDetails::Create()
          .SetAttributionReportingIssueDetails(
              std::move(attribution_reporting_issue_details))
          .Build();

  auto issue = protocol::Audits::InspectorIssue::Create()
                   .SetCode(protocol::Audits::InspectorIssueCodeEnum::
                                AttributionReportingIssue)
                   .SetDetails(std::move(protocol_issue_details))
                   .Build();
  return issue;
}

protocol::Audits::FederatedAuthRequestIssueReason
FederatedAuthRequestResultToProtocol(
    blink::mojom::FederatedAuthRequestResult result) {
  using blink::mojom::FederatedAuthRequestResult;
  namespace FederatedAuthRequestIssueReasonEnum =
      protocol::Audits::FederatedAuthRequestIssueReasonEnum;
  switch (result) {
    case FederatedAuthRequestResult::kShouldEmbargo: {
      return FederatedAuthRequestIssueReasonEnum::ShouldEmbargo;
    }
    case FederatedAuthRequestResult::kDisabledInSettings: {
      return FederatedAuthRequestIssueReasonEnum::DisabledInSettings;
    }
    case FederatedAuthRequestResult::kDisabledInFlags: {
      return FederatedAuthRequestIssueReasonEnum::DisabledInFlags;
    }
    case FederatedAuthRequestResult::kIdpNotPotentiallyTrustworthy: {
      return FederatedAuthRequestIssueReasonEnum::IdpNotPotentiallyTrustworthy;
    }
    case FederatedAuthRequestResult::kTooManyRequests: {
      return FederatedAuthRequestIssueReasonEnum::TooManyRequests;
    }
    case FederatedAuthRequestResult::kWellKnownHttpNotFound: {
      return FederatedAuthRequestIssueReasonEnum::WellKnownHttpNotFound;
    }
    case FederatedAuthRequestResult::kWellKnownNoResponse: {
      return FederatedAuthRequestIssueReasonEnum::WellKnownNoResponse;
    }
    case FederatedAuthRequestResult::kWellKnownInvalidResponse: {
      return FederatedAuthRequestIssueReasonEnum::WellKnownInvalidResponse;
    }
    case FederatedAuthRequestResult::kWellKnownListEmpty: {
      return FederatedAuthRequestIssueReasonEnum::WellKnownListEmpty;
    }
    case FederatedAuthRequestResult::kWellKnownInvalidContentType: {
      return FederatedAuthRequestIssueReasonEnum::WellKnownInvalidContentType;
    }
    case FederatedAuthRequestResult::kConfigNotInWellKnown: {
      return FederatedAuthRequestIssueReasonEnum::ConfigNotInWellKnown;
    }
    case FederatedAuthRequestResult::kWellKnownTooBig: {
      return FederatedAuthRequestIssueReasonEnum::WellKnownTooBig;
    }
    case FederatedAuthRequestResult::kConfigHttpNotFound: {
      return FederatedAuthRequestIssueReasonEnum::ConfigHttpNotFound;
    }
    case FederatedAuthRequestResult::kConfigNoResponse: {
      return FederatedAuthRequestIssueReasonEnum::ConfigNoResponse;
    }
    case FederatedAuthRequestResult::kConfigInvalidResponse: {
      return FederatedAuthRequestIssueReasonEnum::ConfigInvalidResponse;
    }
    case FederatedAuthRequestResult::kConfigInvalidContentType: {
      return FederatedAuthRequestIssueReasonEnum::ConfigInvalidContentType;
    }
    case FederatedAuthRequestResult::kClientMetadataHttpNotFound: {
      return FederatedAuthRequestIssueReasonEnum::ClientMetadataHttpNotFound;
    }
    case FederatedAuthRequestResult::kClientMetadataNoResponse: {
      return FederatedAuthRequestIssueReasonEnum::ClientMetadataNoResponse;
    }
    case FederatedAuthRequestResult::kClientMetadataInvalidResponse: {
      return FederatedAuthRequestIssueReasonEnum::ClientMetadataInvalidResponse;
    }
    case FederatedAuthRequestResult::kClientMetadataInvalidContentType: {
      return FederatedAuthRequestIssueReasonEnum::
          ClientMetadataInvalidContentType;
    }
    case FederatedAuthRequestResult::kAccountsHttpNotFound: {
      return FederatedAuthRequestIssueReasonEnum::AccountsHttpNotFound;
    }
    case FederatedAuthRequestResult::kAccountsNoResponse: {
      return FederatedAuthRequestIssueReasonEnum::AccountsNoResponse;
    }
    case FederatedAuthRequestResult::kAccountsInvalidResponse: {
      return FederatedAuthRequestIssueReasonEnum::AccountsInvalidResponse;
    }
    case FederatedAuthRequestResult::kAccountsListEmpty: {
      return FederatedAuthRequestIssueReasonEnum::AccountsListEmpty;
    }
    case FederatedAuthRequestResult::kAccountsInvalidContentType: {
      return FederatedAuthRequestIssueReasonEnum::AccountsInvalidContentType;
    }
    case FederatedAuthRequestResult::kIdTokenHttpNotFound: {
      return FederatedAuthRequestIssueReasonEnum::IdTokenHttpNotFound;
    }
    case FederatedAuthRequestResult::kIdTokenNoResponse: {
      return FederatedAuthRequestIssueReasonEnum::IdTokenNoResponse;
    }
    case FederatedAuthRequestResult::kIdTokenInvalidResponse: {
      return FederatedAuthRequestIssueReasonEnum::IdTokenInvalidResponse;
    }
    case FederatedAuthRequestResult::kIdTokenIdpErrorResponse: {
      return FederatedAuthRequestIssueReasonEnum::IdTokenIdpErrorResponse;
    }
    case FederatedAuthRequestResult::kIdTokenCrossSiteIdpErrorResponse: {
      return FederatedAuthRequestIssueReasonEnum::
          IdTokenCrossSiteIdpErrorResponse;
    }
    case FederatedAuthRequestResult::kIdTokenInvalidContentType: {
      return FederatedAuthRequestIssueReasonEnum::IdTokenInvalidContentType;
    }
    case FederatedAuthRequestResult::kCanceled: {
      return FederatedAuthRequestIssueReasonEnum::Canceled;
    }
    case FederatedAuthRequestResult::kRpPageNotVisible:
      return FederatedAuthRequestIssueReasonEnum::RpPageNotVisible;
    case FederatedAuthRequestResult::kError: {
      return FederatedAuthRequestIssueReasonEnum::ErrorIdToken;
    }
    case FederatedAuthRequestResult::kSilentMediationFailure: {
      return FederatedAuthRequestIssueReasonEnum::SilentMediationFailure;
    }
    case FederatedAuthRequestResult::kThirdPartyCookiesBlocked: {
      return FederatedAuthRequestIssueReasonEnum::ThirdPartyCookiesBlocked;
    }
    case FederatedAuthRequestResult::kNotSignedInWithIdp: {
      return FederatedAuthRequestIssueReasonEnum::NotSignedInWithIdp;
    }
    case FederatedAuthRequestResult::kMissingTransientUserActivation: {
      return FederatedAuthRequestIssueReasonEnum::
          MissingTransientUserActivation;
    }
    case FederatedAuthRequestResult::kReplacedByActiveMode: {
      return FederatedAuthRequestIssueReasonEnum::ReplacedByActiveMode;
    }
    case FederatedAuthRequestResult::kInvalidFieldsSpecified: {
      return FederatedAuthRequestIssueReasonEnum::InvalidFieldsSpecified;
    }
    case FederatedAuthRequestResult::kRelyingPartyOriginIsOpaque: {
      return FederatedAuthRequestIssueReasonEnum::RelyingPartyOriginIsOpaque;
    }
    case FederatedAuthRequestResult::kTypeNotMatching: {
      return FederatedAuthRequestIssueReasonEnum::TypeNotMatching;
    }
    case FederatedAuthRequestResult::kSuccess: {
      NOTREACHED();
    }
  }
}

std::unique_ptr<protocol::Audits::InspectorIssue>
BuildFederatedAuthRequestIssue(
    const blink::mojom::FederatedAuthRequestIssueDetailsPtr& issue_details) {
  auto federated_auth_request_details =
      protocol::Audits::FederatedAuthRequestIssueDetails::Create()
          .SetFederatedAuthRequestIssueReason(
              FederatedAuthRequestResultToProtocol(issue_details->status))
          .Build();

  auto protocol_issue_details =
      protocol::Audits::InspectorIssueDetails::Create()
          .SetFederatedAuthRequestIssueDetails(
              std::move(federated_auth_request_details))
          .Build();

  auto issue = protocol::Audits::InspectorIssue::Create()
                   .SetCode(protocol::Audits::InspectorIssueCodeEnum::
                                FederatedAuthRequestIssue)
                   .SetDetails(std::move(protocol_issue_details))
                   .Build();
  return issue;
}

protocol::Audits::FederatedAuthUserInfoRequestIssueReason
FederatedAuthUserInfoRequestResultToProtocol(
    blink::mojom::FederatedAuthUserInfoRequestResult result) {
  using blink::mojom::FederatedAuthUserInfoRequestResult;
  namespace FederatedAuthUserInfoRequestIssueReasonEnum =
      protocol::Audits::FederatedAuthUserInfoRequestIssueReasonEnum;
  switch (result) {
    case FederatedAuthUserInfoRequestResult::kNotSameOrigin: {
      return FederatedAuthUserInfoRequestIssueReasonEnum::NotSameOrigin;
    }
    case FederatedAuthUserInfoRequestResult::kNotIframe: {
      return FederatedAuthUserInfoRequestIssueReasonEnum::NotIframe;
    }
    case FederatedAuthUserInfoRequestResult::kNotPotentiallyTrustworthy: {
      return FederatedAuthUserInfoRequestIssueReasonEnum::
          NotPotentiallyTrustworthy;
    }
    case FederatedAuthUserInfoRequestResult::kNoApiPermission: {
      return FederatedAuthUserInfoRequestIssueReasonEnum::NoApiPermission;
    }
    case FederatedAuthUserInfoRequestResult::kNotSignedInWithIdp: {
      return FederatedAuthUserInfoRequestIssueReasonEnum::NotSignedInWithIdp;
    }
    case FederatedAuthUserInfoRequestResult::kNoAccountSharingPermission: {
      return FederatedAuthUserInfoRequestIssueReasonEnum::
          NoAccountSharingPermission;
    }
    case FederatedAuthUserInfoRequestResult::kInvalidConfigOrWellKnown: {
      return FederatedAuthUserInfoRequestIssueReasonEnum::
          InvalidConfigOrWellKnown;
    }
    case FederatedAuthUserInfoRequestResult::kInvalidAccountsResponse: {
      return FederatedAuthUserInfoRequestIssueReasonEnum::
          InvalidAccountsResponse;
    }
    case FederatedAuthUserInfoRequestResult::
        kNoReturningUserFromFetchedAccounts: {
      return FederatedAuthUserInfoRequestIssueReasonEnum::
          NoReturningUserFromFetchedAccounts;
    }
    case FederatedAuthUserInfoRequestResult::kSuccess:
    case FederatedAuthUserInfoRequestResult::kUnhandledRequest: {
      NOTREACHED();
    }
  }
}

std::unique_ptr<protocol::Audits::InspectorIssue>
BuildFederatedAuthUserInfoRequestIssue(
    const blink::mojom::FederatedAuthUserInfoRequestIssueDetailsPtr&
        issue_details) {
  auto federated_auth_user_info_request_details =
      protocol::Audits::FederatedAuthUserInfoRequestIssueDetails::Create()
          .SetFederatedAuthUserInfoRequestIssueReason(
              FederatedAuthUserInfoRequestResultToProtocol(
                  issue_details->status))
          .Build();

  auto protocol_issue_details =
      protocol::Audits::InspectorIssueDetails::Create()
          .SetFederatedAuthUserInfoRequestIssueDetails(
              std::move(federated_auth_user_info_request_details))
          .Build();

  auto issue = protocol::Audits::InspectorIssue::Create()
                   .SetCode(protocol::Audits::InspectorIssueCodeEnum::
                                FederatedAuthUserInfoRequestIssue)
                   .SetDetails(std::move(protocol_issue_details))
                   .Build();
  return issue;
}

const char* DeprecationIssueTypeToProtocol(
    blink::mojom::DeprecationIssueType error_type) {
  switch (error_type) {
    case blink::mojom::DeprecationIssueType::kPrivacySandboxExtensionsAPI:
      return kPrivacySandboxExtensionsAPI;
  }
}

std::unique_ptr<protocol::Audits::InspectorIssue> BuildDeprecationIssue(
    const blink::mojom::DeprecationIssueDetailsPtr& issue_details) {
  std::unique_ptr<protocol::Audits::SourceCodeLocation> source_code_location =
      protocol::Audits::SourceCodeLocation::Create()
          .SetUrl(issue_details->affected_location->url.value())
          .SetLineNumber(issue_details->affected_location->line)
          .SetColumnNumber(issue_details->affected_location->column)
          .Build();

  if (issue_details->affected_location->script_id.has_value()) {
    source_code_location->SetScriptId(
        issue_details->affected_location->script_id.value());
  }

  auto deprecation_issue_details =
      protocol::Audits::DeprecationIssueDetails::Create()
          .SetSourceCodeLocation(std::move(source_code_location))
          .SetType(DeprecationIssueTypeToProtocol(issue_details->type))
          .Build();

  auto protocol_issue_details =
      protocol::Audits::InspectorIssueDetails::Create()
          .SetDeprecationIssueDetails(std::move(deprecation_issue_details))
          .Build();

  auto deprecation_issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(protocol::Audits::InspectorIssueCodeEnum::DeprecationIssue)
          .SetDetails(std::move(protocol_issue_details))
          .Build();

  return deprecation_issue;
}

std::unique_ptr<protocol::Audits::InspectorIssue> BuildBounceTrackingIssue(
    const blink::mojom::BounceTrackingIssueDetailsPtr& issue_details) {
  auto bounce_tracking_issue_details =
      protocol::Audits::BounceTrackingIssueDetails::Create()
          .SetTrackingSites(std::make_unique<protocol::Array<protocol::String>>(
              issue_details->tracking_sites))
          .Build();

  auto protocol_issue_details =
      protocol::Audits::InspectorIssueDetails::Create()
          .SetBounceTrackingIssueDetails(
              std::move(bounce_tracking_issue_details))
          .Build();

  auto issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(
              protocol::Audits::InspectorIssueCodeEnum::BounceTrackingIssue)
          .SetDetails(std::move(protocol_issue_details))
          .Build();

  return issue;
}

void UpdateChildFrameTrees(FrameTreeNode* ftn, bool update_target_info) {
  if (auto* agent_host = WebContentsDevToolsAgentHost::GetFor(
          WebContentsImpl::FromFrameTreeNode(ftn))) {
    agent_host->UpdateChildFrameTrees(update_target_info);
  }
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

void OnNavigationResponseReceived(const NavigationRequest& nav_request,
                                  const network::mojom::URLResponseHead& head) {
  // This response is artificial (see CachedNavigationURLLoader), so we don't
  // want to report it.
  if (nav_request.IsPageActivation()) {
    return;
  }

  FrameTreeNode* ftn = nav_request.frame_tree_node();
  std::string id = nav_request.devtools_navigation_token().ToString();
  std::string frame_id =
      ftn->current_frame_host()->devtools_frame_token().ToString();
  GURL url = nav_request.common_params().url;

  network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
      network::ExtractDevToolsInfo(head);
  DispatchToAgents(ftn, &protocol::NetworkHandler::ResponseReceived, id, id,
                   url, protocol::Network::ResourceTypeEnum::Document,
                   *head_info, frame_id);
}

void OnFetchKeepAliveRequestWillBeSent(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const network::ResourceRequest& request,
    std::optional<std::pair<const GURL&,
                            const network::mojom::URLResponseHeadDevToolsInfo&>>
        redirect_info) {
  CHECK(frame_tree_node);

  auto timestamp = base::TimeTicks::Now();
  std::string frame_token =
      frame_tree_node->current_frame_host()->devtools_frame_token().ToString();
  GURL initiator_url;
  if (request.request_initiator.has_value()) {
    initiator_url = request.request_initiator->GetURL();
  }
  DispatchToAgents(frame_tree_node,
                   &protocol::NetworkHandler::FetchKeepAliveRequestWillBeSent,
                   request_id, request, initiator_url, frame_token, timestamp,
                   redirect_info);
}

void OnFetchKeepAliveResponseReceived(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const GURL& url,
    const network::mojom::URLResponseHead& head) {
  CHECK(frame_tree_node);

  std::string frame_token =
      frame_tree_node->current_frame_host()->devtools_frame_token().ToString();
  network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
      network::ExtractDevToolsInfo(head);
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::ResponseReceived,
                   request_id, request_id, url,
                   protocol::Network::ResourceTypeEnum::Fetch, *head_info,
                   frame_token);
}

void OnFetchKeepAliveRequestComplete(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status) {
  CHECK(frame_tree_node);

  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::LoadingComplete,
                   request_id, protocol::Network::ResourceTypeEnum::Fetch,
                   status);
}

void BackForwardCacheNotUsed(
    const NavigationRequest* nav_request,
    const BackForwardCacheCanStoreDocumentResult* result,
    const BackForwardCacheCanStoreTreeResult* tree_result) {
  DCHECK(nav_request);
  FrameTreeNode* ftn = nav_request->frame_tree_node();
  DispatchToAgents(ftn, &protocol::PageHandler::BackForwardCacheNotUsed,
                   nav_request, result, tree_result);
}

void WillSwapFrameTreeNode(FrameTreeNode& old_node, FrameTreeNode& new_node) {
  auto* host = static_cast<RenderFrameDevToolsAgentHost*>(
      RenderFrameDevToolsAgentHost::GetFor(&old_node));
  if (!host || host->HasSessionsWithoutTabTargetSupport()) {
    return;
  }
  // The new node may have a previous host associated, disconnect it first.
  scoped_refptr<RenderFrameDevToolsAgentHost> previous_host =
      static_cast<RenderFrameDevToolsAgentHost*>(
          RenderFrameDevToolsAgentHost::GetFor(&new_node));
  // Disconnect old host entirely, so it detaches from renderer and does not
  // cause problem if renderer comes back from the BFCache.
  previous_host->DisconnectWebContents();
  host->SetFrameTreeNode(&new_node);
}

void OnFrameTreeNodeDestroyed(FrameTreeNode& frame_tree_node) {
  // If the child frame is an OOPIF, we emit Page.frameDetached event which
  // otherwise might be lost because the OOPIF target is being destroyed.
  content::RenderFrameHostImpl* parent = frame_tree_node.parent();
  if (!parent) {
    return;
  }
  if (RenderFrameDevToolsAgentHost::GetFor(&frame_tree_node) !=
      RenderFrameDevToolsAgentHost::GetFor(parent)) {
    DispatchToAgents(
        RenderFrameDevToolsAgentHost::GetFor(parent),
        &protocol::PageHandler::OnFrameDetached,
        frame_tree_node.current_frame_host()->devtools_frame_token());
  }
}

bool IsPrerenderAllowed(FrameTree& frame_tree) {
  FrameTreeNode* ftn = frame_tree.root();

  auto* render_frame_agent_host = static_cast<RenderFrameDevToolsAgentHost*>(
      RenderFrameDevToolsAgentHost::GetFor(ftn));
  if (render_frame_agent_host &&
      render_frame_agent_host->HasSessionsWithoutTabTargetSupport()) {
    return false;
  }

  bool is_allowed = true;
  DispatchToAgents(ftn, &protocol::PageHandler::IsPrerenderingAllowed,
                   is_allowed);
  return is_allowed;
}

void WillInitiatePrerender(FrameTree& frame_tree) {
  DCHECK(frame_tree.is_prerendering());
  auto* wc = WebContentsImpl::FromFrameTreeNode(frame_tree.root());
  if (auto* host = WebContentsDevToolsAgentHost::GetFor(wc)) {
    host->WillInitiatePrerender(frame_tree.root());
  }
}

void DidActivatePrerender(const NavigationRequest& nav_request,
                          const std::optional<base::UnguessableToken>&
                              initiator_devtools_navigation_token) {
  FrameTreeNode* ftn = nav_request.frame_tree_node();
  UpdateChildFrameTrees(ftn, /* update_target_info= */ true);
}

void DidUpdatePolicyContainerHost(FrameTreeNode* ftn) {
  if (!ftn) {
    return;
  }

  DispatchToAgents(ftn,
                   &protocol::NetworkHandler::OnPolicyContainerHostUpdated);
}

void DidUpdatePrefetchStatus(
    FrameTreeNode* ftn,
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const GURL& prefetch_url,
    PreloadingTriggeringOutcome status,
    PrefetchStatus prefetch_status,
    const std::string& request_id) {
  if (!ftn) {
    return;
  }

  // We update DevToolsPreloadStorage, even if there are no active DevTools
  // sessions, to persist the latest status update.
  DevToolsPreloadStorage::GetOrCreateForCurrentDocument(
      ftn->current_frame_host())
      ->UpdatePrefetchStatus(prefetch_url, status, prefetch_status, request_id);

  std::string initiating_frame_id =
      ftn->current_frame_host()->devtools_frame_token().ToString();
  DispatchToAgents(ftn, &protocol::PreloadHandler::DidUpdatePrefetchStatus,
                   initiator_devtools_navigation_token, initiating_frame_id,
                   prefetch_url, status, prefetch_status, request_id);
}

void DidUpdatePrerenderStatus(
    FrameTreeNodeId initiator_frame_tree_node_id,
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const GURL& prerender_url,
    std::optional<blink::mojom::SpeculationTargetHint> target_hint,
    PreloadingTriggeringOutcome status,
    std::optional<PrerenderFinalStatus> prerender_status,
    std::optional<std::string> disallowed_mojo_interface,
    const std::vector<PrerenderMismatchedHeaders>* mismatched_headers) {
  auto* ftn = FrameTreeNode::GloballyFindByID(initiator_frame_tree_node_id);
  // ftn will be null if this is browser-initiated, which has no initiator.
  if (!ftn) {
    return;
  }

  // We update DevToolsPreloadStorage, even if there are no active DevTools
  // sessions, to persist the latest status update.
  DevToolsPreloadStorage::GetOrCreateForCurrentDocument(
      ftn->current_frame_host())
      ->UpdatePrerenderStatus(prerender_url, target_hint, status,
                              prerender_status, disallowed_mojo_interface,
                              mismatched_headers);

  DispatchToAgents(ftn, &protocol::PreloadHandler::DidUpdatePrerenderStatus,
                   initiator_devtools_navigation_token, prerender_url,
                   target_hint, status, prerender_status,
                   disallowed_mojo_interface, mismatched_headers);
}

void DidUpdateSpeculationCandidates(
    RenderFrameHost& rfh,
    const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  if (auto* storage = DevToolsPreloadStorage::GetForCurrentDocument(&rfh)) {
    storage->SpeculationCandidatesUpdated(candidates);
  }
}

namespace {

protocol::String BuildBlockedByResponseReason(
    network::mojom::BlockedByResponseReason reason) {
  // TODO(crbug.com/336752983):
  // Add specific error messages when a subresource load was blocked due to
  // Document-Isolation-Policy (Dip).
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
    case network::mojom::BlockedByResponseReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByDip:
    case network::mojom::BlockedByResponseReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByCoep;
    case network::mojom::BlockedByResponseReason::kCorpNotSameSite:
      return protocol::Audits::BlockedByResponseReasonEnum::CorpNotSameSite;
  }
}

void ReportBlockedByResponseIssue(
    const GURL& url,
    std::string& requestId,
    FrameTreeNode* ftn,
    RenderFrameHostImpl* parent_frame,
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(status.blocked_by_response_reason);

  auto issueDetails = protocol::Audits::InspectorIssueDetails::Create();
  auto request = protocol::Audits::AffectedRequest::Create()
                     .SetRequestId(requestId)
                     .SetUrl(url.spec())
                     .Build();
  auto blockedByResponseDetails =
      protocol::Audits::BlockedByResponseIssueDetails::Create()
          .SetRequest(std::move(request))
          .SetReason(
              BuildBlockedByResponseReason(*status.blocked_by_response_reason))
          .Build();

  blockedByResponseDetails->SetBlockedFrame(
      protocol::Audits::AffectedFrame::Create()
          .SetFrameId(
              ftn->current_frame_host()->devtools_frame_token().ToString())
          .Build());
  if (parent_frame) {
    blockedByResponseDetails->SetParentFrame(
        protocol::Audits::AffectedFrame::Create()
            .SetFrameId(parent_frame->devtools_frame_token().ToString())
            .Build());
  }

  issueDetails.SetBlockedByResponseIssueDetails(
      std::move(blockedByResponseDetails));

  auto inspector_issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(
              protocol::Audits::InspectorIssueCodeEnum::BlockedByResponseIssue)
          .SetDetails(issueDetails.Build())
          .Build();

  ReportBrowserInitiatedIssue(ftn->current_frame_host(), inspector_issue.get());
}

}  // namespace

void OnNavigationRequestFailed(
    const NavigationRequest& nav_request,
    const network::URLLoaderCompletionStatus& status) {
  FrameTreeNode* ftn = nav_request.frame_tree_node();
  std::string id = nav_request.devtools_navigation_token().ToString();

  if (status.blocked_by_response_reason) {
    ReportBlockedByResponseIssue(
        const_cast<NavigationRequest&>(nav_request).GetURL(), id, ftn,
        ftn->parent(), status);
  }

  // If a BFCache navigation fails, it will be restarted as a regular
  // navigation, so we don't want to report this failure.
  if (nav_request.IsServedFromBackForwardCache()) {
    return;
  }

  // Activation of a prerender page is synchronous with its own activation flow
  // (crrev.com/c/2992411); if the prerender is cancelled (e.g. speculation rule
  // removed), the flow will fallback to a normal navigation, which is no longer
  // considered as a page activation.
  DCHECK(!nav_request.IsPageActivation());

  DispatchToAgents(ftn, &protocol::NetworkHandler::LoadingComplete, id,
                   protocol::Network::ResourceTypeEnum::Document, status);
}

bool ShouldBypassCSP(const NavigationRequest& nav_request) {
  DevToolsAgentHostImpl* agent_host =
      RenderFrameDevToolsAgentHost::GetFor(nav_request.frame_tree_node());
  if (!agent_host) {
    return false;
  }

  for (auto* page : protocol::PageHandler::ForAgentHost(agent_host)) {
    if (page->ShouldBypassCSP()) {
      return true;
    }
  }
  return false;
}

bool ShouldBypassCertificateErrors(DevToolsAgentHost* agent_host) {
  if (!agent_host) {
    return false;
  }

  DevToolsAgentHostImpl* host_impl =
      static_cast<DevToolsAgentHostImpl*>(agent_host);
  for (auto* security_handler :
       protocol::SecurityHandler::ForAgentHost(host_impl)) {
    if (security_handler->IsIgnoreCertificateErrorsSet()) {
      return true;
    }
  }
  return false;
}

bool ShouldBypassCertificateErrors() {
  for (auto* browser_agent_host : BrowserDevToolsAgentHost::Instances()) {
    if (ShouldBypassCertificateErrors(browser_agent_host)) {
      return true;
    }
  }
  return false;
}

void ApplyNetworkOverridesForDownload(
    RenderFrameHostImpl* rfh,
    download::DownloadUrlParameters* parameters) {
  FrameTreeNode* ftn =
      FrameTreeNode::GloballyFindByID(rfh->GetFrameTreeNodeId());
  if (ftn) {
    DispatchToAgents(
        ftn, &protocol::EmulationHandler::ApplyNetworkOverridesForDownload,
        parameters);
  }
}

void WillBeginDownload(download::DownloadCreateInfo* info,
                       download::DownloadItem* item) {
  if (!item) {
    return;
  }
  auto* rfh = static_cast<RenderFrameHostImpl*>(
      RenderFrameHost::FromID(info->render_process_id, info->render_frame_id));
  FrameTreeNode* ftn =
      rfh ? FrameTreeNode::GloballyFindByID(rfh->GetFrameTreeNodeId())
          : nullptr;
  if (!ftn) {
    return;
  }
  DispatchToAgents(ftn, &protocol::BrowserHandler::DownloadWillBegin, ftn,
                   item);
  DispatchToAgents(ftn, &protocol::PageHandler::DownloadWillBegin, ftn, item);

  for (auto* agent_host : BrowserDevToolsAgentHost::Instances()) {
    for (auto* browser_handler :
         protocol::BrowserHandler::ForAgentHost(agent_host)) {
      browser_handler->DownloadWillBegin(ftn, item);
    }
  }
}

void OnSignedExchangeReceived(
    FrameTreeNode* frame_tree_node,
    std::optional<const base::UnguessableToken> devtools_navigation_token,
    const GURL& outer_request_url,
    const network::mojom::URLResponseHead& outer_response,
    const std::optional<SignedExchangeEnvelope>& envelope,
    const scoped_refptr<net::X509Certificate>& certificate,
    const std::optional<net::SSLInfo>& ssl_info,
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
  network::mojom::URLRequestDevToolsInfoPtr request_info =
      network::ExtractDevToolsInfo(request);
  DispatchToAgents(
      frame_tree_node, &protocol::NetworkHandler::RequestSent,
      request_id.ToString(), loader_id.ToString(), request.headers,
      *request_info, protocol::Network::Initiator::TypeEnum::SignedExchange,
      signed_exchange_url, /*initiator_devtools_request_id=*/"", timestamp);

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
  network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
      network::ExtractDevToolsInfo(head);
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::ResponseReceived,
                   request_id.ToString(), loader_id.ToString(), url,
                   protocol::Network::ResourceTypeEnum::Other, *head_info,
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

void ThrottleForServiceWorkerAgentHost(
    ServiceWorkerDevToolsAgentHost* agent_host,
    DevToolsAgentHostImpl* requesting_agent_host,
    scoped_refptr<DevToolsThrottleHandle> throttle_handle) {
  for (auto* target_handler :
       protocol::TargetHandler::ForAgentHost(requesting_agent_host)) {
    target_handler->AddWorkerThrottle(agent_host, throttle_handle);
  }
}

std::vector<std::unique_ptr<NavigationThrottle>> CreateNavigationThrottles(
    NavigationHandle* navigation_handle) {
  FrameTreeNode* frame_tree_node =
      NavigationRequest::From(navigation_handle)->frame_tree_node();
  FrameTreeNode* parent = FrameTreeNode::From(frame_tree_node->parent());

  std::vector<std::unique_ptr<NavigationThrottle>> result;

  if (!parent) {
    FrameTreeNode* outer_delegate_node =
        frame_tree_node->render_manager()->GetOuterDelegateNode();
    if (outer_delegate_node && frame_tree_node->IsFencedFrameRoot()) {
      parent = outer_delegate_node->parent()->frame_tree_node();
    } else if (frame_tree_node->GetFrameType() ==
                   FrameType::kPrerenderMainFrame &&
               !frame_tree_node->current_frame_host()
                    ->has_committed_any_navigation()) {
      if (auto* agent_host = WebContentsDevToolsAgentHost::GetFor(
              WebContentsImpl::FromFrameTreeNode(frame_tree_node))) {
        // For prerender, perform auto-attach to tab target at the point of
        // initial navigation.
        agent_host->auto_attacher()->AppendNavigationThrottles(
            navigation_handle, &result);
        return result;
      }
    }
  }

  if (parent) {
    if (auto* agent_host = RenderFrameDevToolsAgentHost::GetFor(parent)) {
      agent_host->auto_attacher()->AppendNavigationThrottles(navigation_handle,
                                                             &result);
    }
  } else {
    for (DevToolsAgentHostImpl* host : BrowserDevToolsAgentHost::Instances()) {
      host->auto_attacher()->AppendNavigationThrottles(navigation_handle,
                                                       &result);
    }
  }

  return result;
}

void ThrottleServiceWorkerMainScriptFetch(
    ServiceWorkerContextWrapper* wrapper,
    int64_t version_id,
    const GlobalRenderFrameHostId& requesting_frame_id,
    scoped_refptr<DevToolsThrottleHandle> throttle_handle) {
  ServiceWorkerDevToolsAgentHost* agent_host =
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForNewInstallingWorker(wrapper, version_id);
  DCHECK(agent_host);

  // TODO(crbug.com/40276949): We should probably also add the
  // possibility for Browser wide agents to throttle the request.

  // If we have a requesting_frame_id, we should have a frame and a frame tree
  // node. However since the lifetime of these objects can be complex, we check
  // at each step that we indeed can go reach all the way to the FrameTreeNode.
  if (!requesting_frame_id) {
    return;
  }

  RenderFrameHostImpl* requesting_frame =
      RenderFrameHostImpl::FromID(requesting_frame_id);
  if (!requesting_frame) {
    return;
  }

  FrameTreeNode* ftn = requesting_frame->frame_tree_node();
  DCHECK(ftn);

  DevToolsAgentHostImpl* requesting_agent_host =
      RenderFrameDevToolsAgentHost::GetFor(ftn);
  if (!requesting_agent_host) {
    return;
  }

  ThrottleForServiceWorkerAgentHost(agent_host, requesting_agent_host,
                                    throttle_handle);
}

void ThrottleWorkerMainScriptFetch(
    const base::UnguessableToken& devtools_worker_token,
    const GlobalRenderFrameHostId& ancestor_render_frame_host_id,
    scoped_refptr<DevToolsThrottleHandle> throttle_handle) {
  DedicatedWorkerDevToolsAgentHost* agent_host =
      WorkerDevToolsManager::GetInstance().GetDevToolsHostFromToken(
          devtools_worker_token);
  if (!agent_host) {
    return;
  }

  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id);
  if (!rfh) {
    return;
  }

  FrameTreeNode* ftn = rfh->frame_tree_node();
  DispatchToAgents(ftn, &protocol::TargetHandler::AddWorkerThrottle, agent_host,
                   std::move(throttle_handle));
}

bool ShouldWaitForDebuggerInWindowOpen() {
  for (auto* browser_agent_host : BrowserDevToolsAgentHost::Instances()) {
    for (auto* target_handler :
         protocol::TargetHandler::ForAgentHost(browser_agent_host)) {
      if (target_handler->ShouldThrottlePopups()) {
        return true;
      }
    }
  }
  return false;
}

namespace {
// This is a helper function used in ApplyNetworkRequestOverrides and
// ApplyUserAgentMetadataOverrides to help correctly set network request header
// overrides. It behaves the same as RenderFrameDevToolsAgentHost::GetFor for
// all FrameTreeNodes except those that are prerendering. For prerendering
// FrameTreeNodes, it returns the DevToolsAgentHost of the primary main frame,
// even if it has a DTAH of its own. The network header overrides are applied
// too early, before the correct values sent by the client are propagated to the
// prerender's DTAH's handlers. As a result, we use the values that were
// previously set for the primary main frame.
DevToolsAgentHostImpl* GetDevToolsAgentHostForNetworkOverrides(
    FrameTreeNode* frame_tree_node) {
  if (frame_tree_node->frame_tree().is_prerendering()) {
    return RenderFrameDevToolsAgentHost::GetFor(
        WebContentsImpl::FromFrameTreeNode(frame_tree_node)
            ->GetPrimaryMainFrame()
            ->frame_tree_node());
  }
  return RenderFrameDevToolsAgentHost::GetFor(frame_tree_node);
}

void ApplyNetworkRequestOverrides(
    DevToolsAgentHostImpl* agent_host,
    net::HttpRequestHeaders* headers,
    bool* disable_cache,
    bool* network_instrumentation_enabled,
    bool* skip_service_worker,
    std::optional<std::vector<net::SourceStream::SourceType>>*
        devtools_accepted_stream_types,
    bool* devtools_user_agent_overridden,
    bool* devtools_accept_language_overridden) {
  for (auto* network : protocol::NetworkHandler::ForAgentHost(agent_host)) {
    if (!network->enabled()) {
      continue;
    }
    if (network_instrumentation_enabled) {
      *network_instrumentation_enabled = true;
    }
    network->ApplyOverrides(headers, skip_service_worker, disable_cache,
                            devtools_accepted_stream_types);
  }

  for (auto* emulation : protocol::EmulationHandler::ForAgentHost(agent_host)) {
    bool ua_overridden = false;
    bool accept_language_overridden = false;
    emulation->ApplyOverrides(headers, &ua_overridden,
                              &accept_language_overridden);
    if (devtools_user_agent_overridden) {
      *devtools_user_agent_overridden |= ua_overridden;
    }
    if (devtools_accept_language_overridden) {
      *devtools_accept_language_overridden |= accept_language_overridden;
    }
  }
}

}  // namespace

void ApplyAuctionNetworkRequestOverrides(
    FrameTreeNode* frame_tree_node,
    network::ResourceRequest* request,
    bool* network_instrumentation_enabled) {
  bool disable_cache = false;
  DevToolsAgentHostImpl* agent_host =
      GetDevToolsAgentHostForNetworkOverrides(frame_tree_node);
  if (!agent_host) {
    return;
  }
  ApplyNetworkRequestOverrides(
      agent_host, &request->headers, &disable_cache,
      network_instrumentation_enabled, &request->skip_service_worker,
      &request->devtools_accepted_stream_types, nullptr, nullptr);
  if (disable_cache) {
    request->load_flags = net::LOAD_BYPASS_CACHE;
  }
}

void ApplyNetworkRequestOverrides(
    FrameTreeNode* frame_tree_node,
    blink::mojom::BeginNavigationParams* begin_params,
    bool* report_raw_headers,
    std::optional<std::vector<net::SourceStream::SourceType>>*
        devtools_accepted_stream_types,
    bool* devtools_user_agent_overridden,
    bool* devtools_accept_language_overridden) {
  *devtools_user_agent_overridden = false;
  *devtools_accept_language_overridden = false;
  bool disable_cache = false;
  DevToolsAgentHostImpl* agent_host =
      GetDevToolsAgentHostForNetworkOverrides(frame_tree_node);
  if (!agent_host) {
    return;
  }
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(begin_params->headers);
  ApplyNetworkRequestOverrides(
      agent_host, &headers, &disable_cache, report_raw_headers,
      &begin_params->skip_service_worker, devtools_accepted_stream_types,
      devtools_user_agent_overridden, devtools_accept_language_overridden);
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
    std::optional<blink::UserAgentMetadata>* override_out) {
  DevToolsAgentHostImpl* agent_host =
      GetDevToolsAgentHostForNetworkOverrides(frame_tree_node);
  if (!agent_host) {
    return false;
  }

  bool result = false;
  for (auto* emulation : protocol::EmulationHandler::ForAgentHost(agent_host)) {
    result = emulation->ApplyUserAgentMetadataOverrides(override_out) || result;
  }

  return result;
}

namespace {
template <typename HandlerType>
bool MaybeCreateProxyForInterception(
    DevToolsAgentHostImpl* agent_host,
    int process_id,
    StoragePartition* storage_partition,
    const base::UnguessableToken& frame_token,
    bool is_navigation,
    bool is_download,
    network::mojom::URLLoaderFactoryOverride* agent_override) {
  if (!agent_host) {
    return false;
  }
  bool had_interceptors = false;
  const auto& handlers = HandlerType::ForAgentHost(agent_host);
  for (const auto& handler : base::Reversed(handlers)) {
    had_interceptors |= handler->MaybeCreateProxyForInterception(
        process_id, storage_partition, frame_token, is_navigation, is_download,
        agent_override);
  }
  return had_interceptors;
}

}  // namespace

bool WillCreateURLLoaderFactoryParams::Run(
    bool is_navigation,
    bool is_download,
    network::URLLoaderFactoryBuilder& factory_builder,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  CHECK(!is_download || is_navigation);

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
  bool had_interceptors =
      MaybeCreateProxyForInterception<protocol::NetworkHandler>(
          agent_host_, process_id_, storage_partition_, devtools_token_,
          is_navigation, is_download, handler_override);

  had_interceptors |= MaybeCreateProxyForInterception<protocol::FetchHandler>(
      agent_host_, process_id_, storage_partition_, devtools_token_,
      is_navigation, is_download, handler_override);

  // TODO(caseq): assure deterministic order of browser agents (or sessions).
  for (auto* browser_agent_host : BrowserDevToolsAgentHost::Instances()) {
    had_interceptors |= MaybeCreateProxyForInterception<protocol::FetchHandler>(
        browser_agent_host, process_id_, storage_partition_, devtools_token_,
        is_navigation, is_download, handler_override);
  }
  if (!had_interceptors) {
    return false;
  }
  CHECK(handler_override->overriding_factory);
  CHECK(handler_override->overridden_factory_receiver);
  if (!factory_override) {
    // Not a subresource navigation, so just override the target receiver.
    auto [receiver, remote] = factory_builder.Append();
    mojo::FusePipes(std::move(receiver),
                    std::move(devtools_override.overriding_factory));
    mojo::FusePipes(std::move(devtools_override.overridden_factory_receiver),
                    std::move(remote));
  } else if (!*factory_override) {
    // No other overrides, so just returns ours as is.
    *factory_override = network::mojom::URLLoaderFactoryOverride::New(
        std::move(devtools_override.overriding_factory),
        std::move(devtools_override.overridden_factory_receiver),
        /*skip_cors_enabled_scheme_check=*/false);
  }
  // ... else things are already taken care of, as handler_override was pointing
  // to factory override and we've done all magic in-place.
  CHECK(!devtools_override.overriding_factory);
  CHECK(!devtools_override.overridden_factory_receiver);

  return true;
}

WillCreateURLLoaderFactoryParams::WillCreateURLLoaderFactoryParams(
    DevToolsAgentHostImpl* agent_host,
    const base::UnguessableToken& devtools_token,
    int process_id,
    StoragePartition* storage_partition)
    : agent_host_(agent_host),
      devtools_token_(devtools_token),
      process_id_(process_id),
      storage_partition_(storage_partition) {}

WillCreateURLLoaderFactoryParams WillCreateURLLoaderFactoryParams::ForFrame(
    RenderFrameHostImpl* rfh) {
  return WillCreateURLLoaderFactoryParams(
      RenderFrameDevToolsAgentHost::GetFor(rfh), rfh->GetDevToolsFrameToken(),
      rfh->GetProcess()->GetID(), rfh->GetProcess()->GetStoragePartition());
}

WillCreateURLLoaderFactoryParams
WillCreateURLLoaderFactoryParams::ForServiceWorker(RenderProcessHost& rph,
                                                   int routing_id) {
  ServiceWorkerDevToolsAgentHost* agent_host =
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForWorker(rph.GetID(), routing_id);
  CHECK(agent_host);
  return WillCreateURLLoaderFactoryParams(
      agent_host, agent_host->devtools_worker_token(), rph.GetID(),
      rph.GetStoragePartition());
}

std::optional<WillCreateURLLoaderFactoryParams>
WillCreateURLLoaderFactoryParams::ForServiceWorkerMainScript(
    const ServiceWorkerContextWrapper* context_wrapper,
    std::optional<int64_t> version_id) {
  if (!version_id.has_value()) {
    return std::nullopt;
  }

  // If we have a version_id, we are fetching a worker main script. We have a
  // DevtoolsAgentHost ready for the worker and we can add the devtools override
  // before instantiating the URLFactoryLoader.
  ServiceWorkerDevToolsAgentHost* agent_host =
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForNewInstallingWorker(context_wrapper,
                                                       *version_id);
  CHECK(agent_host);
  return WillCreateURLLoaderFactoryParams(
      agent_host, agent_host->devtools_worker_token(),
      ChildProcessHost::kInvalidUniqueID, context_wrapper->storage_partition());
}

std::optional<WillCreateURLLoaderFactoryParams>
WillCreateURLLoaderFactoryParams::ForSharedWorker(SharedWorkerHost* host) {
  auto* agent_host = SharedWorkerDevToolsAgentHost::GetFor(host);
  if (!agent_host) {
    return std::nullopt;
  }
  RenderProcessHost* rph = agent_host->GetProcessHost();
  CHECK(rph);
  return WillCreateURLLoaderFactoryParams(
      agent_host, agent_host->devtools_worker_token(), rph->GetID(),
      rph->GetStoragePartition());
}

WillCreateURLLoaderFactoryParams
WillCreateURLLoaderFactoryParams::ForWorkerMainScript(
    DevToolsAgentHostImpl* agent_host,
    const base::UnguessableToken& worker_token,
    RenderFrameHostImpl& ancestor_render_frame_host) {
  // Use the ancestor frame's interceptor to align with the interception
  // behavior in the renderer that reuses the same url loader factory from
  // the ancestor frame for the worker.
  return WillCreateURLLoaderFactoryParams::ForFrame(
      &ancestor_render_frame_host);
}

void OnPrefetchRequestWillBeSent(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const GURL& initiator,
    const network::ResourceRequest& request,
    std::optional<std::pair<const GURL&,
                            const network::mojom::URLResponseHeadDevToolsInfo&>>
        redirect_info) {
  auto timestamp = base::TimeTicks::Now();
  std::string frame_token =
      frame_tree_node->current_frame_host()->devtools_frame_token().ToString();
  DispatchToAgents(
      frame_tree_node, &protocol::NetworkHandler::PrefetchRequestWillBeSent,
      request_id, request, initiator, frame_token, timestamp, redirect_info);
}

void OnPrefetchResponseReceived(FrameTreeNode* frame_tree_node,
                                const std::string& request_id,
                                const GURL& url,
                                const network::mojom::URLResponseHead& head) {
  std::string frame_token =
      frame_tree_node->current_frame_host()->devtools_frame_token().ToString();

  network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
      network::ExtractDevToolsInfo(head);
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::ResponseReceived,
                   request_id, request_id, url,
                   protocol::Network::ResourceTypeEnum::Prefetch, *head_info,
                   frame_token);
}
void OnPrefetchRequestComplete(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status) {
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::LoadingComplete,
                   request_id, protocol::Network::ResourceTypeEnum::Prefetch,
                   status);
}
void OnPrefetchBodyDataReceived(FrameTreeNode* frame_tree_node,
                                const std::string& request_id,
                                const std::string& body,
                                bool is_base64_encoded) {
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::BodyDataReceived,
                   request_id, body, is_base64_encoded);
}

void OnAuctionWorkletNetworkRequestWillBeSent(
    FrameTreeNodeId frame_tree_node_id,
    const network::ResourceRequest& request,
    base::TimeTicks timestamp) {
  if (request.devtools_request_id->empty()) {
    return;
  }

  network::mojom::URLRequestDevToolsInfoPtr request_info =
      network::ExtractDevToolsInfo(request);

  GURL initiator_url;
  if (request.request_initiator.has_value()) {
    initiator_url = request.request_initiator->GetURL();
  }
  // if we cannot get the loader_id from the parent, use an empty string.
  std::string loader_id = "";
  if (frame_tree_node_id) {
    FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id);

    if (ftn == nullptr) {
      return;
    }
    const std::optional<base::UnguessableToken>& devtools_navigation_token =
        ftn->current_frame_host()->GetDevToolsNavigationToken();

    if (devtools_navigation_token.has_value()) {
      loader_id = devtools_navigation_token->ToString();
    }

    DispatchToAgents(
        frame_tree_node_id, &protocol::NetworkHandler::RequestSent,
        /*request_id=*/request.devtools_request_id.value(),
        /*loader_id=*/loader_id, request.headers, *request_info,
        /*initiator_type=*/protocol::Network::Initiator::TypeEnum::Other,
        initiator_url,
        /*initiator_devtools_request_id=*/"", timestamp);
  }
}

void OnAuctionWorkletNetworkResponseReceived(
    FrameTreeNodeId frame_tree_node_id,
    const std::string& request_id,
    const std::string& loader_id,
    const GURL& request_url,
    const network::mojom::URLResponseHead& headers) {
  network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
      network::ExtractDevToolsInfo(headers);
  DispatchToAgents(frame_tree_node_id,
                   &protocol::NetworkHandler::ResponseReceived, request_id,
                   loader_id, request_url,
                   /*resource_type=*/protocol::Network::ResourceTypeEnum::Other,
                   *head_info, base::ToString(frame_tree_node_id));
}

void OnAuctionWorkletNetworkRequestComplete(
    FrameTreeNodeId frame_tree_node_id,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status) {
  DispatchToAgents(frame_tree_node_id,
                   &protocol::NetworkHandler::LoadingComplete, request_id,
                   /*resource_type=*/protocol::Network::ResourceTypeEnum::Other,
                   status);
}

bool NeedInterestGroupAuctionEvents(FrameTreeNodeId frame_tree_node_id) {
  FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!ftn) {
    return false;
  }
  DevToolsAgentHostImpl* agent_host = RenderFrameDevToolsAgentHost::GetFor(ftn);
  if (!agent_host) {
    return false;
  }
  for (auto* storage : protocol::StorageHandler::ForAgentHost(agent_host)) {
    if (storage->interest_group_auction_tracking_enabled()) {
      return true;
    }
  }
  return false;
}

void OnInterestGroupAuctionEventOccurred(
    FrameTreeNodeId frame_tree_node_id,
    base::Time event_time,
    InterestGroupAuctionEventType type,
    const std::string& unique_auction_id,
    base::optional_ref<const std::string> parent_auction_id,
    const base::Value::Dict& auction_config) {
  DispatchToAgents(
      frame_tree_node_id,
      &protocol::StorageHandler::NotifyInterestGroupAuctionEventOccurred,
      event_time, type, unique_auction_id, parent_auction_id, auction_config);
}

void OnInterestGroupAuctionNetworkRequestCreated(
    FrameTreeNodeId frame_tree_node_id,
    content::InterestGroupAuctionFetchType type,
    const std::string& request_id,
    const std::vector<std::string>& devtools_auction_ids) {
  DispatchToAgents(frame_tree_node_id,
                   &protocol::StorageHandler::
                       NotifyInterestGroupAuctionNetworkRequestCreated,
                   type, request_id, devtools_auction_ids);
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
       rfh; rfh = rfh->GetParentOrOuterDocument()) {
    // Only check frames that qualify as DevTools targets, i.e. (local)? roots.
    if (!RenderFrameDevToolsAgentHost::ShouldCreateDevToolsForHost(rfh)) {
      continue;
    }
    auto* agent_host = static_cast<RenderFrameDevToolsAgentHost*>(
        RenderFrameDevToolsAgentHost::GetFor(rfh));
    if (!agent_host) {
      continue;
    }
    agent_host->OnNavigationRequestWillBeSent(navigation_request);
  }

  // We use CachedNavigationURLLoader for page activation (BFCache navigations
  // and Prerender activations) and don't actually send a network request, so we
  // don't report this request to DevTools.
  if (navigation_request.IsPageActivation()) {
    return;
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

void FencedFrameCreated(
    base::SafeRef<RenderFrameHostImpl> owner_render_frame_host,
    FencedFrame* fenced_frame) {
  auto* agent_host = static_cast<RenderFrameDevToolsAgentHost*>(
      RenderFrameDevToolsAgentHost::GetFor(
          owner_render_frame_host->frame_tree_node()));
  if (!agent_host) {
    return;
  }
  agent_host->DidCreateFencedFrame(fenced_frame);
}

void WillStartDragging(FrameTreeNode* main_frame_tree_node,
                       const content::DropData& drop_data,
                       const blink::mojom::DragDataPtr drag_data,
                       blink::DragOperationsMask drag_operations_mask,
                       bool* intercepted) {
  DCHECK(main_frame_tree_node->frame_tree().root() == main_frame_tree_node);
  DispatchToAgents(main_frame_tree_node, &protocol::InputHandler::StartDragging,
                   drop_data, *drag_data, drag_operations_mask, intercepted);
}

void DragEnded(FrameTreeNode& node) {
  DCHECK(node.frame_tree().root() == &node);
  DispatchToAgents(&node, &protocol::InputHandler::DragEnded);
}

namespace {
std::unique_ptr<protocol::Array<protocol::String>> BuildExclusionReasons(
    net::CookieInclusionStatus status) {
  auto exclusion_reasons =
      std::make_unique<protocol::Array<protocol::String>>();
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::
              EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX)) {
    exclusion_reasons->push_back(protocol::Audits::CookieExclusionReasonEnum::
                                     ExcludeSameSiteUnspecifiedTreatedAsLax);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE)) {
    exclusion_reasons->push_back(protocol::Audits::CookieExclusionReasonEnum::
                                     ExcludeSameSiteNoneInsecure);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)) {
    exclusion_reasons->push_back(
        protocol::Audits::CookieExclusionReasonEnum::ExcludeSameSiteLax);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)) {
    exclusion_reasons->push_back(
        protocol::Audits::CookieExclusionReasonEnum::ExcludeSameSiteStrict);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_DOMAIN_NON_ASCII)) {
    exclusion_reasons->push_back(
        protocol::Audits::CookieExclusionReasonEnum::ExcludeDomainNonASCII);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::
              EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET)) {
    exclusion_reasons->push_back(
        protocol::Audits::CookieExclusionReasonEnum::
            ExcludeThirdPartyCookieBlockedInFirstPartySet);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT)) {
    exclusion_reasons->push_back(
        protocol::Audits::CookieExclusionReasonEnum::ExcludeThirdPartyPhaseout);
  }

  return exclusion_reasons;
}

std::unique_ptr<protocol::Array<protocol::String>> BuildWarningReasons(
    net::CookieInclusionStatus status) {
  auto warning_reasons = std::make_unique<protocol::Array<protocol::String>>();
  if (status.HasWarningReason(
          net::CookieInclusionStatus::WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnAttributeValueExceedsMaxSize);
  }
  if (status.HasWarningReason(
          net::CookieInclusionStatus::
              WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteUnspecifiedCrossSiteContext);
  }
  if (status.HasWarningReason(
          net::CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE)) {
    warning_reasons->push_back(
        protocol::Audits::CookieWarningReasonEnum::WarnSameSiteNoneInsecure);
  }
  if (status.HasWarningReason(net::CookieInclusionStatus::
                                  WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteUnspecifiedLaxAllowUnsafe);
  }

  // There can only be one of the following warnings.
  if (status.HasWarningReason(net::CookieInclusionStatus::
                                  WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteStrictLaxDowngradeStrict);
  } else if (status.HasWarningReason(
                 net::CookieInclusionStatus::
                     WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteStrictCrossDowngradeStrict);
  } else if (status.HasWarningReason(
                 net::CookieInclusionStatus::
                     WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteStrictCrossDowngradeLax);
  } else if (status.HasWarningReason(
                 net::CookieInclusionStatus::
                     WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteLaxCrossDowngradeStrict);
  } else if (status.HasWarningReason(
                 net::CookieInclusionStatus::
                     WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteLaxCrossDowngradeLax);
  }

  if (status.HasWarningReason(
          net::CookieInclusionStatus::WARN_DOMAIN_NON_ASCII)) {
    warning_reasons->push_back(
        protocol::Audits::CookieWarningReasonEnum::WarnDomainNonASCII);
  }

  if (status.HasWarningReason(
          net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT)) {
    warning_reasons->push_back(
        protocol::Audits::CookieWarningReasonEnum::WarnThirdPartyPhaseout);
  }

  if (status.exemption_reason() ==
      net::CookieInclusionStatus::ExemptionReason::k3PCDMetadata) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnDeprecationTrialMetadata);
  } else if (status.exemption_reason() ==
             net::CookieInclusionStatus::ExemptionReason::k3PCDHeuristics) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnThirdPartyCookieHeuristic);
  }

  // This warning only affects cookies when the corresponding feature is
  // enabled, therefore we should only create an issue for it then.
  if (base::FeatureList::IsEnabled(
          net::features::kCookieSameSiteConsidersRedirectChain) &&
      status.HasWarningReason(
          net::CookieInclusionStatus::
              WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION)) {
    warning_reasons->push_back(
        protocol::Audits::CookieWarningReasonEnum::
            WarnCrossSiteRedirectDowngradeChangesInclusion);
  }

  return warning_reasons;
}

protocol::String BuildCookieOperation(blink::mojom::CookieOperation operation) {
  switch (operation) {
    case blink::mojom::CookieOperation::kReadCookie:
      return protocol::Audits::CookieOperationEnum::ReadCookie;
    case blink::mojom::CookieOperation::kSetCookie:
      return protocol::Audits::CookieOperationEnum::SetCookie;
  }
}

std::unique_ptr<protocol::Audits::InspectorIssue>
BuildCookieDeprecationMetadataIssue(
    const blink::mojom::CookieDeprecationMetadataIssueDetailsPtr&
        issue_details) {
  auto metadata_issue_details =
      protocol::Audits::CookieDeprecationMetadataIssueDetails::Create()
          .SetAllowedSites(std::make_unique<protocol::Array<protocol::String>>(
              issue_details->allowed_sites))
          .SetOptOutPercentage(issue_details->opt_out_percentage)
          .SetIsOptOutTopLevel(issue_details->is_opt_out_top_level)
          .SetOperation(BuildCookieOperation(issue_details->operation))
          .Build();

  auto protocol_issue_details =
      protocol::Audits::InspectorIssueDetails::Create()
          .SetCookieDeprecationMetadataIssueDetails(
              std::move(metadata_issue_details))
          .Build();

  auto issue = protocol::Audits::InspectorIssue::Create()
                   .SetCode(protocol::Audits::InspectorIssueCodeEnum::
                                CookieDeprecationMetadataIssue)
                   .SetDetails(std::move(protocol_issue_details))
                   .Build();

  return issue;
}

}  // namespace

void ReportCookieIssue(
    RenderFrameHostImpl* render_frame_host_impl,
    const network::mojom::CookieOrLineWithAccessResultPtr& excluded_cookie,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    blink::mojom::CookieOperation operation,
    const std::optional<std::string>& devtools_request_id,
    const std::optional<std::string>& devtools_issue_id) {
  auto exclusion_reasons =
      BuildExclusionReasons(excluded_cookie->access_result.status);
  auto warning_reasons =
      BuildWarningReasons(excluded_cookie->access_result.status);
  if (exclusion_reasons->empty() && warning_reasons->empty()) {
    // If we don't report any reason, there is no point in informing DevTools.
    return;
  }

  std::unique_ptr<protocol::Audits::AffectedRequest> affected_request;
  if (devtools_request_id) {
    // We can report the url here, because if devtools_request_id is set, the
    // url is the url of the request.
    affected_request = protocol::Audits::AffectedRequest::Create()
                           .SetRequestId(*devtools_request_id)
                           .SetUrl(url.spec())
                           .Build();
  }

  auto cookie_issue_details =
      protocol::Audits::CookieIssueDetails::Create()
          .SetCookieExclusionReasons(std::move(exclusion_reasons))
          .SetCookieWarningReasons(std::move(warning_reasons))
          .SetOperation(BuildCookieOperation(operation))
          .SetCookieUrl(url.spec())
          .SetRequest(std::move(affected_request))
          .Build();

  if (excluded_cookie->cookie_or_line->is_cookie()) {
    const auto& cookie = excluded_cookie->cookie_or_line->get_cookie();
    auto affected_cookie = protocol::Audits::AffectedCookie::Create()
                               .SetName(cookie.Name())
                               .SetPath(cookie.Path())
                               .SetDomain(cookie.Domain())
                               .Build();
    cookie_issue_details->SetCookie(std::move(affected_cookie));
  } else {
    CHECK(excluded_cookie->cookie_or_line->is_cookie_string());
    cookie_issue_details->SetRawCookieLine(
        excluded_cookie->cookie_or_line->get_cookie_string());
  }

  if (!site_for_cookies.IsNull()) {
    cookie_issue_details->SetSiteForCookies(
        site_for_cookies.RepresentativeUrl().spec());
  }

  auto details = protocol::Audits::InspectorIssueDetails::Create()
                     .SetCookieIssueDetails(std::move(cookie_issue_details))
                     .Build();

  auto issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(protocol::Audits::InspectorIssueCodeEnum::CookieIssue)
          .SetDetails(std::move(details))
          .Build();
  if (devtools_issue_id.has_value()) {
    issue->SetIssueId(devtools_issue_id.value());
  }

  ReportBrowserInitiatedIssue(render_frame_host_impl, issue.get());
}

namespace {

void AddIssueToIssueStorage(
    RenderFrameHost* rfh,
    std::unique_ptr<protocol::Audits::InspectorIssue> issue) {
  // We only utilize a central storage on the page. Each issue is still
  // associated with the originating |RenderFrameHost| though.
  DevToolsIssueStorage* issue_storage =
      DevToolsIssueStorage::GetOrCreateForPage(
          rfh->GetOutermostMainFrame()->GetPage());

  issue_storage->AddInspectorIssue(rfh, std::move(issue));
}

}  // namespace

void ReportBrowserInitiatedIssue(RenderFrameHostImpl* frame,
                                 protocol::Audits::InspectorIssue* issue) {
  FrameTreeNode* ftn = frame->frame_tree_node();
  if (!ftn) {
    return;
  }

  AddIssueToIssueStorage(frame, issue->Clone());
  DispatchToAgents(ftn, &protocol::AuditsHandler::OnIssueAdded, issue);
}

void BuildAndReportBrowserInitiatedIssue(
    RenderFrameHostImpl* frame,
    blink::mojom::InspectorIssueInfoPtr info) {
  std::unique_ptr<protocol::Audits::InspectorIssue> issue;
  if (info->code == blink::mojom::InspectorIssueCode::kHeavyAdIssue) {
    issue = BuildHeavyAdIssue(info->details->heavy_ad_issue_details);
  } else if (info->code ==
             blink::mojom::InspectorIssueCode::kFederatedAuthRequestIssue) {
    issue = BuildFederatedAuthRequestIssue(
        info->details->federated_auth_request_details);
  } else if (info->code ==
             blink::mojom::InspectorIssueCode::kDeprecationIssue) {
    issue = BuildDeprecationIssue(info->details->deprecation_issue_details);
  } else if (info->code ==
             blink::mojom::InspectorIssueCode::kBounceTrackingIssue) {
    issue =
        BuildBounceTrackingIssue(info->details->bounce_tracking_issue_details);
  } else if (info->code == blink::mojom::InspectorIssueCode::
                               kCookieDeprecationMetadataIssue) {
    issue = BuildCookieDeprecationMetadataIssue(
        info->details->cookie_deprecation_metadata_issue_details);
  } else if (info->code == blink::mojom::InspectorIssueCode::
                               kFederatedAuthUserInfoRequestIssue) {
    issue = BuildFederatedAuthUserInfoRequestIssue(
        info->details->federated_auth_user_info_request_details);
  } else if (info->code ==
             blink::mojom::InspectorIssueCode::kAttributionReportingIssue) {
    issue = BuildAttributionReportingIssue(
        info->details->attribution_reporting_issue_details);
  } else {
    NOTREACHED_IN_MIGRATION() << "Unsupported type of browser-initiated issue";
  }
  ReportBrowserInitiatedIssue(frame, issue.get());
}

void OnWebTransportHandshakeFailed(
    RenderFrameHostImpl* frame,
    const GURL& url,
    const std::optional<net::WebTransportError>& error) {
  FrameTreeNode* ftn = frame->frame_tree_node();
  if (!ftn) {
    return;
  }
  std::string text = base::StringPrintf(
      "Failed to establish a connection to %s", url.spec().c_str());
  if (error) {
    text += ": ";
    text += net::WebTransportErrorToString(*error);
  }
  text += ".";
  auto entry =
      protocol::Log::LogEntry::Create()
          .SetSource(protocol::Log::LogEntry::SourceEnum::Network)
          .SetLevel(protocol::Log::LogEntry::LevelEnum::Error)
          .SetText(text)
          .SetTimestamp(base::Time::Now().InMillisecondsFSinceUnixEpoch())
          .Build();
  DispatchToAgents(ftn, &protocol::LogHandler::EntryAdded, entry.get());
}

void OnServiceWorkerMainScriptFetchingFailed(
    const GlobalRenderFrameHostId& requesting_frame_id,
    const ServiceWorkerContextWrapper* context_wrapper,
    int64_t version_id,
    const std::string& error,
    const network::URLLoaderCompletionStatus& status,
    const network::mojom::URLResponseHead* response_head,
    const GURL& url) {
  DCHECK(!error.empty());
  DCHECK_NE(net::OK, status.error_code);

  // If we have a requesting_frame_id, we should have a frame and a frame tree
  // node. However since the lifetime of these objects can be complex, we check
  // at each step that we indeed can go reach all the way to the FrameTreeNode.
  if (!requesting_frame_id) {
    return;
  }

  RenderFrameHostImpl* requesting_frame =
      RenderFrameHostImpl::FromID(requesting_frame_id);
  if (!requesting_frame) {
    return;
  }

  FrameTreeNode* ftn = requesting_frame->frame_tree_node();
  if (!ftn) {
    return;
  }

  auto entry =
      protocol::Log::LogEntry::Create()
          .SetSource(protocol::Log::LogEntry::SourceEnum::Network)
          .SetLevel(protocol::Log::LogEntry::LevelEnum::Error)
          .SetText(error)
          .SetTimestamp(base::Time::Now().InMillisecondsFSinceUnixEpoch())
          .Build();
  DispatchToAgents(ftn, &protocol::LogHandler::EntryAdded, entry.get());

  ServiceWorkerDevToolsAgentHost* agent_host =
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForNewInstallingWorker(context_wrapper,
                                                       version_id);

  if (response_head) {
    DCHECK(agent_host);
    network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
        network::ExtractDevToolsInfo(*response_head);
    auto worker_token = agent_host->devtools_worker_token().ToString();
    for (auto* network_handler :
         protocol::NetworkHandler::ForAgentHost(agent_host)) {
      network_handler->ResponseReceived(
          worker_token, worker_token, url,
          protocol::Network::ResourceTypeEnum::Other, *head_info,
          requesting_frame->devtools_frame_token().ToString());
      network_handler->frontend()->LoadingFinished(
          worker_token,
          status.completion_time.ToInternalValue() /
              static_cast<double>(base::Time::kMicrosecondsPerSecond),
          status.encoded_data_length);
    }
  } else if (agent_host) {
    for (auto* network_handler :
         protocol::NetworkHandler::ForAgentHost(agent_host)) {
      network_handler->LoadingComplete(
          agent_host->devtools_worker_token().ToString(),
          protocol::Network::ResourceTypeEnum::Other, status);
    }
  }
}

namespace {

// Only assign request id if there's an enabled agent host.
void MaybeAssignResourceRequestId(DevToolsAgentHostImpl* host,
                                  const std::string& id,
                                  network::ResourceRequest& request) {
  DCHECK(!request.devtools_request_id.has_value());
  for (auto* network_handler : protocol::NetworkHandler::ForAgentHost(host)) {
    if (network_handler->enabled()) {
      request.devtools_request_id = id;
      return;
    }
  }
}

}  // namespace

void MaybeAssignResourceRequestId(FrameTreeNode* ftn,
                                  const std::string& id,
                                  network::ResourceRequest& request) {
  if (auto* host = RenderFrameDevToolsAgentHost::GetFor(ftn)) {
    MaybeAssignResourceRequestId(host, id, request);
  }
}

void OnServiceWorkerMainScriptRequestWillBeSent(
    const GlobalRenderFrameHostId& requesting_frame_id,
    const ServiceWorkerContextWrapper* context_wrapper,
    int64_t version_id,
    network::ResourceRequest& request) {
  // Currently, `requesting_frame_id` is invalid when payment apps and
  // extensions register a service worker. See the callers of
  // ServiceWorkerContextWrapper::RegisterServiceWorker().
  if (!requesting_frame_id) {
    return;
  }

  RenderFrameHostImpl* requesting_frame =
      RenderFrameHostImpl::FromID(requesting_frame_id);
  if (!requesting_frame) {
    return;
  }

  auto timestamp = base::TimeTicks::Now();
  network::mojom::URLRequestDevToolsInfoPtr request_info =
      network::ExtractDevToolsInfo(request);

  ServiceWorkerDevToolsAgentHost* agent_host =
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForNewInstallingWorker(context_wrapper,
                                                       version_id);
  DCHECK(agent_host);
  const std::string request_id = agent_host->devtools_worker_token().ToString();
  MaybeAssignResourceRequestId(agent_host, request_id, request);
  for (auto* network_handler :
       protocol::NetworkHandler::ForAgentHost(agent_host)) {
    network_handler->RequestSent(
        request_id,
        /*loader_id=*/"", request.headers, *request_info,
        protocol::Network::Initiator::TypeEnum::Other,
        requesting_frame->GetLastCommittedURL(),
        /*initiator_devtools_request_id=*/"", timestamp);
  }
}

void OnWorkerMainScriptLoadingFailed(
    const GURL& url,
    const base::UnguessableToken& worker_token,
    FrameTreeNode* ftn,
    RenderFrameHostImpl* ancestor_rfh,
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(ftn);

  std::string id = worker_token.ToString();

  if (status.blocked_by_response_reason) {
    ReportBlockedByResponseIssue(url, id, ftn, ancestor_rfh, status);
  }

  DispatchToAgents(ftn, &protocol::NetworkHandler::LoadingComplete, id,
                   protocol::Network::ResourceTypeEnum::Other, status);
}

void OnWorkerMainScriptLoadingFinished(
    FrameTreeNode* ftn,
    const base::UnguessableToken& worker_token,
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(ftn);
  DispatchToAgents(ftn, &protocol::NetworkHandler::LoadingComplete,
                   worker_token.ToString(),
                   protocol::Network::ResourceTypeEnum::Other, status);
}

void OnWorkerMainScriptRequestWillBeSent(
    FrameTreeNode* ftn,
    const base::UnguessableToken& worker_token,
    network::ResourceRequest& request) {
  DCHECK(ftn);

  auto timestamp = base::TimeTicks::Now();
  network::mojom::URLRequestDevToolsInfoPtr request_info =
      network::ExtractDevToolsInfo(request);

  auto* owner_host = RenderFrameDevToolsAgentHost::GetFor(ftn);
  if (!owner_host) {
    return;
  }
  MaybeAssignResourceRequestId(owner_host, worker_token.ToString(), request);

  // Note: we apply overrides from the owner frame to match the behavior in the
  // renderer.
  bool disable_cache = false;
  ApplyNetworkRequestOverrides(owner_host, &request.headers, &disable_cache,
                               nullptr, &request.skip_service_worker,
                               &request.devtools_accepted_stream_types, nullptr,
                               nullptr);
  if (disable_cache) {
    request.load_flags &=
        ~(net::LOAD_VALIDATE_CACHE | net::LOAD_SKIP_CACHE_VALIDATION |
          net::LOAD_ONLY_FROM_CACHE | net::LOAD_DISABLE_CACHE);
    request.load_flags |= net::LOAD_BYPASS_CACHE;
  }

  DispatchToAgents(
      ftn, &protocol::NetworkHandler::RequestSent, worker_token.ToString(),
      /*loader_id=*/"", request.headers, *request_info,
      protocol::Network::Initiator::TypeEnum::Other, ftn->current_url(),
      /*initiator_devtools_request_id*/ "", timestamp);
}

void LogWorkletMessage(RenderFrameHostImpl& frame_host,
                       blink::mojom::ConsoleMessageLevel log_level,
                       const std::string& message) {
  FrameTreeNode* ftn = frame_host.frame_tree_node();
  if (!ftn) {
    return;
  }

  std::string log_level_string;
  switch (log_level) {
    case blink::mojom::ConsoleMessageLevel::kVerbose:
      log_level_string = protocol::Log::LogEntry::LevelEnum::Verbose;
      break;
    case blink::mojom::ConsoleMessageLevel::kInfo:
      log_level_string = protocol::Log::LogEntry::LevelEnum::Info;
      break;
    case blink::mojom::ConsoleMessageLevel::kWarning:
      log_level_string = protocol::Log::LogEntry::LevelEnum::Warning;
      break;
    case blink::mojom::ConsoleMessageLevel::kError:
      log_level_string = protocol::Log::LogEntry::LevelEnum::Error;
      break;
  }

  DCHECK(!log_level_string.empty());

  auto entry =
      protocol::Log::LogEntry::Create()
          .SetSource(protocol::Log::LogEntry::SourceEnum::Other)
          .SetLevel(log_level_string)
          .SetText(message)
          .SetTimestamp(base::Time::Now().InMillisecondsFSinceUnixEpoch())
          .Build();
  DispatchToAgents(ftn, &protocol::LogHandler::EntryAdded, entry.get());

  // Manually trigger RenderFrameHostImpl::DidAddMessageToConsole, so that the
  // observer behavior aligns more with the observer behavior for the regular
  // devtools logging path from the renderer.
  frame_host.DidAddMessageToConsole(log_level, base::UTF8ToUTF16(message),
                                    /*line_no=*/0, /*source_id=*/{},
                                    /*untrusted_stack_trace=*/{});
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

protocol::Audits::GenericIssueErrorType GenericIssueErrorTypeToProtocol(
    blink::mojom::GenericIssueErrorType error_type) {
  switch (error_type) {
    case blink::mojom::GenericIssueErrorType::kFormLabelForNameError:
      return protocol::Audits::GenericIssueErrorTypeEnum::FormLabelForNameError;
    case blink::mojom::GenericIssueErrorType::kFormDuplicateIdForInputError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormDuplicateIdForInputError;
    case blink::mojom::GenericIssueErrorType::kFormInputWithNoLabelError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormInputWithNoLabelError;
    case blink::mojom::GenericIssueErrorType::
        kFormAutocompleteAttributeEmptyError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormAutocompleteAttributeEmptyError;
    case blink::mojom::GenericIssueErrorType::
        kFormEmptyIdAndNameAttributesForInputError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormEmptyIdAndNameAttributesForInputError;
    case blink::mojom::GenericIssueErrorType::
        kFormAriaLabelledByToNonExistingId:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormAriaLabelledByToNonExistingId;
    case blink::mojom::GenericIssueErrorType::
        kFormInputAssignedAutocompleteValueToIdOrNameAttributeError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormInputAssignedAutocompleteValueToIdOrNameAttributeError;
    case blink::mojom::GenericIssueErrorType::
        kFormLabelHasNeitherForNorNestedInput:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormLabelHasNeitherForNorNestedInput;
    case blink::mojom::GenericIssueErrorType::
        kFormLabelForMatchesNonExistingIdError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormLabelForMatchesNonExistingIdError;
    case blink::mojom::GenericIssueErrorType::
        kFormInputHasWrongButWellIntendedAutocompleteValueError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormInputHasWrongButWellIntendedAutocompleteValueError;
    case blink::mojom::GenericIssueErrorType::kResponseWasBlockedByORB:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          ResponseWasBlockedByORB;
  }
}

void UpdateDeviceRequestPrompt(RenderFrameHost* render_frame_host,
                               DevtoolsDeviceRequestPromptInfo* prompt_info) {
  FrameTreeNode* ftn = FrameTreeNode::From(render_frame_host);
  if (!ftn) {
    return;
  }
  DispatchToAgents(ftn,
                   &protocol::DeviceAccessHandler::UpdateDeviceRequestPrompt,
                   prompt_info);
}

void CleanUpDeviceRequestPrompt(RenderFrameHost* render_frame_host,
                                DevtoolsDeviceRequestPromptInfo* prompt_info) {
  FrameTreeNode* ftn = FrameTreeNode::From(render_frame_host);
  if (!ftn) {
    return;
  }
  DispatchToAgents(ftn,
                   &protocol::DeviceAccessHandler::CleanUpDeviceRequestPrompt,
                   prompt_info);
}

void WillSendFedCmRequest(RenderFrameHost& render_frame_host,
                          bool* intercept,
                          bool* disable_delay) {
  FrameTreeNode* ftn = FrameTreeNode::From(&render_frame_host);
  if (!ftn) {
    return;
  }
  DispatchToAgents(ftn, &protocol::FedCmHandler::WillSendRequest, intercept,
                   disable_delay);
}

void WillShowFedCmDialog(RenderFrameHost& render_frame_host, bool* intercept) {
  FrameTreeNode* ftn = FrameTreeNode::From(&render_frame_host);
  if (!ftn) {
    return;
  }
  DispatchToAgents(ftn, &protocol::FedCmHandler::WillShowDialog, intercept);
}

void DidShowFedCmDialog(RenderFrameHost& render_frame_host) {
  FrameTreeNode* ftn = FrameTreeNode::From(&render_frame_host);
  if (!ftn) {
    return;
  }
  DispatchToAgents(ftn, &protocol::FedCmHandler::DidShowDialog);
}

void DidCloseFedCmDialog(RenderFrameHost& render_frame_host) {
  FrameTreeNode* ftn = FrameTreeNode::From(&render_frame_host);
  if (!ftn) {
    return;
  }
  DispatchToAgents(ftn, &protocol::FedCmHandler::DidCloseDialog);
}

void OnFencedFrameReportRequestSent(
    FrameTreeNodeId initiator_frame_tree_node_id,
    const std::string& devtools_request_id,
    network::ResourceRequest& request,
    const std::string& event_data) {
  DispatchToAgents(initiator_frame_tree_node_id,
                   &protocol::NetworkHandler::FencedFrameReportRequestSent,
                   /*request_id=*/devtools_request_id, request, event_data,
                   base::TimeTicks::Now());
}

void OnFencedFrameReportResponseReceived(
    FrameTreeNodeId initiator_frame_tree_node_id,
    const std::string& devtools_request_id,
    const GURL& final_url,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  network::mojom::URLResponseHeadDevToolsInfoPtr response_info =
      network::mojom::URLResponseHeadDevToolsInfo::New();
  response_info->headers = headers;

  DispatchToAgents(initiator_frame_tree_node_id,
                   &protocol::NetworkHandler::ResponseReceived,
                   /*request_id=*/devtools_request_id,
                   /*loader_id=*/devtools_request_id, final_url,
                   protocol::Network::ResourceTypeEnum::Other, *response_info,
                   /*frame_id=*/protocol::Maybe<std::string>());

  DispatchToAgents(initiator_frame_tree_node_id,
                   &protocol::NetworkHandler::LoadingComplete,
                   /*request_id=*/devtools_request_id,
                   protocol::Network::Initiator::TypeEnum::Other,
                   network::URLLoaderCompletionStatus(net::OK));
}

void DidChangeFrameLoadingState(FrameTreeNode& ftn) {
  DispatchToAgents(&ftn, &protocol::PageHandler::DidChangeFrameLoadingState,
                   ftn);
}

}  // namespace devtools_instrumentation

}  // namespace content
