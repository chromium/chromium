// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/cross_thread_security_policy_violation_event_init.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_security_policy_violation_event_disposition.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_security_policy_violation_event_init.h"

namespace blink {

// static
CrossThreadSecurityPolicyViolationEventInit
CrossThreadSecurityPolicyViolationEventInit::From(
    const SecurityPolicyViolationEventInit& init) {
  CrossThreadSecurityPolicyViolationEventInit cross_thread_init;
  cross_thread_init.blocked_uri_ = init.blockedURI();
  cross_thread_init.column_number_ = init.columnNumber();
  switch (init.disposition().AsEnum()) {
    case V8SecurityPolicyViolationEventDisposition::Enum::kEnforce:
      cross_thread_init.disposition_ = Disposition::kEnforce;
      break;
    case V8SecurityPolicyViolationEventDisposition::Enum::kReport:
      cross_thread_init.disposition_ = Disposition::kReport;
      break;
  }
  cross_thread_init.document_uri_ = init.documentURI();
  cross_thread_init.effective_directive_ = init.effectiveDirective();
  if (init.hasEvalHash()) {
    cross_thread_init.eval_hash_ = init.evalHash();
    cross_thread_init.has_eval_hash_ = true;
  }
  cross_thread_init.line_number_ = init.lineNumber();
  cross_thread_init.original_policy_ = init.originalPolicy();
  cross_thread_init.referrer_ = init.referrer();
  cross_thread_init.sample_ = init.sample();
  cross_thread_init.source_file_ = init.sourceFile();
  cross_thread_init.status_code_ = init.statusCode();
  if (init.hasUrlHash()) {
    cross_thread_init.url_hash_ = init.urlHash();
    cross_thread_init.has_url_hash_ = true;
  }
  cross_thread_init.violated_directive_ = init.violatedDirective();
  return cross_thread_init;
}

SecurityPolicyViolationEventInit*
CrossThreadSecurityPolicyViolationEventInit::ToEventInit() const {
  auto* init = SecurityPolicyViolationEventInit::Create();
  init->setBlockedURI(blocked_uri_);
  init->setColumnNumber(column_number_);
  switch (disposition_) {
    case kEnforce:
      init->setDisposition(
          V8SecurityPolicyViolationEventDisposition::Enum::kEnforce);
      break;
    case kReport:
      init->setDisposition(
          V8SecurityPolicyViolationEventDisposition::Enum::kReport);
      break;
  }
  init->setDocumentURI(document_uri_);
  init->setEffectiveDirective(effective_directive_);
  if (has_eval_hash_) {
    init->setEvalHash(eval_hash_);
  }
  init->setLineNumber(line_number_);
  init->setOriginalPolicy(original_policy_);
  init->setReferrer(referrer_);
  init->setSample(sample_);
  init->setSourceFile(source_file_);
  init->setStatusCode(status_code_);
  if (has_url_hash_) {
    init->setUrlHash(url_hash_);
  }
  init->setViolatedDirective(violated_directive_);
  return init;
}

}  // namespace blink
