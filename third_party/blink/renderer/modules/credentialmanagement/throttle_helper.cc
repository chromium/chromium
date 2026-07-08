// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/credentialmanagement/throttle_helper.h"

#include "base/functional/callback_helpers.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

void SetIdpSigninStatus(const blink::LocalFrameToken& local_frame_token,
                        const url::Origin& origin,
                        mojom::blink::IdpSigninStatus status) {
  CHECK(IsMainThread());
  LocalFrame* local_frame = LocalFrame::FromFrameToken(local_frame_token);
  // Null checking DomWindow() and GetFrame() for detached frame case. See
  // https://crbug.com/382646175 for details.
  if (!local_frame || !local_frame->DomWindow() ||
      !local_frame->DomWindow()->GetFrame()) {
    return;
  }

  auto* service = CredentialManagerProxy::From(local_frame->DomWindow())
                      ->FederatedRequestService();
  service->SetIdpSigninStatus(SecurityOrigin::CreateFromUrlOrigin(origin),
                              status, /*options=*/nullptr, base::DoNothing());
}

}  // namespace blink
