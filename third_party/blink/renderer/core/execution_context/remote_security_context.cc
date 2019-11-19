// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/remote_security_context.h"

#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

RemoteSecurityContext::RemoteSecurityContext() : SecurityContext() {
  // RemoteSecurityContext's origin is expected to stay uninitialized until
  // we set it using replicated origin data from the browser process.
  DCHECK(!GetSecurityOrigin());

  // Start with a clean slate.
  SetContentSecurityPolicy(MakeGarbageCollected<ContentSecurityPolicy>());

  // FIXME: Document::initSecurityContext has a few other things we may
  // eventually want here, such as enforcing a setting to
  // grantUniversalAccess().
}

void RemoteSecurityContext::Trace(blink::Visitor* visitor) {
  SecurityContext::Trace(visitor);
}

void RemoteSecurityContext::SetReplicatedOrigin(
    scoped_refptr<SecurityOrigin> origin) {
  DCHECK(origin);
  SetSecurityOrigin(std::move(origin));
  GetContentSecurityPolicy()->SetupSelf(*GetSecurityOrigin());
}

void RemoteSecurityContext::ResetReplicatedContentSecurityPolicy() {
  DCHECK(GetSecurityOrigin());
  SetContentSecurityPolicy(MakeGarbageCollected<ContentSecurityPolicy>());
  GetContentSecurityPolicy()->SetupSelf(*GetSecurityOrigin());
}

void RemoteSecurityContext::ResetAndEnforceSandboxFlags(WebSandboxFlags flags) {
  sandbox_flags_ = flags;

  if (IsSandboxed(WebSandboxFlags::kOrigin) && GetSecurityOrigin() &&
      !GetSecurityOrigin()->IsOpaque()) {
    SetSecurityOrigin(GetSecurityOrigin()->DeriveNewOpaqueOrigin());
  }
}

void RemoteSecurityContext::InitializeFeaturePolicy(
    const ParsedFeaturePolicy& parsed_header,
    const ParsedFeaturePolicy& container_policy,
    const FeaturePolicy* parent_feature_policy,
    const FeaturePolicy::FeatureState* opener_feature_state) {
  // Feature policy should either come from a parent in the case of an embedded
  // child frame, or from an opener if any when a new window is created by an
  // opener. A main frame without an opener would not have a parent policy nor
  // an opener feature state.
  DCHECK(!parent_feature_policy || !opener_feature_state);
  report_only_feature_policy_ = nullptr;
  if (!opener_feature_state ||
      !RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
    feature_policy_ = FeaturePolicy::CreateFromParentPolicy(
        parent_feature_policy, container_policy,
        security_origin_->ToUrlOrigin());
  } else {
    DCHECK(!parent_feature_policy);
    feature_policy_ = FeaturePolicy::CreateWithOpenerPolicy(
        *opener_feature_state, security_origin_->ToUrlOrigin());
  }
  feature_policy_->SetHeaderPolicy(parsed_header);
}

}  // namespace blink
