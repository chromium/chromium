// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/binder_wrappers/ad_auction_service_impl_wrapper.h"

#include "content/browser/interest_group/ad_auction_service_impl.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"

namespace content {

namespace internal {

void AdAuctionServiceImplCreateMojoService(
    mojo::BinderMapWithContext<RenderFrameHost*>& map) {
  map.Add<blink::mojom::AdAuctionService>(
      base::BindRepeating(&AdAuctionServiceImpl::CreateMojoService));
}

}  // namespace internal

}  // namespace content
