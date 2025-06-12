// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/integrity_policy.h"

#include "base/containers/contains.h"
#include "services/network/public/cpp/integrity_policy.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/mojom/integrity_algorithm.mojom-blink.h"
#include "services/network/public/mojom/integrity_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/integrity_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

Vector<String> ConvertToBlink(const std::vector<std::string>& in) {
  Vector<String> out;
  for (const std::string& str : in) {
    out.push_back(String::FromUTF8(str));
  }
  return out;
}

void SendReport(ExecutionContext* context,
                Vector<String> report_endpoints,
                const KURL& blocked_url,
                const String& destination,
                bool report_only) {
  const SecurityOrigin* origin = context->GetSecurityOrigin();
  const KURL& document_url = context->Url();
  String safe_document_url = ContentSecurityPolicy::StripURLForUseInReport(
      origin, document_url, CSPDirectiveName::Unknown);
  String safe_blocked_url = ContentSecurityPolicy::StripURLForUseInReport(
      origin, blocked_url, CSPDirectiveName::Unknown);

  auto* body = MakeGarbageCollected<IntegrityViolationReportBody>(
      safe_document_url, safe_blocked_url, destination, report_only);
  Report* observed_report = MakeGarbageCollected<Report>(
      ReportType::kIntegrityViolation, safe_document_url, body);
  ReportingContext::From(context)->QueueReport(observed_report,
                                               std::move(report_endpoints));
}

bool ShouldBlockOrReport(const network::IntegrityPolicy& integrity_policy) {
  return (base::Contains(
              integrity_policy.blocked_destinations,
              ::network::mojom::blink::IntegrityPolicy::Destination::kScript) &&
          base::Contains(
              integrity_policy.sources,
              ::network::mojom::blink::IntegrityPolicy::Source::kInline));
}

}  // namespace

// https://w3c.github.io/webappsec-subresource-integrity/#should-request-be-blocked-by-integrity-policy-section
// static
bool IntegrityPolicy::AllowRequest(
    ExecutionContext* context,
    const DOMWrapperWorld* world,
    network::mojom::RequestDestination request_destination,
    network::mojom::RequestMode request_mode,
    const IntegrityMetadataSet& integrity_metadata,
    const KURL& url) {
  if (!context) {
    return true;
  }
  if (world && world->IsIsolatedWorld()) {
    return true;
  }

  // 3. If parsedMetadata is not the empty set and request’s mode is either
  // "cors" or "same-origin", return "Allowed".
  // 4. If request’s url is local, return "Allowed".
  if ((!integrity_metadata.empty() &&
       request_mode != network::mojom::RequestMode::kNoCors) ||
      url.ProtocolIsData() || url.ProtocolIs("blob") || url.ProtocolIsAbout()) {
    return true;
  }
  PolicyContainer* policy_container = context->GetPolicyContainer();
  if (!policy_container) {
    return true;
  }
  const network::IntegrityPolicy& integrity_policy =
      policy_container->GetPolicies().integrity_policy;
  const network::IntegrityPolicy& integrity_policy_report_only =
      policy_container->GetPolicies().integrity_policy_report_only;

  bool allow = true;
  if (request_destination == network::mojom::RequestDestination::kScript) {
    if (ShouldBlockOrReport(integrity_policy)) {
      allow = false;
      SendReport(
          context, ConvertToBlink(integrity_policy.endpoints), url,
          String(network::RequestDestinationToString(request_destination)),
          /*report_only=*/false);
    }
    if (ShouldBlockOrReport(integrity_policy_report_only)) {
      SendReport(
          context, ConvertToBlink(integrity_policy_report_only.endpoints), url,
          String(network::RequestDestinationToString(request_destination)),
          /*report_only=*/true);
    }
  }
  return allow;
}

void IntegrityPolicy::LogParsingErrorsIfAny(
    ExecutionContext* context,
    const network::IntegrityPolicy& policy) {
  for (const std::string& error : policy.parsing_errors) {
    context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity,
        mojom::blink::ConsoleMessageLevel::kError, String(error)));
  }
}

}  // namespace blink
