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

class CONTENT_EXPORT AuctionNonceManager {
 public:
  virtual ~AuctionNonceManager();

  // Creates an auction nonce to be claimed by a later call to
  // AuctionNonceManager::ClaimAuctionNonceIfAvailable.
  virtual AuctionNonce CreateAuctionNonce() = 0;

  // Returns true if the nonce is valid, false otherwise.
  virtual bool ClaimAuctionNonceIfAvailable(AuctionNonce nonce) = 0;
};

std::unique_ptr<AuctionNonceManager> CreateAuctionNonceManager(
    RenderFrameHostImpl* frame_host);

// Used to track nonces returned by calls to navigator.createAuctionNonce, which
// are claimed by auction configs passed to future calls to
// navigator.runAdAuction. This is used when
// blink::features::kFledgeCreateAuctionNonceSynchronousResolution is
// *enabled*. Exposed for test. Browser code should create an instance of this
// using CreateAuctionNonceManager.
class CONTENT_EXPORT SynchronousAuctionNonceManager
    : public AuctionNonceManager {
 public:
  // The frame_host argument is used to access the base auction nonce used to
  // generate all auction nonces returned by navigator.createAuctionNonce.
  // If this is null, ClaimAuctionNonceIfAvailable will always return false.
  explicit SynchronousAuctionNonceManager(RenderFrameHostImpl* frame_host);
  ~SynchronousAuctionNonceManager() override;

  // Invalid on SynchronousAuctionNonceManager.
  // Calls to this function will always CHECK-fail!
  AuctionNonce CreateAuctionNonce() override;

  // Returns true if *both* of the following are true:
  // 1) The first 32 characters of the given nonce match the
  //    first 32 characters of the nonce stored on the frame_host.
  // 2) The given auction nonce has not been previously claimed.
  //
  // If both of the above are true, this also records that this auction nonce
  // was seen in order to correctly handle future calls to this method.
  bool ClaimAuctionNonceIfAvailable(AuctionNonce nonce) override;

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

// Used to track nonces returned by calls to navigator.createAuctionNonce, which
// are claimed by auction configs passed to future calls to
// navigator.runAdAuction. This is used when
// blink::features::kFledgeCreateAuctionNonceSynchronousResolution is
// *disabled*. Exposed for test. Browser code should create an instance of this
// using CreateAuctionNonceManager.
class CONTENT_EXPORT AsynchronousAuctionNonceManager
    : public AuctionNonceManager {
 public:
  // The frame_host argument is used to write an error to the devtools console
  // when a call to ClaimAuctionNonceIfAvailable is unsuccessful. Callers can
  // pass in a value of nullptr for this, in which case the error message is
  // elided.
  explicit AsynchronousAuctionNonceManager(RenderFrameHostImpl* frame_host);
  ~AsynchronousAuctionNonceManager() override;

  // Creates a nonce to be claimed by a later call to
  // ClaimAuctionNonceIfAvailable.
  AuctionNonce CreateAuctionNonce() override;

  // Returns true if the nonce was found (created by a previous
  // call to CreateAuctionNonce and not already claimed), false otherwise.
  bool ClaimAuctionNonceIfAvailable(AuctionNonce nonce) override;

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
