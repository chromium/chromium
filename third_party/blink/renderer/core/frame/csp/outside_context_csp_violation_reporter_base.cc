// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/outside_context_csp_violation_reporter_base.h"

#include "mojo/public/cpp/bindings/message.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "services/network/public/mojom/source_location.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"

namespace blink {

void OutsideContextCSPViolationReporterBase::ReportCSPViolation(
    network::mojom::blink::CSPViolationPtr violation) {
  CSPDirectiveName effective_type =
      ContentSecurityPolicy::GetDirectiveType(violation->effective_directive);
  if (!IsCSPEffectiveDirectiveAllowedForViolationReporting(effective_type)) {
    mojo::ReportBadMessage("Unexpected CSP violation effective directive");
    return;
  }

  if (auto* policy = GetContentSecurityPolicyForViolationReporting()) {
    policy->LogToConsoleAndReportViolation(
        effective_type, violation->console_message, violation->blocked_url,
        violation->report_endpoints, violation->use_reporting_api,
        violation->header, violation->type,
        ContentSecurityPolicyViolationType::kURLViolation,
        MakeGarbageCollected<SourceLocation>(
            violation->source_location->url, String(),
            violation->source_location->line,
            violation->source_location->column, nullptr),
        GetContextFrameForCSPViolationReporting(effective_type));
  }
}

}  // namespace blink
