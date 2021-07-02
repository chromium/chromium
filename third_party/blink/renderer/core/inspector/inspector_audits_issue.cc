// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"

#include "base/unguessable_token.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/protocol/Audits.h"
#include "third_party/blink/renderer/core/inspector/protocol/Network.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

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

std::unique_ptr<protocol::Audits::SourceCodeLocation> CreateProtocolLocation(
    const SourceLocation& location) {
  auto protocol_location = protocol::Audits::SourceCodeLocation::create()
                               .setUrl(location.Url())
                               .setLineNumber(location.LineNumber() - 1)
                               .setColumnNumber(location.ColumnNumber())
                               .build();
  protocol_location->setScriptId(WTF::String::Number(location.ScriptId()));
  return protocol_location;
}

void AuditsIssue::ReportCorsIssue(
    ExecutionContext* execution_context,
    int64_t identifier,
    RendererCorsIssueCode code,
    String url,
    String initiator_origin,
    String failedParameter,
    absl::optional<base::UnguessableToken> issue_id) {
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
  auto location = SourceLocation::Capture(execution_context);
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

protocol::Audits::AttributionReportingIssueType
BuildAttributionReportingIssueType(AttributionReportingIssueType type) {
  switch (type) {
    case AttributionReportingIssueType::kPermissionPolicyDisabled:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          PermissionPolicyDisabled;
    case AttributionReportingIssueType::kInvalidAttributionSourceEventId:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          InvalidAttributionSourceEventId;
    case AttributionReportingIssueType::kAttributionSourceUntrustworthyOrigin:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          AttributionSourceUntrustworthyOrigin;
    case AttributionReportingIssueType::kInvalidAttributionData:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          InvalidAttributionData;
    case AttributionReportingIssueType::kAttributionUntrustworthyOrigin:
      return protocol::Audits::AttributionReportingIssueTypeEnum::
          AttributionUntrustworthyOrigin;
  }
}

}  // namespace

void AuditsIssue::ReportAttributionIssue(
    ExecutionContext* reporting_execution_context,
    AttributionReportingIssueType type,
    const absl::optional<base::UnguessableToken>& offending_frame_token,
    Element* element,
    const absl::optional<String>& request_id,
    const absl::optional<String>& invalid_parameter) {
  auto details = protocol::Audits::AttributionReportingIssueDetails::create()
                     .setViolationType(BuildAttributionReportingIssueType(type))
                     .build();

  if (offending_frame_token) {
    details->setFrame(
        protocol::Audits::AffectedFrame::create()
            .setFrameId(IdentifiersFactory::IdFromToken(*offending_frame_token))
            .build());
  }
  if (element) {
    details->setViolatingNodeId(DOMNodeIds::IdForNode(element));
  }
  if (request_id) {
    details->setRequest(protocol::Audits::AffectedRequest::create()
                            .setRequestId(*request_id)
                            .build());
  }
  if (invalid_parameter) {
    details->setInvalidParameter(*invalid_parameter);
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
  reporting_execution_context->AddInspectorIssue(AuditsIssue(std::move(issue)));
}

void AuditsIssue::ReportNavigatorUserAgentAccess(
    ExecutionContext* execution_context,
    String url) {
  auto navigator_user_agent_details =
      protocol::Audits::NavigatorUserAgentIssueDetails::create()
          .setUrl(url)
          .build();

  // Try to get only the script name quickly.
  String script_url = GetCurrentScriptUrl(5);
  if (script_url.IsEmpty())
    script_url = GetCurrentScriptUrl(200);

  std::unique_ptr<SourceLocation> location;
  if (!script_url.IsEmpty())
    location = std::make_unique<SourceLocation>(script_url, 1, 0, nullptr);
  else
    location = SourceLocation::Capture(execution_context);

  if (location) {
    navigator_user_agent_details->setLocation(
        CreateProtocolLocation(*location));
  }

  auto details = protocol::Audits::InspectorIssueDetails::create()
                     .setNavigatorUserAgentIssueDetails(
                         std::move(navigator_user_agent_details))
                     .build();
  auto issue =
      protocol::Audits::InspectorIssue::create()
          .setCode(
              protocol::Audits::InspectorIssueCodeEnum::NavigatorUserAgentIssue)
          .setDetails(std::move(details))
          .build();
  execution_context->AddInspectorIssue(AuditsIssue(std::move(issue)));
}

void AuditsIssue::ReportCrossOriginWasmModuleSharingIssue(
    ExecutionContext* execution_context,
    const std::string& wasm_source_url,
    WTF::String source_origin,
    WTF::String target_origin,
    bool is_warning) {
  auto details =
      protocol::Audits::WasmCrossOriginModuleSharingIssueDetails::create()
          .setWasmModuleUrl(WTF::String::FromUTF8(wasm_source_url))
          .setSourceOrigin(source_origin)
          .setTargetOrigin(target_origin)
          .setIsWarning(is_warning)
          .build();

  auto issue_details =
      protocol::Audits::InspectorIssueDetails::create()
          .setWasmCrossOriginModuleSharingIssue(std::move(details))
          .build();
  auto issue = protocol::Audits::InspectorIssue::create()
                   .setCode(protocol::Audits::InspectorIssueCodeEnum::
                                WasmCrossOriginModuleSharingIssue)
                   .setDetails(std::move(issue_details))
                   .build();
  execution_context->AddInspectorIssue(AuditsIssue(std::move(issue)));
}
}  // namespace blink
