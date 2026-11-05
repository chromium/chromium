// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_OUTSIDE_CONTEXT_CSP_VIOLATION_REPORTER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_OUTSIDE_CONTEXT_CSP_VIOLATION_REPORTER_BASE_H_

#include "services/network/public/mojom/content_security_policy.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/security_context/outside_context_csp_violation_reporter.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class ContentSecurityPolicy;
class LocalFrame;

// Reports the violations received through
// mojom::blink::OutsideContextCSPViolationReporter on the context. The
// subclasses provide what differs between the senders.
class CORE_EXPORT OutsideContextCSPViolationReporterBase
    : public mojom::blink::OutsideContextCSPViolationReporter {
 public:
  // mojom::blink::OutsideContextCSPViolationReporter:
  void ReportCSPViolation(
      network::mojom::blink::CSPViolationPtr violation) final;

 protected:
  // Returns true if the sender is allowed to report a violation of
  // `effective_type`. Otherwise, ReportCSPViolation() reports a bad message.
  virtual bool IsCSPEffectiveDirectiveAllowedForViolationReporting(
      network::mojom::blink::CSPDirectiveName effective_type) const = 0;

  // Returns the policy of the context to report on, or null if the violation
  // cannot be reported (e.g. the context is destroyed).
  virtual ContentSecurityPolicy*
  GetContentSecurityPolicyForViolationReporting() = 0;

  // Returns the frame to pass as the context frame of the violation, or null to
  // report on the context of the policy. See
  // ContentSecurityPolicy::LogToConsoleAndReportViolation().
  virtual LocalFrame* GetContextFrameForCSPViolationReporting(
      network::mojom::blink::CSPDirectiveName effective_type) {
    return nullptr;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_OUTSIDE_CONTEXT_CSP_VIOLATION_REPORTER_BASE_H_
