// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_shared_storage_host.h"

#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

namespace {

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
    storage::SharedStorageManager* shared_storage_manager)
    : shared_storage_manager_(shared_storage_manager) {
  DCHECK(shared_storage_manager_);
}

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

void AuctionSharedStorageHost::Set(
    const std::u16string& key,
    const std::u16string& value,
    bool ignore_if_present,
    auction_worklet::mojom::AuctionWorkletFunction
        source_auction_worklet_function) {
  storage::SharedStorageManager::SetBehavior set_behavior =
      ignore_if_present
          ? storage::SharedStorageManager::SetBehavior::kIgnoreIfPresent
          : storage::SharedStorageManager::SetBehavior::kDefault;

  shared_storage_manager_->Set(receiver_set_.current_context().worklet_origin,
                               key, value, base::DoNothing(), set_behavior);

  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      receiver_set_.current_context().auction_runner_rfh,
      ToWebFeature(source_auction_worklet_function));
}

void AuctionSharedStorageHost::Append(
    const std::u16string& key,
    const std::u16string& value,
    auction_worklet::mojom::AuctionWorkletFunction
        source_auction_worklet_function) {
  shared_storage_manager_->Append(
      receiver_set_.current_context().worklet_origin, key, value,
      base::DoNothing());

  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      receiver_set_.current_context().auction_runner_rfh,
      ToWebFeature(source_auction_worklet_function));
}

void AuctionSharedStorageHost::Delete(
    const std::u16string& key,
    auction_worklet::mojom::AuctionWorkletFunction
        source_auction_worklet_function) {
  shared_storage_manager_->Delete(
      receiver_set_.current_context().worklet_origin, key, base::DoNothing());

  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      receiver_set_.current_context().auction_runner_rfh,
      ToWebFeature(source_auction_worklet_function));
}

void AuctionSharedStorageHost::Clear(
    auction_worklet::mojom::AuctionWorkletFunction
        source_auction_worklet_function) {
  shared_storage_manager_->Clear(receiver_set_.current_context().worklet_origin,
                                 base::DoNothing());

  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      receiver_set_.current_context().auction_runner_rfh,
      ToWebFeature(source_auction_worklet_function));
}

}  // namespace content
