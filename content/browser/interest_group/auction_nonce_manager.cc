// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_nonce_manager.h"

#include "base/feature_list.h"
#include "base/uuid.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

AuctionNonceManager::AuctionNonceManager(RenderFrameHostImpl* frame_host)
    : frame_host_(frame_host) {}

AuctionNonceManager::~AuctionNonceManager() = default;

AuctionNonce AuctionNonceManager::CreateAuctionNonce() {
  AuctionNonce nonce =
      static_cast<AuctionNonce>(base::Uuid::GenerateRandomV4());
  pending_auction_nonces_.insert(nonce);
  return nonce;
}

bool AuctionNonceManager::ClaimAuctionNonceIfAvailable(AuctionNonce nonce) {
  if (auto nonce_iter = pending_auction_nonces_.find(nonce);
      nonce_iter != pending_auction_nonces_.end()) {
    pending_auction_nonces_.erase(nonce_iter);
    return true;
  }

  // No matching auction nonce from a prior call to CreateAuctionNonce.
  if (frame_host_) {
    devtools_instrumentation::LogWorkletMessage(
        *frame_host_, blink::mojom::ConsoleMessageLevel::kError,
        "Invalid AuctionConfig. The config provided an auctionNonce value "
        "that was _not_ created by a previous call to createAuctionNonce.");
  }
  return false;
}

}  // namespace content
