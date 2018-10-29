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

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// static
std::vector<unsigned> SecurityContext::SerializeInsecureNavigationSet(
    const InsecureNavigationsSet& set) {
  // The set is serialized as a sorted array. Sorting it makes it easy to know
  // if two serialized sets are equal.
  std::vector<unsigned> serialized;
  serialized.reserve(set.size());
  for (unsigned host : set)
    serialized.push_back(host);
  std::sort(serialized.begin(), serialized.end());

  return serialized;
}

SecurityContext::SecurityContext()
    : sandbox_flags_(kSandboxNone),
      address_space_(mojom::IPAddressSpace::kPublic),
      insecure_request_policy_(kLeaveInsecureRequestsAlone),
      require_safe_types_(false) {}

SecurityContext::~SecurityContext() = default;

void SecurityContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(content_security_policy_);
}

void SecurityContext::SetSecurityOrigin(
    scoped_refptr<SecurityOrigin> security_origin) {
  security_origin_ = std::move(security_origin);
}

void SecurityContext::SetContentSecurityPolicy(
    ContentSecurityPolicy* content_security_policy) {
  content_security_policy_ = content_security_policy;
}

void SecurityContext::EnforceSandboxFlags(SandboxFlags mask) {
  ApplySandboxFlags(mask);
}

void SecurityContext::ApplySandboxFlags(SandboxFlags mask,
                                        bool is_potentially_trustworthy) {
  sandbox_flags_ |= mask;

  if (IsSandboxed(kSandboxOrigin) && GetSecurityOrigin() &&
      !GetSecurityOrigin()->IsOpaque()) {
    scoped_refptr<SecurityOrigin> security_origin =
        GetSecurityOrigin()->DeriveNewOpaqueOrigin();
    security_origin->SetOpaqueOriginIsPotentiallyTrustworthy(
        is_potentially_trustworthy);
    SetSecurityOrigin(std::move(security_origin));
    DidUpdateSecurityOrigin();
  }
}

String SecurityContext::addressSpaceForBindings() const {
  switch (address_space_) {
    case mojom::IPAddressSpace::kPublic:
      return "public";

    case mojom::IPAddressSpace::kPrivate:
      return "private";

    case mojom::IPAddressSpace::kLocal:
      return "local";
  }
  NOTREACHED();
  return "public";
}

void SecurityContext::SetFeaturePolicy(
    std::unique_ptr<FeaturePolicy> feature_policy) {
  // This method should be called before a FeaturePolicy has been created.
  DCHECK(!feature_policy_);
  feature_policy_ = std::move(feature_policy);
}

void SecurityContext::InitializeFeaturePolicy(
    const ParsedFeaturePolicy& parsed_header,
    const ParsedFeaturePolicy& container_policy,
    const FeaturePolicy* parent_feature_policy) {
  feature_policy_ = FeaturePolicy::CreateFromParentPolicy(
      parent_feature_policy, container_policy, security_origin_->ToUrlOrigin());
  feature_policy_->SetHeaderPolicy(parsed_header);
}

bool SecurityContext::IsFeatureEnabled(mojom::FeaturePolicyFeature feature,
                                       ReportOptions report_on_failure,
                                       const String& message) const {
  // The policy should always be initialized before checking it to ensure we
  // properly inherit the parent policy.
  DCHECK(feature_policy_);

  if (feature_policy_->IsFeatureEnabled(feature))
    return true;
  if (report_on_failure == ReportOptions::kReportOnFailure)
    ReportFeaturePolicyViolation(feature, message);
  return false;
}

}  // namespace blink
