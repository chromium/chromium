// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_response_impl.h"

#include "base/callback.h"
#include "content/browser/webid/id_token_request_callback_data.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

using blink::mojom::ProvideIdTokenStatus;

namespace content {

FederatedAuthResponseImpl::FederatedAuthResponseImpl(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::FederatedAuthResponse> receiver)
    : DocumentService(host, std::move(receiver)) {}

// TODO(majidvp): We should reject any pending promise here.
// http://crbug.com/1141125
FederatedAuthResponseImpl::~FederatedAuthResponseImpl() = default;

// static
void FederatedAuthResponseImpl::Create(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::FederatedAuthResponse> receiver) {
  DCHECK(host);

  // TODO(kenrb): This should also be verified in the renderer process before
  // the mojo method is invoked, causing the promise to be rejected.
  // https://crbug.com/1141125
  // It is safe to access host->GetLastCommittedOrigin during construction
  // but DocumentService::origin() should be used thereafter.
  if (!IsSameOriginWithAncestors(host, host->GetLastCommittedOrigin())) {
    mojo::ReportBadMessage(
        "FedCM cannot be invoked from within cross-origin iframes.");
    return;
  }

  // FederatedAuthRequestImpl owns itself. It will self-destruct when a mojo
  // interface error occurs, the render frame host is deleted, or the render
  // frame host navigates to a new document.
  new FederatedAuthResponseImpl(host, std::move(receiver));
}

void FederatedAuthResponseImpl::ProvideIdToken(
    const std::string& id_token,
    ProvideIdTokenCallback idp_callback) {
  // The ptr below is actually the same as |idp_web_contents_| but because this
  // is a different instance of |FederatedAuthRequestImpl| for which
  // |idp_web_contents_| has not been initialized.
  //
  // TODO(majidvp): We should have two separate mojo service for request and
  // response sides would have make this more obvious. http://crbug.com/1141125
  WebContents* idp_web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host());
  auto* request_callback_data =
      IdTokenRequestCallbackData::Get(idp_web_contents);

  // TODO(majidvp): This may happen if the page is not loaded by the browser's
  // WebID machinery. We need a way for IDP logic to detect that and not provide
  // a token. The current plan is to send a special header but we may also need
  // to not expose this in JS somehow. Investigate this further.
  // http://crbug.com/1141125
  if (!request_callback_data) {
    std::move(idp_callback).Run(ProvideIdTokenStatus::kError);
    return;
  }

  // After running the RP done callback the IDP sign-in page gets closed and its
  // web contents cleared in `FederatedAuthRequestImpl::CompleteRequest()`. So
  // we should not access |idp_web_contents| or any of its associated objects
  // as it may already be destructed. This is why we first run any logic that
  // needs to touch the IDP web contents and then run the RP done callback.

  auto rp_done_callback = request_callback_data->TakeDoneCallback();
  IdTokenRequestCallbackData::Remove(idp_web_contents);

  if (!rp_done_callback) {
    std::move(idp_callback).Run(ProvideIdTokenStatus::kErrorTooManyResponses);
    return;
  }
  std::move(idp_callback).Run(ProvideIdTokenStatus::kSuccess);

  std::move(rp_done_callback).Run(id_token);
  // Don't access |idp_web_contents| passed this point.
}

}  // namespace content
