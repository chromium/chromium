// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_service_impl.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"

namespace content {

AdAuctionServiceImpl::AdAuctionServiceImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)) {}

// static
void AdAuctionServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver) {
  DCHECK(render_frame_host);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See FrameServiceBase for details.
  new AdAuctionServiceImpl(render_frame_host, std::move(receiver));
}

void AdAuctionServiceImpl::RunAdAuction(blink::mojom::AuctionAdConfigPtr config,
                                        RunAdAuctionCallback callback) {
  // TODO(crbug.com/1186444): Pass |config| to auction manager service and call
  // callback with result.

  std::move(callback).Run(base::nullopt);
}

AdAuctionServiceImpl::~AdAuctionServiceImpl() = default;

}  // namespace content
