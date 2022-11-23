// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/webid_utils.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/federated_identity_sharing_permission_context_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/web_identity.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

bool IsSameOriginWithAncestors(RenderFrameHost* host,
                               const url::Origin& origin) {
  RenderFrameHost* parent = host->GetParentOrOuterDocument();
  while (parent) {
    if (!parent->GetLastCommittedOrigin().IsSameOriginWith(origin)) {
      return false;
    }
    parent = parent->GetParent();
  }
  return true;
}

void SetIdpSigninStatus(content::BrowserContext* context,
                        const url::Origin& origin,
                        blink::mojom::IdpSigninStatus status) {
  auto* delegate = context->GetFederatedIdentitySharingPermissionContext();
  if (!delegate) {
    // The embedder may not have a delegate (e.g. webview)
    return;
  }
  delegate->SetIdpSigninStatus(
      origin, status == blink::mojom::IdpSigninStatus::kSignedIn);
}

}  // namespace content
