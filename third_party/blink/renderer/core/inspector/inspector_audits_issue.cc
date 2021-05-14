// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/protocol/Audits.h"
#include "third_party/blink/renderer/core/inspector/protocol/Network.h"

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

void AuditsIssue::ReportCorsIssue(ExecutionContext* execution_context,
                                  int64_t identifier,
                                  RendererCorsIssueCode code,
                                  String url,
                                  String initiator_origin,
                                  String failedParameter) {
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
  execution_context->AddInspectorIssue(AuditsIssue(std::move(issue)));
}

}  // namespace blink
