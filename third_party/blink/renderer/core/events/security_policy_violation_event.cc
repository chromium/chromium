/*
 * Copyright (C) 2016 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/events/security_policy_violation_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_security_policy_violation_event_init.h"
#include "third_party/blink/renderer/core/securitypolicyviolation_disposition_names.h"

namespace blink {

SecurityPolicyViolationEvent::SecurityPolicyViolationEvent(
    const AtomicString& type)
    : Event(type, Bubbles::kYes, Cancelable::kNo, ComposedMode::kComposed) {}

SecurityPolicyViolationEvent::SecurityPolicyViolationEvent(
    const AtomicString& type,
    const SecurityPolicyViolationEventInit* initializer)
    : SecurityPolicyViolationEvent(type) {
  if (initializer->hasDocumentURI())
    document_uri_ = initializer->documentURI();
  if (initializer->hasReferrer())
    referrer_ = initializer->referrer();
  if (initializer->hasBlockedURI())
    blocked_uri_ = initializer->blockedURI();
  if (initializer->hasViolatedDirective())
    violated_directive_ = initializer->violatedDirective();
  if (initializer->hasEffectiveDirective())
    effective_directive_ = initializer->effectiveDirective();
  if (initializer->hasOriginalPolicy())
    original_policy_ = initializer->originalPolicy();
  disposition_ = initializer->disposition() ==
                         securitypolicyviolation_disposition_names::kReport
                     ? network::mojom::ContentSecurityPolicyType::kReport
                     : network::mojom::ContentSecurityPolicyType::kEnforce;
  if (initializer->hasSourceFile())
    source_file_ = initializer->sourceFile();
  if (initializer->hasLineNumber())
    line_number_ = initializer->lineNumber();
  if (initializer->hasColumnNumber())
    column_number_ = initializer->columnNumber();
  if (initializer->hasStatusCode())
    status_code_ = initializer->statusCode();
  if (initializer->hasSample())
    sample_ = initializer->sample();
}

const String& SecurityPolicyViolationEvent::disposition() const {
  return disposition_ == network::mojom::ContentSecurityPolicyType::kReport
             ? securitypolicyviolation_disposition_names::kReport
             : securitypolicyviolation_disposition_names::kEnforce;
}

}  // namespace blink
