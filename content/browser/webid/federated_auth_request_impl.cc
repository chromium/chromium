// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

namespace {

// Determines whether |host| is same-origin with all of its ancestors in the
// frame tree. Returns false if not.
// |origin| is provided because it is not considered safe to use
// host->GetLastCommittedOrigin() at some times, so FrameServiceBase::origin()
// should be used to obtain the frame's origin.
bool IsSameOriginWithAncestors(RenderFrameHost* host,
                               const url::Origin& origin) {
  RenderFrameHost* parent = host->GetParent();
  while (parent) {
    if (!parent->GetLastCommittedOrigin().IsSameOriginWith(origin)) {
      return false;
    }
    parent = parent->GetParent();
  }
  return true;
}

}  // namespace

FederatedAuthRequestImpl::FederatedAuthRequestImpl(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver)
    : FrameServiceBase(host, std::move(receiver)) {}

FederatedAuthRequestImpl::~FederatedAuthRequestImpl() = default;

// static
void FederatedAuthRequestImpl::Create(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  DCHECK(host);

  // TODO(kenrb): This should also be verified in the renderer process before
  // the mojo method is invoked, causing the promise to be rejected.
  // https://crbug.com/1141125
  // It is safe to access host->GetLastCommittedOrigin during construction
  // but FrameServiceBase::origin() should be used thereafter.
  if (!IsSameOriginWithAncestors(host, host->GetLastCommittedOrigin())) {
    mojo::ReportBadMessage(
        "WebID cannot be invoked from within cross-origin iframes.");
    return;
  }

  // FederatedAuthRequestImpl owns itself. It will self-destruct when a mojo
  // interface error occurs, the render frame host is deleted, or the render
  // frame host navigates to a new document.
  new FederatedAuthRequestImpl(host, std::move(receiver));
}

void FederatedAuthRequestImpl::RequestIdToken(const ::GURL& provider,
                                              RequestIdTokenCallback callback) {
  // TODO(kenrb): Instantiate a WebIDRequestManager object and invoke
  // the main request handler logic.
  // https://crbug.com/1141125
  std::move(callback).Run("");
}

}  // namespace content
