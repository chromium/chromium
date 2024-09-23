// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_nonce_manager.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/uuid.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

AuctionNonceManager::AuctionNonceManager(RenderFrameHostImpl* frame_host)
    : frame_host_(frame_host) {}

AuctionNonceManager::~AuctionNonceManager() = default;

namespace {
// Returns true iff the first 30 characters of the two provided UUIDs are
// identical. Each UUID follows the standard structure of a UUIDv4, such that
// each is represented as a 36-character string of groups of hexadecimal digits
// separated by dashes. If either Uuid provided is not valid, this always return
// false.
bool HasIdenticalNoncePrefix(base::Uuid base_nonce, AuctionNonce nonce) {
  if (!base_nonce.is_valid() || !nonce->is_valid()) {
    return false;
  }
  const std::string& base_nonce_string = base_nonce.AsLowercaseString();
  const std::string& nonce_string = nonce->AsLowercaseString();
  return base_nonce_string.substr(0, 30) == nonce_string.substr(0, 30);
}

// Gets the three least significant bytes of the provided nonce and returns
// them as a uint32_t. This expects that the nonce was already verified to be
// valid.
uint32_t GetNonceSuffix(AuctionNonce nonce) {
  const std::string& nonce_string = nonce->AsLowercaseString();
  uint32_t nonce_suffix;
  CHECK(base::HexStringToUInt(std::string_view(nonce_string).substr(30),
                              &nonce_suffix));
  return nonce_suffix;
}
}  // namespace

bool AuctionNonceManager::ClaimAuctionNonceIfAvailable(AuctionNonce nonce) {
  if (!frame_host_ || !nonce->is_valid()) {
    return false;
  }

  if (!HasIdenticalNoncePrefix(frame_host_->GetBaseAuctionNonce(), nonce)) {
    devtools_instrumentation::LogWorkletMessage(
        *frame_host_, blink::mojom::ConsoleMessageLevel::kError,
        "Invalid AuctionConfig. The config provided an auctionNonce value "
        "that was _not_ created by a previous call to createAuctionNonce.");
    return false;
  }

  uint32_t nonce_suffix = GetNonceSuffix(nonce);
  if (base::Contains(claimed_auction_nonce_suffixes_, nonce_suffix)) {
    devtools_instrumentation::LogWorkletMessage(
        *frame_host_, blink::mojom::ConsoleMessageLevel::kError,
        "Invalid AuctionConfig. The config provided an auctionNonce value "
        "that was already used by a previous call auction config.");
    return false;
  }

  claimed_auction_nonce_suffixes_.insert(nonce_suffix);
  return true;
}

}  // namespace content
