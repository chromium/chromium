// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/remote_security_context.h"

#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

RemoteSecurityContext::RemoteSecurityContext() : SecurityContext(nullptr) {
  // RemoteSecurityContext's origin is expected to stay uninitialized until
  // we set it using replicated origin data from the browser process.
  DCHECK(!GetSecurityOrigin());

  // FIXME: Document::initSecurityContext has a few other things we may
  // eventually want here, such as enforcing a setting to
  // grantUniversalAccess().
}

void RemoteSecurityContext::SetReplicatedOrigin(
    scoped_refptr<SecurityOrigin> origin) {
  DCHECK(origin);
  SetSecurityOrigin(std::move(origin));
}

void RemoteSecurityContext::ResetAndEnforceSandboxFlags(
    network::mojom::blink::WebSandboxFlags flags) {
  sandbox_flags_ = flags;

  if (IsSandboxed(network::mojom::blink::WebSandboxFlags::kOrigin) &&
      GetSecurityOrigin() && !GetSecurityOrigin()->IsOpaque()) {
    SetSecurityOrigin(GetSecurityOrigin()->DeriveNewOpaqueOrigin());
  }
}

void RemoteSecurityContext::InitializeFeaturePolicy(
    const ParsedFeaturePolicy& parsed_header,
    const ParsedFeaturePolicy& container_policy,
    const FeaturePolicy* parent_feature_policy) {
  report_only_feature_policy_ = nullptr;
  feature_policy_ = FeaturePolicy::CreateFromParentPolicy(
      parent_feature_policy, container_policy, security_origin_->ToUrlOrigin());
  feature_policy_->SetHeaderPolicy(parsed_header);
}

}  // namespace blink
