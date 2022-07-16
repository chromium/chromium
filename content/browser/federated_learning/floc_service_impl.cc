// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/federated_learning/floc_service_impl.h"

#include "base/bind.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/mojom/federated_learning/floc.mojom.h"

namespace content {

FlocServiceImpl::FlocServiceImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::FlocService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

// static
void FlocServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::FlocService> receiver) {
  DCHECK(render_frame_host);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new FlocServiceImpl(render_frame_host, std::move(receiver));
}

void FlocServiceImpl::GetInterestCohort(GetInterestCohortCallback callback) {
  blink::mojom::InterestCohortPtr interest_cohort =
      GetContentClient()->browser()->GetInterestCohortForJsApi(
          WebContents::FromRenderFrameHost(render_frame_host()),
          render_frame_host()->GetLastCommittedURL(),
          render_frame_host()
              ->GetIsolationInfoForSubresources()
              .top_frame_origin());

  std::move(callback).Run(std::move(interest_cohort));
}

FlocServiceImpl::~FlocServiceImpl() = default;

}  // namespace content
