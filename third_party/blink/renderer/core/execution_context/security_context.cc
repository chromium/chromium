/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/execution_context/security_context.h"

#include "base/metrics/histogram_macros.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "third_party/blink/public/common/permissions_policy/document_policy.h"
#include "third_party/blink/public/common/permissions_policy/document_policy_features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_value.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// static
WTF::Vector<unsigned> SecurityContext::SerializeInsecureNavigationSet(
    const InsecureNavigationsSet& set) {
  // The set is serialized as a sorted array. Sorting it makes it easy to know
  // if two serialized sets are equal.
  WTF::Vector<unsigned> serialized;
  serialized.reserve(set.size());
  for (unsigned host : set)
    serialized.emplace_back(host);
  std::sort(serialized.begin(), serialized.end());

  return serialized;
}

SecurityContext::SecurityContext(ExecutionContext* execution_context)
    : execution_context_(execution_context),
      insecure_request_policy_(
          mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone) {}

SecurityContext::~SecurityContext() = default;

void SecurityContext::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
}

void SecurityContext::SetSecurityOrigin(
    scoped_refptr<SecurityOrigin> security_origin) {
  // Enforce that we don't change access, we might change the reference (via
  // IsolatedCopy but we can't change the security policy).
  CHECK(security_origin);
  // The purpose of this check is to ensure that the SecurityContext does not
  // change after script has executed in the ExecutionContext. If this is a
  // RemoteSecurityContext, then there is no local script execution and the
  // context is permitted to represent multiple origins over its lifetime, so it
  // is safe for the SecurityOrigin to change.
  // NOTE: A worker may need to make its origin opaque after the main worker
  // script is loaded if the worker is origin-sandboxed. Specifically exempt
  // that transition. See https://crbug.com/1068008. It would be great if we
  // could get rid of this exemption.
  bool is_worker_transition_to_opaque =
      execution_context_ &&
      execution_context_->IsWorkerOrWorkletGlobalScope() &&
      IsSandboxed(network::mojom::blink::WebSandboxFlags::kOrigin) &&
      security_origin->IsOpaque() &&
      security_origin->GetOriginOrPrecursorOriginIfOpaque() == security_origin_;
  CHECK(!execution_context_ || !security_origin_ ||
        security_origin_->CanAccess(security_origin.get()) ||
        is_worker_transition_to_opaque);
  security_origin_ = std::move(security_origin);

  if (!security_origin_->IsPotentiallyTrustworthy() &&
      !is_worker_loaded_from_data_url_) {
    secure_context_mode_ = SecureContextMode::kInsecureContext;
    secure_context_explanation_ = SecureContextModeExplanation::kInsecureScheme;
  } else if (SchemeRegistry::SchemeShouldBypassSecureContextCheck(
                 security_origin_->Protocol())) {
    // data: URL has opaque origin so security_origin's protocol will be empty
    // and should never be bypassed.
    CHECK(!is_worker_loaded_from_data_url_);
    secure_context_mode_ = SecureContextMode::kSecureContext;
    secure_context_explanation_ = SecureContextModeExplanation::kSecure;
  } else if (execution_context_) {
    if (execution_context_->HasInsecureContextInAncestors()) {
      secure_context_mode_ = SecureContextMode::kInsecureContext;
      secure_context_explanation_ =
          SecureContextModeExplanation::kInsecureAncestor;
    } else {
      secure_context_mode_ = SecureContextMode::kSecureContext;
      secure_context_explanation_ =
          security_origin_->IsLocalhost()
              ? SecureContextModeExplanation::kSecureLocalhost
              : SecureContextModeExplanation::kSecure;
    }
  }

  bool is_secure = secure_context_mode_ == SecureContextMode::kSecureContext;
  if (sandbox_flags_ != network::mojom::blink::WebSandboxFlags::kNone) {
    UseCounter::Count(
        execution_context_,
        is_secure ? WebFeature::kSecureContextCheckForSandboxedOriginPassed
                  : WebFeature::kSecureContextCheckForSandboxedOriginFailed);
  }

  UseCounter::Count(execution_context_,
                    is_secure ? WebFeature::kSecureContextCheckPassed
                              : WebFeature::kSecureContextCheckFailed);
}

void SecurityContext::SetSecurityOriginForTesting(
    scoped_refptr<SecurityOrigin> security_origin) {
  security_origin_ = std::move(security_origin);
}

bool SecurityContext::IsSandboxed(
    network::mojom::blink::WebSandboxFlags mask) const {
  return (sandbox_flags_ & mask) !=
         network::mojom::blink::WebSandboxFlags::kNone;
}

void SecurityContext::SetSandboxFlags(
    network::mojom::blink::WebSandboxFlags flags) {
  sandbox_flags_ = flags;
}

void SecurityContext::SetPermissionsPolicy(
    std::unique_ptr<PermissionsPolicy> permissions_policy) {
  permissions_policy_ = std::move(permissions_policy);
}

void SecurityContext::SetReportOnlyPermissionsPolicy(
    std::unique_ptr<PermissionsPolicy> permissions_policy) {
  report_only_permissions_policy_ = std::move(permissions_policy);
}

void SecurityContext::SetDocumentPolicy(
    std::unique_ptr<DocumentPolicy> policy) {
  document_policy_ = std::move(policy);
}

void SecurityContext::SetReportOnlyDocumentPolicy(
    std::unique_ptr<DocumentPolicy> policy) {
  report_only_document_policy_ = std::move(policy);
}

SecurityContext::FeatureStatus SecurityContext::IsFeatureEnabled(
    mojom::blink::PermissionsPolicyFeature feature) const {
  DCHECK(permissions_policy_);
  bool permissions_policy_result =
      permissions_policy_->IsFeatureEnabled(feature);
  bool report_only_permissions_policy_result =
      !report_only_permissions_policy_ ||
      report_only_permissions_policy_->IsFeatureEnabled(feature);

  bool should_report =
      !permissions_policy_result || !report_only_permissions_policy_result;

  std::optional<String> reporting_endpoint;
  if (!permissions_policy_result) {
    reporting_endpoint = std::optional<String>(
        permissions_policy_->GetEndpointForFeature(feature));
  } else if (!report_only_permissions_policy_result) {
    reporting_endpoint = std::optional<String>(
        report_only_permissions_policy_->GetEndpointForFeature(feature));
  } else {
    reporting_endpoint = std::nullopt;
  }

  return {permissions_policy_result, should_report, reporting_endpoint};
}

bool SecurityContext::IsFeatureEnabled(
    mojom::blink::DocumentPolicyFeature feature) const {
  DCHECK(GetDocumentPolicyFeatureInfoMap().at(feature).default_value.Type() ==
         mojom::blink::PolicyValueType::kBool);
  return IsFeatureEnabled(feature, PolicyValue::CreateBool(true)).enabled;
}

SecurityContext::FeatureStatus SecurityContext::IsFeatureEnabled(
    mojom::blink::DocumentPolicyFeature feature,
    PolicyValue threshold_value) const {
  DCHECK(document_policy_);
  bool policy_result =
      document_policy_->IsFeatureEnabled(feature, threshold_value);
  bool report_only_policy_result =
      !report_only_document_policy_ ||
      report_only_document_policy_->IsFeatureEnabled(feature, threshold_value);
  return {policy_result, !policy_result || !report_only_policy_result,
          std::nullopt};
}

}  // namespace blink
