// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/remote_security_context.h"

#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

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

void RemoteSecurityContext::InitializePermissionsPolicy(
    const ParsedPermissionsPolicy& parsed_header,
    const ParsedPermissionsPolicy& container_policy,
    const PermissionsPolicy* parent_permissions_policy) {
  report_only_permissions_policy_ = nullptr;
  permissions_policy_ = PermissionsPolicy::CreateFromParentPolicy(
      parent_permissions_policy, parsed_header, container_policy,
      security_origin_->ToUrlOrigin());
}

}  // namespace blink
