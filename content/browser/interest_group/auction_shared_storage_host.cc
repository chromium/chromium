// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_shared_storage_host.h"

#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "services/network/public/mojom/shared_storage.mojom.h"

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

void AuctionSharedStorageHost::SharedStorageUpdate(
    network::mojom::SharedStorageModifierMethodPtr method,
    auction_worklet::mojom::AuctionWorkletFunction
        source_auction_worklet_function) {
  if (method->is_set_method()) {
    network::mojom::SharedStorageSetMethodPtr& set_method =
        method->get_set_method();

    storage::SharedStorageManager::SetBehavior set_behavior =
        set_method->ignore_if_present
            ? storage::SharedStorageManager::SetBehavior::kIgnoreIfPresent
            : storage::SharedStorageManager::SetBehavior::kDefault;

    shared_storage_manager_->Set(receiver_set_.current_context().worklet_origin,
                                 set_method->key, set_method->value,
                                 base::DoNothing(), set_behavior);
  } else if (method->is_append_method()) {
    network::mojom::SharedStorageAppendMethodPtr& append_method =
        method->get_append_method();

    shared_storage_manager_->Append(
        receiver_set_.current_context().worklet_origin, append_method->key,
        append_method->value, base::DoNothing());
  } else if (method->is_delete_method()) {
    network::mojom::SharedStorageDeleteMethodPtr& delete_method =
        method->get_delete_method();

    shared_storage_manager_->Delete(
        receiver_set_.current_context().worklet_origin, delete_method->key,
        base::DoNothing());
  } else {
    CHECK(method->is_clear_method());

    shared_storage_manager_->Clear(
        receiver_set_.current_context().worklet_origin, base::DoNothing());
  }

  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      receiver_set_.current_context().auction_runner_rfh,
      ToWebFeature(source_auction_worklet_function));
}

}  // namespace content
