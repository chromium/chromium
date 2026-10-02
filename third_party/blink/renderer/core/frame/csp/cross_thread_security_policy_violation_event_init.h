// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CROSS_THREAD_SECURITY_POLICY_VIOLATION_EVENT_INIT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CROSS_THREAD_SECURITY_POLICY_VIOLATION_EVENT_INIT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class SecurityPolicyViolationEventInit;

// Repackage the garbage-collected SecurityPolicyViolationEventInit to move
// across threads. Used to request dispatching the CSP violation event on the
// appropriate target (https://w3c.github.io/webappsec-csp/#report-violation)
// from the WorkerOrWorkletGlobalScope.
class CORE_EXPORT CrossThreadSecurityPolicyViolationEventInit {
 public:
  static CrossThreadSecurityPolicyViolationEventInit From(
      const SecurityPolicyViolationEventInit&);
  SecurityPolicyViolationEventInit* ToEventInit() const;

 private:
  CrossThreadSecurityPolicyViolationEventInit() = default;

  bool has_eval_hash_ = false;
  bool has_url_hash_ = false;

  String blocked_uri_;
  int column_number_ = 0;
  enum Disposition { kEnforce, kReport } disposition_ = kEnforce;
  String document_uri_;
  String effective_directive_;
  String eval_hash_;
  int line_number_ = 0;
  String original_policy_;
  String referrer_;
  String sample_;
  String source_file_;
  uint16_t status_code_ = 0;
  String url_hash_;
  String violated_directive_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CROSS_THREAD_SECURITY_POLICY_VIOLATION_EVENT_INIT_H_
