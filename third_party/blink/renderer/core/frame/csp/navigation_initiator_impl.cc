// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/navigation_initiator_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"

namespace blink {

NavigationInitiatorImpl::NavigationInitiatorImpl(Document& document)
    : document_(document) {}

void NavigationInitiatorImpl::Trace(Visitor* visitor) {
  visitor->Trace(document_);
}

void NavigationInitiatorImpl::SendViolationReport(
    mojom::blink::CSPViolationParamsPtr violation_params) {
  std::unique_ptr<SourceLocation> source_location =
      std::make_unique<SourceLocation>(
          violation_params->source_location->url,
          violation_params->source_location->line_number,
          violation_params->source_location->column_number, nullptr);

  Vector<String> report_endpoints;
  for (const String& end_point : violation_params->report_endpoints)
    report_endpoints.push_back(end_point);

  document_->AddConsoleMessage(ConsoleMessage::Create(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError, violation_params->console_message));
  document_->GetContentSecurityPolicy()->ReportViolation(
      violation_params->directive,
      ContentSecurityPolicy::GetDirectiveType(
          violation_params->effective_directive),
      violation_params->console_message, KURL(violation_params->blocked_url),
      report_endpoints, violation_params->use_reporting_api,
      violation_params->header,
      static_cast<ContentSecurityPolicyHeaderType>(
          violation_params->disposition),
      ContentSecurityPolicy::ViolationType::kURLViolation,
      std::move(source_location), nullptr /* LocalFrame */,
      violation_params->after_redirect ? RedirectStatus::kFollowedRedirect
                                       : RedirectStatus::kNoRedirect,
      nullptr /* Element */);
}

void NavigationInitiatorImpl::Dispose() {
  navigation_initiator_receivers_.Clear();
}

}  // namespace blink
