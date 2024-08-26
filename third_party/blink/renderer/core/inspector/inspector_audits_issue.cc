// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"

#include "base/unguessable_token.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_security_policy_violation_event_init.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/protocol/audits.h"
#include "third_party/blink/renderer/core/inspector/protocol/network.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"

namespace blink {

AuditsIssue::AuditsIssue(
    std::unique_ptr<protocol::Audits::InspectorIssue> issue)
    : issue_(std::move(issue)) {}

AuditsIssue::AuditsIssue(AuditsIssue&&) = default;
AuditsIssue& AuditsIssue::operator=(AuditsIssue&&) = default;
AuditsIssue::~AuditsIssue() = default;

std::unique_ptr<protocol::Audits::InspectorIssue> AuditsIssue::TakeIssue() {
  return std::move(issue_);
}

void AuditsIssue::ReportQuirksModeIssue(ExecutionContext* execution_context,
                                        bool isLimitedQuirksMode,
                                        DOMNodeId document_node_id,
                                        String url,
                                        String frame_id,
                                        String loader_id) {
  auto quirks_mode_issue_details =
      protocol::Audits::QuirksModeIssueDetails::create()
          .setIsLimitedQuirksMode(isLimitedQuirksMode)
          .setDocumentNodeId(document_node_id)
          .setUrl(url)
          .setFrameId(frame_id)
          .setLoaderId(loader_id)
          .build();

  auto details =
      protocol::Audits::InspectorIssueDetails::create()
          .setQuirksModeIssueDetails(std::move(quirks_mode_issue_details))
          .build();

  auto issue =
      protocol::Audits::InspectorIssue::create()
          .setCode(protocol::Audits::InspectorIssueCodeEnum::QuirksModeIssue)
          .setDetails(std::move(details))
          .build();
  execution_context->AddInspectorIssue(AuditsIssue(std::move(issue)));
}

namespace {

protocol::Network::CorsError RendererCorsIssueCodeToProtocol(
    RendererCorsIssueCode code) {
  switch (code) {
    case RendererCorsIssueCode::kCorsDisabledScheme:
      return protocol::Network::CorsErrorEnum::CorsDisabledScheme;
    case RendererCorsIssueCode::kNoCorsRedirectModeNotFollow:
      return protocol::Network::CorsErrorEnum::NoCorsRedirectModeNotFollow;
    case RendererCorsIssueCode::kDisallowedByMode:
      return protocol::Network::CorsErrorEnum::DisallowedByMode;
  }
}
}  // namespace

std::unique_ptr<protocol::Audits::SourceCodeLocation> CreateProtocolLocation(
    const SourceLocation& location) {
  auto protocol_location = protocol::Audits::SourceCodeLocation::create()
                               .setUrl(location.Url())
                               .setLineNumber(location.LineNumber() - 1)
                               .setColumnNumber(location.ColumnNumber())
                               .build();
  if (location.ScriptId()) {
    protocol_location->setScriptId(WTF::String::Number(location.ScriptId()));
  }
  return protocol_location;
}

protocol::Audits::GenericIssueErrorType
AuditsIssue::GenericIssueErrorTypeToProtocol(
    mojom::blink::GenericIssueErrorType error_type) {
  switch (error_type) {
    case mojom::blink::GenericIssueErrorType::kFormLabelForNameError:
      return protocol::Audits::GenericIssueErrorTypeEnum::FormLabelForNameError;
    case mojom::blink::GenericIssueErrorType::kFormDuplicateIdForInputError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormDuplicateIdForInputError;
    case mojom::blink::GenericIssueErrorType::kFormInputWithNoLabelError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormInputWithNoLabelError;
    case mojom::blink::GenericIssueErrorType::
        kFormAutocompleteAttributeEmptyError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormAutocompleteAttributeEmptyError;
    case mojom::blink::GenericIssueErrorType::
        kFormEmptyIdAndNameAttributesForInputError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormEmptyIdAndNameAttributesForInputError;
    case mojom::blink::GenericIssueErrorType::
        kFormAriaLabelledByToNonExistingId:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormAriaLabelledByToNonExistingId;
    case mojom::blink::GenericIssueErrorType::
        kFormInputAssignedAutocompleteValueToIdOrNameAttributeError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormInputAssignedAutocompleteValueToIdOrNameAttributeError;
    case mojom::blink::GenericIssueErrorType::
        kFormLabelHasNeitherForNorNestedInput:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormLabelHasNeitherForNorNestedInput;
    case mojom::blink::GenericIssueErrorType::
        kFormLabelForMatchesNonExistingIdError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormLabelForMatchesNonExistingIdError;
    case mojom::blink::GenericIssueErrorType::
        kFormInputHasWrongButWellIntendedAutocompleteValueError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormInputHasWrongButWellIntendedAutocompleteValueError;
    case mojom::blink::GenericIssueErrorType::kResponseWasBlockedByORB:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          ResponseWasBlockedByORB;
  }
}

void AuditsIssue::ReportCorsIssue(
    ExecutionContext* execution_context,
    int64_t identifier,
    RendererCorsIssueCode code,
    String url,
    String initiator_origin,
    String failedParameter,
    std::optional<base::UnguessableToken> issue_id) {
  String devtools_request_id =
      IdentifiersFactory::SubresourceRequestId(identifier);
  std::unique_ptr<protocol::Audits::AffectedRequest> affected_request =
      protocol::Audits::AffectedRequest::create()
          .setRequestId(devtools_request_id)
          .setUrl(url)
          .build();
  auto protocol_cors_error_status =
      protocol::Network::CorsErrorStatus::create()
          .setCorsError(RendererCorsIssueCodeToProtocol(code))
          .setFailedParameter(failedParameter)
          .build();
  auto cors_issue_details =
      protocol::Audits::CorsIssueDetails::create()
          .setIsWarning(false)
          .setRequest(std::move(affected_request))
          .setCorsErrorStatus(std::move(protocol_cors_error_status))
          .build();
  cors_issue_details->setInitiatorOrigin(initiator_origin);
  auto location = CaptureSourceLocation(execution_context);
  if (location) {
    cors_issue_details->setLocation(CreateProtocolLocation(*location));
  }
  auto details = protocol::Audits::InspectorIssueDetails::create()
                     .setCorsIssueDetails(std::move(cors_issue_details))
                     .build();
  auto issue = protocol::Audits::InspectorIssue::create()
                   .setCode(protocol::Audits::InspectorIssueCodeEnum::CorsIssue)
                   .setDetails(std::move(details))
                   .build();
  if (issue_id) {
    issue->setIssueId(IdentifiersFactory::IdFromToken(*issue_id));
  }
  execution_context->AddInspectorIssue(AuditsIssue(std::move(issue)));
}

namespace {

using mojom::blink::AttributionReportingIssueType;

protocol::Audits::AttributionReportingIssueType
BuildAttributionReportingIssueType(AttributionReportingIssueType type) {
  switch (type) {
    case AttributionReportingIssueType::kPermissionPolicyDisabled:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          PermissionPolicyDisabled;
    case AttributionReportingIssueType::kUntrustworthyReportingOrigin:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          UntrustworthyReportingOrigin;
    case AttributionReportingIssueType::kInsecureContext:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          InsecureContext;
    case AttributionReportingIssueType::kInvalidRegisterSourceHeader:
      return protocol::Audits::AttributionReportingIssueTypeEnum::InvalidHeader;
    case AttributionReportingIssueType::kInvalidRegisterTriggerHeader:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          InvalidRegisterTriggerHeader;
    case AttributionReportingIssueType::kSourceAndTriggerHeaders:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          SourceAndTriggerHeaders;
    case AttributionReportingIssueType::kSourceIgnored:
      return protocol::Audits::AttributionReportingIssueTypeEnum::SourceIgnored;
    case AttributionReportingIssueType::kTriggerIgnored:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          TriggerIgnored;
    case AttributionReportingIssueType::kOsSourceIgnored:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          OsSourceIgnored;
    case AttributionReportingIssueType::kOsTriggerIgnored:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          OsTriggerIgnored;
    case AttributionReportingIssueType::kInvalidRegisterOsSourceHeader:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          InvalidRegisterOsSourceHeader;
    case AttributionReportingIssueType::kInvalidRegisterOsTriggerHeader:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          InvalidRegisterOsTriggerHeader;
    case AttributionReportingIssueType::kWebAndOsHeaders:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          WebAndOsHeaders;
    case AttributionReportingIssueType::kNoWebOrOsSupport:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          NoWebOrOsSupport;
    case AttributionReportingIssueType::
        kNavigationRegistrationWithoutTransientUserActivation:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          NavigationRegistrationWithoutTransientUserActivation;
    case AttributionReportingIssueType::kInvalidInfoHeader:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          InvalidInfoHeader;
    case AttributionReportingIssueType::kNoRegisterSourceHeader:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          NoRegisterSourceHeader;
    case AttributionReportingIssueType::kNoRegisterTriggerHeader:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          NoRegisterTriggerHeader;
    case AttributionReportingIssueType::kNoRegisterOsSourceHeader:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          NoRegisterOsSourceHeader;
    case AttributionReportingIssueType::kNoRegisterOsTriggerHeader:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          NoRegisterOsTriggerHeader;
    case AttributionReportingIssueType::
        kNavigationRegistrationUniqueScopeAlreadySet:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          NavigationRegistrationUniqueScopeAlreadySet;
  }
}

}  // namespace

void AuditsIssue::ReportAttributionIssue(
    ExecutionContext* execution_context,
    AttributionReportingIssueType type,
    Element* element,
    const String& request_id,
    const String& invalid_parameter) {
  auto details = protocol::Audits::AttributionReportingIssueDetails::create()
                     .setViolationType(BuildAttributionReportingIssueType(type))
                     .build();

  if (element) {
    details->setViolatingNodeId(element->GetDomNodeId());
  }
  if (!request_id.IsNull()) {
    details->setRequest(protocol::Audits::AffectedRequest::create()
                            .setRequestId(request_id)
                            .build());
  }
  if (!invalid_parameter.IsNull()) {
    details->setInvalidParameter(invalid_parameter);
  }

  auto issue_details =
      protocol::Audits::InspectorIssueDetails::create()
          .setAttributionReportingIssueDetails(std::move(details))
          .build();
  auto issue = protocol::Audits::InspectorIssue::create()
                   .setCode(protocol::Audits::InspectorIssueCodeEnum::
                                AttributionReportingIssue)
                   .setDetails(std::move(issue_details))
                   .build();
  execution_context->AddInspectorIssue(AuditsIssue(std::move(issue)));
}

namespace {

protocol::Audits::SharedArrayBufferIssueType
SharedArrayBufferIssueTypeToProtocol(SharedArrayBufferIssueType issue_type) {
  switch (issue_type) {
    case SharedArrayBufferIssueType::kTransferIssue:
      return protocol::Audits::SharedArrayBufferIssueTypeEnum::TransferIssue;
    case SharedArrayBufferIssueType::kCreationIssue:
      return protocol::Audits::SharedArrayBufferIssueTypeEnum::CreationIssue;
  }
}

protocol::Audits::BlockedByResponseReason BlockedByResponseReasonToProtocol(
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
    case network::mojom::BlockedByResponseReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByDip:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByDip;
    case network::mojom::BlockedByResponseReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip;
    case network::mojom::BlockedByResponseReason::kCorpNotSameSite:
      return protocol::Audits::BlockedByResponseReasonEnum::CorpNotSameSite;
  }
}

protocol::Audits::MixedContentResourceType
RequestContextToMixedContentResourceType(
    mojom::blink::RequestContextType request_context) {
  switch (request_context) {
    case mojom::blink::RequestContextType::ATTRIBUTION_SRC:
      return protocol::Audits::MixedContentResourceTypeEnum::AttributionSrc;
    case mojom::blink::RequestContextType::AUDIO:
      return protocol::Audits::MixedContentResourceTypeEnum::Audio;
    case mojom::blink::RequestContextType::BEACON:
      return protocol::Audits::MixedContentResourceTypeEnum::Beacon;
    case mojom::blink::RequestContextType::CSP_REPORT:
      return protocol::Audits::MixedContentResourceTypeEnum::CSPReport;
    case mojom::blink::RequestContextType::DOWNLOAD:
      return protocol::Audits::MixedContentResourceTypeEnum::Download;
    case mojom::blink::RequestContextType::EMBED:
      return protocol::Audits::MixedContentResourceTypeEnum::PluginResource;
    case mojom::blink::RequestContextType::EVENT_SOURCE:
      return protocol::Audits::MixedContentResourceTypeEnum::EventSource;
    case mojom::blink::RequestContextType::FAVICON:
      return protocol::Audits::MixedContentResourceTypeEnum::Favicon;
    case mojom::blink::RequestContextType::FETCH:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case mojom::blink::RequestContextType::FONT:
      return protocol::Audits::MixedContentResourceTypeEnum::Font;
    case mojom::blink::RequestContextType::FORM:
      return protocol::Audits::MixedContentResourceTypeEnum::Form;
    case mojom::blink::RequestContextType::FRAME:
      return protocol::Audits::MixedContentResourceTypeEnum::Frame;
    case mojom::blink::RequestContextType::HYPERLINK:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case mojom::blink::RequestContextType::IFRAME:
      return protocol::Audits::MixedContentResourceTypeEnum::Frame;
    case mojom::blink::RequestContextType::IMAGE:
      return protocol::Audits::MixedContentResourceTypeEnum::Image;
    case mojom::blink::RequestContextType::IMAGE_SET:
      return protocol::Audits::MixedContentResourceTypeEnum::Image;
    case mojom::blink::RequestContextType::INTERNAL:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case mojom::blink::RequestContextType::JSON:
      // TODO(crbug.com/1511738): Consider adding a type
      // specific to JSON modules requests
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case mojom::blink::RequestContextType::LOCATION:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case mojom::blink::RequestContextType::MANIFEST:
      return protocol::Audits::MixedContentResourceTypeEnum::Manifest;
    case mojom::blink::RequestContextType::OBJECT:
      return protocol::Audits::MixedContentResourceTypeEnum::PluginResource;
    case mojom::blink::RequestContextType::PING:
      return protocol::Audits::MixedContentResourceTypeEnum::Ping;
    case mojom::blink::RequestContextType::PLUGIN:
      return protocol::Audits::MixedContentResourceTypeEnum::PluginData;
    case mojom::blink::RequestContextType::PREFETCH:
      return protocol::Audits::MixedContentResourceTypeEnum::Prefetch;
    case mojom::blink::RequestContextType::SCRIPT:
      return protocol::Audits::MixedContentResourceTypeEnum::Script;
    case mojom::blink::RequestContextType::SERVICE_WORKER:
      return protocol::Audits::MixedContentResourceTypeEnum::ServiceWorker;
    case mojom::blink::RequestContextType::SHARED_WORKER:
      return protocol::Audits::MixedContentResourceTypeEnum::SharedWorker;
    case mojom::blink::RequestContextType::SPECULATION_RULES:
      return protocol::Audits::MixedContentResourceTypeEnum::SpeculationRules;
    case mojom::blink::RequestContextType::STYLE:
      return protocol::Audits::MixedContentResourceTypeEnum::Stylesheet;
    case mojom::blink::RequestContextType::SUBRESOURCE:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case mojom::blink::RequestContextType::SUBRESOURCE_WEBBUNDLE:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case mojom::blink::RequestContextType::TRACK:
      return protocol::Audits::MixedContentResourceTypeEnum::Track;
    case mojom::blink::RequestContextType::UNSPECIFIED:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case mojom::blink::RequestContextType::VIDEO:
      return protocol::Audits::MixedContentResourceTypeEnum::Video;
    case mojom::blink::RequestContextType::WORKER:
      return protocol::Audits::MixedContentResourceTypeEnum::Worker;
    case mojom::blink::RequestContextType::XML_HTTP_REQUEST:
      return protocol::Audits::MixedContentResourceTypeEnum::XMLHttpRequest;
    case mojom::blink::RequestContextType::XSLT:
      return protocol::Audits::MixedContentResourceTypeEnum::XSLT;
  }
}

protocol::Audits::MixedContentResolutionStatus
MixedContentResolutionStatusToProtocol(
    MixedContentResolutionStatus resolution_type) {
  switch (resolution_type) {
    case MixedContentResolutionStatus::kMixedContentBlocked:
      return protocol::Audits::MixedContentResolutionStatusEnum::
          MixedContentBlocked;
    case MixedContentResolutionStatus::kMixedContentAutomaticallyUpgraded:
      return protocol::Audits::MixedContentResolutionStatusEnum::
          MixedContentAutomaticallyUpgraded;
    case MixedContentResolutionStatus::kMixedContentWarning:
      return protocol::Audits::MixedContentResolutionStatusEnum::
          MixedContentWarning;
  }
}

protocol::Audits::ContentSecurityPolicyViolationType CSPViolationTypeToProtocol(
    ContentSecurityPolicyViolationType violation_type) {
  switch (violation_type) {
    case ContentSecurityPolicyViolationType::kEvalViolation:
      return protocol::Audits::ContentSecurityPolicyViolationTypeEnum::
          KEvalViolation;
    case ContentSecurityPolicyViolationType::kWasmEvalViolation:
      return protocol::Audits::ContentSecurityPolicyViolationTypeEnum::
          KWasmEvalViolation;
    case ContentSecurityPolicyViolationType::kInlineViolation:
      return protocol::Audits::ContentSecurityPolicyViolationTypeEnum::
          KInlineViolation;
    case ContentSecurityPolicyViolationType::kTrustedTypesPolicyViolation:
      return protocol::Audits::ContentSecurityPolicyViolationTypeEnum::
          KTrustedTypesPolicyViolation;
    case ContentSecurityPolicyViolationType::kTrustedTypesSinkViolation:
      return protocol::Audits::ContentSecurityPolicyViolationTypeEnum::
          KTrustedTypesSinkViolation;
    case ContentSecurityPolicyViolationType::kURLViolation:
      return protocol::Audits::ContentSecurityPolicyViolationTypeEnum::
          KURLViolation;
  }
}

}  // namespace

void AuditsIssue::ReportSharedArrayBufferIssue(
    ExecutionContext* execution_context,
    bool shared_buffer_transfer_allowed,
    SharedArrayBufferIssueType issue_type) {
  auto source_location = CaptureSourceLocation(execution_context);
  auto sab_issue_details =
      protocol::Audits::SharedArrayBufferIssueDetails::create()
          .setSourceCodeLocation(CreateProtocolLocation(*source_location))
          .setIsWarning(shared_buffer_transfer_allowed)
          .setType(SharedArrayBufferIssueTypeToProtocol(issue_type))
          .build();
  auto issue_details =
      protocol::Audits::InspectorIssueDetails::create()
          .setSharedArrayBufferIssueDetails(std::move(sab_issue_details))
          .build();
  auto issue =
      protocol::Audits::InspectorIssue::create()
          .setCode(
              protocol::Audits::InspectorIssueCodeEnum::SharedArrayBufferIssue)
          .setDetails(std::move(issue_details))
          .build();
  execution_context->AddInspectorIssue(AuditsIssue(std::move(issue)));
}

// static
void AuditsIssue::ReportDeprecationIssue(ExecutionContext* execution_context,
                                         String type) {
  auto source_location = CaptureSourceLocation(execution_context);
  auto deprecation_issue_details =
      protocol::Audits::DeprecationIssueDetails::create()
          .setSourceCodeLocation(CreateProtocolLocation(*source_location))
          .setType(type)
          .build();
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    auto affected_frame =
        protocol::Audits::AffectedFrame::create()
            .setFrameId(IdentifiersFactory::FrameId(window->GetFrame()))
            .build();
    deprecation_issue_details->setAffectedFrame(std::move(affected_frame));
  }
  auto issue_details =
      protocol::Audits::InspectorIssueDetails::create()
          .setDeprecationIssueDetails(std::move(deprecation_issue_details))
          .build();
  auto issue =
      protocol::Audits::InspectorIssue::create()
          .setCode(protocol::Audits::InspectorIssueCodeEnum::DeprecationIssue)
          .setDetails(std::move(issue_details))
          .build();
  execution_context->AddInspectorIssue(AuditsIssue(std::move(issue)));
}

namespace {

protocol::Audits::ClientHintIssueReason ClientHintIssueReasonToProtocol(
    ClientHintIssueReason reason) {
  switch (reason) {
    case ClientHintIssueReason::kMetaTagAllowListInvalidOrigin:
      return protocol::Audits::ClientHintIssueReasonEnum::
          MetaTagAllowListInvalidOrigin;
    case ClientHintIssueReason::kMetaTagModifiedHTML:
      return protocol::Audits::ClientHintIssueReasonEnum::MetaTagModifiedHTML;
  }
}

}  // namespace

// static
void AuditsIssue::ReportClientHintIssue(LocalDOMWindow* local_dom_window,
                                        ClientHintIssueReason reason) {
  auto source_location = CaptureSourceLocation(local_dom_window);
  auto client_hint_issue_details =
      protocol::Audits::ClientHintIssueDetails::create()
          .setSourceCodeLocation(CreateProtocolLocation(*source_location))
          .setClientHintIssueReason(ClientHintIssueReasonToProtocol(reason))
          .build();
  auto issue_details =
      protocol::Audits::InspectorIssueDetails::create()
          .setClientHintIssueDetails(std::move(client_hint_issue_details))
          .build();
  auto issue =
      protocol::Audits::InspectorIssue::create()
          .setCode(protocol::Audits::InspectorIssueCodeEnum::ClientHintIssue)
          .setDetails(std::move(issue_details))
          .build();
  local_dom_window->AddInspectorIssue(AuditsIssue(std::move(issue)));
}

AuditsIssue AuditsIssue::CreateBlockedByResponseIssue(
    network::mojom::BlockedByResponseReason reason,
    uint64_t identifier,
    DocumentLoader* loader,
    const ResourceError& error,
    const base::UnguessableToken& token) {
  auto affected_request =
      protocol::Audits::AffectedRequest::create()
          .setRequestId(IdentifiersFactory::RequestId(loader, identifier))
          .setUrl(error.FailingURL())
          .build();

  auto affected_frame = protocol::Audits::AffectedFrame::create()
                            .setFrameId(IdentifiersFactory::IdFromToken(token))
                            .build();

  auto blocked_by_response_details =
      protocol::Audits::BlockedByResponseIssueDetails::create()
          .setReason(BlockedByResponseReasonToProtocol(reason))
          .setRequest(std::move(affected_request))
          .setParentFrame(std::move(affected_frame))
          .build();

  auto details = protocol::Audits::InspectorIssueDetails::create()
                     .setBlockedByResponseIssueDetails(
                         std::move(blocked_by_response_details))
                     .build();

  auto issue =
      protocol::Audits::InspectorIssue::create()
          .setCode(
              protocol::Audits::InspectorIssueCodeEnum::BlockedByResponseIssue)
          .setDetails(std::move(details))
          .build();

  return AuditsIssue(std::move(issue));
}

void AuditsIssue::ReportMixedContentIssue(
    const KURL& main_resource_url,
    const KURL& insecure_url,
    const mojom::blink::RequestContextType request_context,
    LocalFrame* frame,
    const MixedContentResolutionStatus resolution_status,
    const String& devtools_id) {
  auto affected_frame =
      protocol::Audits::AffectedFrame::create()
          .setFrameId(frame->GetDevToolsFrameToken().ToString().c_str())
          .build();

  auto mixedContentDetails =
      protocol::Audits::MixedContentIssueDetails::create()
          .setResourceType(
              RequestContextToMixedContentResourceType(request_context))
          .setResolutionStatus(
              MixedContentResolutionStatusToProtocol(resolution_status))
          .setInsecureURL(insecure_url.GetString())
          .setMainResourceURL(main_resource_url.GetString())
          .setFrame(std::move(affected_frame))
          .build();

  if (!devtools_id.IsNull()) {
    auto request = protocol::Audits::AffectedRequest::create()
                       .setRequestId(devtools_id)
                       .setUrl(insecure_url.GetString())
                       .build();
    mixedContentDetails->setRequest(std::move(request));
  }

  auto details =
      protocol::Audits::InspectorIssueDetails::create()
          .setMixedContentIssueDetails(std::move(mixedContentDetails))
          .build();
  auto issue =
      protocol::Audits::InspectorIssue::create()
          .setCode(protocol::Audits::InspectorIssueCodeEnum::MixedContentIssue)
          .setDetails(std::move(details))
          .build();

  frame->DomWindow()->AddInspectorIssue(AuditsIssue(std::move(issue)));
}

void AuditsIssue::ReportGenericIssue(
    LocalFrame* frame,
    mojom::blink::GenericIssueErrorType error_type,
    int violating_node_id) {
  auto audits_issue_details =
      protocol::Audits::GenericIssueDetails::create()
          .setErrorType(GenericIssueErrorTypeToProtocol(error_type))
          .setViolatingNodeId(violating_node_id)
          .build();

  auto issue =
      protocol::Audits::InspectorIssue::create()
          .setCode(protocol::Audits::InspectorIssueCodeEnum::GenericIssue)
          .setDetails(
              protocol::Audits::InspectorIssueDetails::create()
                  .setGenericIssueDetails(std::move(audits_issue_details))
                  .build())
          .build();

  frame->DomWindow()->AddInspectorIssue(AuditsIssue(std::move(issue)));
}

void AuditsIssue::ReportGenericIssue(
    LocalFrame* frame,
    mojom::blink::GenericIssueErrorType error_type,
    int violating_node_id,
    const String& violating_node_attribute) {
  auto audits_issue_details =
      protocol::Audits::GenericIssueDetails::create()
          .setErrorType(GenericIssueErrorTypeToProtocol(error_type))
          .setViolatingNodeId(violating_node_id)
          .setViolatingNodeAttribute(violating_node_attribute)
          .build();

  auto issue =
      protocol::Audits::InspectorIssue::create()
          .setCode(protocol::Audits::InspectorIssueCodeEnum::GenericIssue)
          .setDetails(
              protocol::Audits::InspectorIssueDetails::create()
                  .setGenericIssueDetails(std::move(audits_issue_details))
                  .build())
          .build();

  frame->DomWindow()->AddInspectorIssue(AuditsIssue(std::move(issue)));
}

void AuditsIssue::ReportPropertyRuleIssue(
    Document* document,
    const KURL& url,
    WTF::OrdinalNumber line,
    WTF::OrdinalNumber column,
    protocol::Audits::PropertyRuleIssueReason reason,
    const String& propertyValue) {
  if (!document || !document->GetExecutionContext()) {
    return;
  }
  auto sourceCodeLocation = protocol::Audits::SourceCodeLocation::create()
                                .setUrl(url)
                                .setLineNumber(line.ZeroBasedInt())
                                .setColumnNumber(column.OneBasedInt())
                                .build();

  auto details = protocol::Audits::PropertyRuleIssueDetails::create()
                     .setSourceCodeLocation(std::move(sourceCodeLocation))
                     .setPropertyRuleIssueReason(reason)
                     .build();

  if (!propertyValue.IsNull()) {
    details->setPropertyValue(propertyValue);
  }

  auto issue =
      protocol::Audits::InspectorIssue::create()
          .setCode(protocol::Audits::InspectorIssueCodeEnum::PropertyRuleIssue)
          .setDetails(protocol::Audits::InspectorIssueDetails::create()
                          .setPropertyRuleIssueDetails(std::move(details))
                          .build())
          .build();

  document->GetExecutionContext()->AddInspectorIssue(
      AuditsIssue(std::move(issue)));
}

void AuditsIssue::ReportStylesheetLoadingLateImportIssue(
    Document* document,
    const KURL& url,
    WTF::OrdinalNumber line,
    WTF::OrdinalNumber column) {
  if (!document || !document->GetExecutionContext()) {
    return;
  }
  auto sourceCodeLocation = protocol::Audits::SourceCodeLocation::create()
                                .setUrl(url)
                                .setLineNumber(line.ZeroBasedInt())
                                .setColumnNumber(column.OneBasedInt())
                                .build();
  auto details = protocol::Audits::StylesheetLoadingIssueDetails::create()
                     .setSourceCodeLocation(std::move(sourceCodeLocation))
                     .setStyleSheetLoadingIssueReason(
                         protocol::Audits::StyleSheetLoadingIssueReasonEnum::
                             LateImportRule)
                     .build();

  auto issue =
      protocol::Audits::InspectorIssue::create()
          .setCode(
              protocol::Audits::InspectorIssueCodeEnum::StylesheetLoadingIssue)
          .setDetails(protocol::Audits::InspectorIssueDetails::create()
                          .setStylesheetLoadingIssueDetails(std::move(details))
                          .build())
          .build();

  document->GetExecutionContext()->AddInspectorIssue(
      AuditsIssue(std::move(issue)));
}

void AuditsIssue::ReportStylesheetLoadingRequestFailedIssue(
    Document* document,
    const KURL& url,
    const String& request_id,
    const KURL& initiator_url,
    WTF::OrdinalNumber initiator_line,
    WTF::OrdinalNumber initiator_column,
    const String& failureMessage) {
  if (!document || !document->GetExecutionContext()) {
    return;
  }
  auto sourceCodeLocation = protocol::Audits::SourceCodeLocation::create()
                                .setUrl(initiator_url)
                                .setLineNumber(initiator_line.ZeroBasedInt())
                                .setColumnNumber(initiator_column.OneBasedInt())
                                .build();
  auto requestDetails = protocol::Audits::FailedRequestInfo::create()
                            .setUrl(url)
                            .setFailureMessage(failureMessage)
                            .build();

  if (!request_id.IsNull()) {
    requestDetails->setRequestId(request_id);
  }
  auto details =
      protocol::Audits::StylesheetLoadingIssueDetails::create()
          .setSourceCodeLocation(std::move(sourceCodeLocation))
          .setFailedRequestInfo(std::move(requestDetails))
          .setStyleSheetLoadingIssueReason(
              protocol::Audits::StyleSheetLoadingIssueReasonEnum::RequestFailed)
          .build();

  auto issue =
      protocol::Audits::InspectorIssue::create()
          .setCode(
              protocol::Audits::InspectorIssueCodeEnum::StylesheetLoadingIssue)
          .setDetails(protocol::Audits::InspectorIssueDetails::create()
                          .setStylesheetLoadingIssueDetails(std::move(details))
                          .build())
          .build();

  document->GetExecutionContext()->AddInspectorIssue(
      AuditsIssue(std::move(issue)));
}

AuditsIssue AuditsIssue::CreateContentSecurityPolicyIssue(
    const blink::SecurityPolicyViolationEventInit& violation_data,
    bool is_report_only,
    ContentSecurityPolicyViolationType violation_type,
    LocalFrame* frame_ancestor,
    Element* element,
    SourceLocation* source_location,
    std::optional<base::UnguessableToken> issue_id) {
  std::unique_ptr<protocol::Audits::ContentSecurityPolicyIssueDetails>
      cspDetails = protocol::Audits::ContentSecurityPolicyIssueDetails::create()
                       .setIsReportOnly(is_report_only)
                       .setViolatedDirective(violation_data.violatedDirective())
                       .setContentSecurityPolicyViolationType(
                           CSPViolationTypeToProtocol(violation_type))
                       .build();
  if (violation_type == ContentSecurityPolicyViolationType::kURLViolation ||
      violation_data.violatedDirective() == "frame-ancestors") {
    cspDetails->setBlockedURL(violation_data.blockedURI());
  }

  if (frame_ancestor) {
    std::unique_ptr<protocol::Audits::AffectedFrame> affected_frame =
        protocol::Audits::AffectedFrame::create()
            .setFrameId(
                frame_ancestor->GetDevToolsFrameToken().ToString().c_str())
            .build();
    cspDetails->setFrameAncestor(std::move(affected_frame));
  }

  if (source_location) {
    cspDetails->setSourceCodeLocation(CreateProtocolLocation(*source_location));
  }

  if (element) {
    cspDetails->setViolatingNodeId(element->GetDomNodeId());
  }

  std::unique_ptr<protocol::Audits::InspectorIssueDetails> details =
      protocol::Audits::InspectorIssueDetails::create()
          .setContentSecurityPolicyIssueDetails(std::move(cspDetails))
          .build();

  std::unique_ptr<protocol::Audits::InspectorIssue> issue =
      protocol::Audits::InspectorIssue::create()
          .setCode(protocol::Audits::InspectorIssueCodeEnum::
                       ContentSecurityPolicyIssue)
          .setDetails(std::move(details))
          .build();

  if (issue_id) {
    issue->setIssueId(IdentifiersFactory::IdFromToken(*issue_id));
  }

  return AuditsIssue(std::move(issue));
}

}  // namespace blink
