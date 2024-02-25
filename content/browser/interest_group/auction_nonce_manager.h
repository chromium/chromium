// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_NONCE_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_NONCE_MANAGER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/types/strong_alias.h"
#include "base/uuid.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/content_export.h"

namespace content {

using AuctionNonce = base::StrongAlias<class AuctionNonceTag, base::Uuid>;

// Used to track nonces returned by calls to navigator.createAuctionNonce, which
// are claimed by auction configs passed to future calls to
// navigator.runAdAuction.
class CONTENT_EXPORT AuctionNonceManager {
 public:
  // The frame_host argument is used to write an error to the devtools console
  // when a call to ClaimAuctionNonceIfAvailable is unsuccessful. Callers can
  // pass in a value of nullptr for this, in which case the error message is
  // elided.
  explicit AuctionNonceManager(RenderFrameHostImpl* frame_host);
  ~AuctionNonceManager();

  // Creates a nonce to be claimed by a later call to
  // ClaimAuctionNonceIfAvailable.
  AuctionNonce CreateAuctionNonce();

  // Returns true if the nonce was found (created by a previous
  // call to CreateAuctionNonce and not already claimed), false otherwise.
  bool ClaimAuctionNonceIfAvailable(AuctionNonce nonce);

 private:
  // Used to log a message to the devtool_instrumentation when an attempt to
  // claim an auction nonce is unsuccessful.
  raw_ptr<RenderFrameHostImpl> frame_host_;

  // Auction nonces that have been created via CreateAuctionNonce, but not yet
  // used by a subsequent call to RunAdAuction.
  std::set<AuctionNonce> pending_auction_nonces_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_NONCE_MANAGER_H_
