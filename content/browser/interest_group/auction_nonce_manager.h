// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_NONCE_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_NONCE_MANAGER_H_

#include <memory>
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
  // The frame_host argument is used to access the base auction nonce used to
  // generate all auction nonces returned by navigator.createAuctionNonce.
  // If this is null, ClaimAuctionNonceIfAvailable will always return false.
  explicit AuctionNonceManager(RenderFrameHostImpl* frame_host);
  ~AuctionNonceManager();

  // Returns true if *both* of the following are true:
  // 1) The first 32 characters of the given nonce match the
  //    first 32 characters of the nonce stored on the frame_host.
  // 2) The given auction nonce has not been previously claimed.
  //
  // If both of the above are true, this also records that this auction nonce
  // was seen in order to correctly handle future calls to this method.
  bool ClaimAuctionNonceIfAvailable(AuctionNonce nonce);

 private:
  // Used to determine the base auction nonce used to generate all auction
  // nonces returned by navigator.createAuctionNonce, and also to log a message
  // to the devtool_instrumentation when an attempt to claim an auction nonce is
  // unsuccessful.
  raw_ptr<RenderFrameHostImpl> frame_host_;

  // Suffixes (the last six hexadecimal characters) of auction nonces that have
  // been claimed by a prior successful call to ClaimAuctionNonceIfAvailable.
  // Because ClaimAuctionNonceIfAvailable guarantees that the first 30
  // characters of any claimed nonce matches those of the base auction nonce,
  // this only stores the nonce suffixes - the last 3 bytes of each nonce -
  // for space efficiency, instead of recording the full UUID.
  std::set<uint32_t> claimed_auction_nonce_suffixes_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_NONCE_MANAGER_H_
