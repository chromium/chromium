// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/federated_learning/floc_service_impl.h"

#include "base/bind.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"

namespace content {

FlocServiceImpl::FlocServiceImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::FlocService> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)),
      render_frame_host_(static_cast<RenderFrameHostImpl*>(render_frame_host)) {
  DCHECK(render_frame_host_);
}

// static
void FlocServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::FlocService> receiver) {
  DCHECK(render_frame_host);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See FrameServiceBase for details.
  new FlocServiceImpl(render_frame_host, std::move(receiver));
}

void FlocServiceImpl::GetInterestCohort(GetInterestCohortCallback callback) {
  BrowserContext* browser_context = render_frame_host_->GetBrowserContext();
  DCHECK(browser_context);

  std::string interest_cohort =
      GetContentClient()->browser()->GetInterestCohortForJsApi(
          browser_context, render_frame_host_->GetLastCommittedOrigin(),
          render_frame_host_->GetIsolationInfoForSubresources()
              .site_for_cookies());

  std::move(callback).Run(interest_cohort);
}

FlocServiceImpl::~FlocServiceImpl() = default;

}  // namespace content
