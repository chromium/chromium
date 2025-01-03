// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_shared_storage_host.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_lock_manager.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "services/network/public/mojom/shared_storage.mojom.h"

namespace content {

namespace {

using AccessScope = SharedStorageLockManager::AccessScope;

blink::mojom::WebFeature ToWebFeature(
    auction_worklet::mojom::AuctionWorkletFunction auction_worklet_function) {
  switch (auction_worklet_function) {
    case auction_worklet::mojom::AuctionWorkletFunction::kBidderGenerateBid:
      return blink::mojom::WebFeature::kSharedStorageWriteFromBidderGenerateBid;
    case auction_worklet::mojom::AuctionWorkletFunction::kBidderReportWin:
      return blink::mojom::WebFeature::kSharedStorageWriteFromBidderReportWin;
    case auction_worklet::mojom::AuctionWorkletFunction::kSellerScoreAd:
      return blink::mojom::WebFeature::kSharedStorageWriteFromSellerScoreAd;
    case auction_worklet::mojom::AuctionWorkletFunction::kSellerReportResult:
      return blink::mojom::WebFeature::
          kSharedStorageWriteFromSellerReportResult;
  }
  NOTREACHED();
}

}  // namespace

struct AuctionSharedStorageHost::ReceiverContext {
  // `auction_runner_rfh` is the frame associated with the
  // `AdAuctionServiceImpl` that owns `this`. Thus, `auction_runner_rfh` must
  // outlive `this`.
  raw_ptr<RenderFrameHostImpl> auction_runner_rfh;
  url::Origin worklet_origin;
};

AuctionSharedStorageHost::AuctionSharedStorageHost(
    StoragePartitionImpl* storage_partition)
    : storage_partition_(storage_partition) {}

AuctionSharedStorageHost::~AuctionSharedStorageHost() = default;

void AuctionSharedStorageHost::BindNewReceiver(
    RenderFrameHostImpl* auction_runner_rfh,
    const url::Origin& worklet_origin,
    mojo::PendingReceiver<auction_worklet::mojom::AuctionSharedStorageHost>
        receiver) {
  receiver_set_.Add(this, std::move(receiver),
                    ReceiverContext{.auction_runner_rfh = auction_runner_rfh,
                                    .worklet_origin = worklet_origin});
}

void AuctionSharedStorageHost::SharedStorageUpdate(
    network::mojom::SharedStorageModifierMethodWithOptionsPtr
        method_with_options,
    auction_worklet::mojom::AuctionWorkletFunction
        source_auction_worklet_function) {
  FrameTreeNodeId main_frame_id =
      receiver_set_.current_context()
          .auction_runner_rfh->GetOutermostMainFrame()
          ->GetFrameTreeNodeId();

  storage_partition_->GetSharedStorageRuntimeManager()
      ->lock_manager()
      .SharedStorageUpdate(std::move(method_with_options),
                           receiver_set_.current_context().worklet_origin,
                           AccessScope::kProtectedAudienceWorklet,
                           main_frame_id, base::DoNothing());

  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      receiver_set_.current_context().auction_runner_rfh,
      ToWebFeature(source_auction_worklet_function));
}

}  // namespace content
