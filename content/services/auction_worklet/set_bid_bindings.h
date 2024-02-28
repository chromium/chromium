// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SET_BID_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SET_BID_BINDINGS_H_

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/reject_reason.mojom.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

// Class to manage bindings for setting a bidding result. Expected to be used
// for a context managed by ContextRecycler.
class CONTENT_EXPORT SetBidBindings : public Bindings {
 public:
  explicit SetBidBindings(AuctionV8Helper* v8_helper);
  SetBidBindings(const SetBidBindings&) = delete;
  SetBidBindings& operator=(const SetBidBindings&) = delete;
  ~SetBidBindings() override;

  // This must be called before every time this is used.
  // bidder_worklet_non_shared_params->ads.has_value() must be true.
  void ReInitialize(
      base::TimeTicks start,
      bool has_top_level_seller_origin,
      const mojom::BidderWorkletNonSharedParams*
          bidder_worklet_non_shared_params,
      const std::optional<blink::AdCurrency>& per_buyer_currency,
      uint16_t multi_bid_limit,
      base::RepeatingCallback<bool(const std::string&)> is_ad_excluded,
      base::RepeatingCallback<bool(const std::string&)>
          is_component_ad_excluded);

  void AttachToContext(v8::Local<v8::Context> context) override;
  void Reset() override;

  bool has_bids() const { return !bids_.empty(); }

  // Note that returned bids do not have their role filled in correctly,
  // it's always kUnenforcedKAnon.
  std::vector<mojom::BidderWorkletBidPtr> TakeBids();

  mojom::RejectReason reject_reason() const { return reject_reason_; }

  // Overwrites the set bids with however many are in `bid_value`, and returns
  // if any errors occurred.
  IdlConvert::Status SetBidImpl(v8::Local<v8::Value> bid_value,
                                std::string error_prefix);

 private:
  struct GenerateBidOutput;

  static void SetBid(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Attempts to convert a single bid from JS to IDL, returning either the IDL
  // type or a failing status.
  //
  // `error_prefix` is the base prefix for errors, `render_prefix` is one for
  // errors in render field, and `components_prefix` is for errors in the
  // adComponents array.
  base::expected<GenerateBidOutput, IdlConvert::Status> ConvertBidToIDL(
      AuctionV8Helper::TimeLimitScope& time_limit_scope,
      v8::Local<v8::Value> input,
      const std::string& error_prefix,
      const std::string& render_prefix,
      const std::string& components_prefix);

  // Tries to semantically check the bid in `idl`, and convert it to
  // mojom::BidderWorkletBid.
  //
  // Note that a valid value that results in no bid is not considered an error.
  // In case of an error, may also set `reject_reason_`.
  //
  // `error_prefix` is the base prefix for errors, `render_prefix` is one for
  // errors in render field, and `components_prefix` is for errors in the
  // adComponents array.
  base::expected<mojom::BidderWorkletBidPtr, IdlConvert::Status>
  SemanticCheckBid(AuctionV8Helper::TimeLimitScope& time_limit_scope,
                   const GenerateBidOutput& idl,
                   const std::string& error_prefix,
                   const std::string& render_prefix,
                   const std::string& components_prefix);

  const raw_ptr<AuctionV8Helper> v8_helper_;

  base::TimeTicks start_;
  bool has_top_level_seller_origin_ = false;
  mojom::RejectReason reject_reason_ = mojom::RejectReason::kNotAvailable;

  raw_ptr<const mojom::BidderWorkletNonSharedParams>
      bidder_worklet_non_shared_params_ = nullptr;

  std::optional<blink::AdCurrency> per_buyer_currency_;

  uint16_t multi_bid_limit_ = 1;

  // Callbacks set by ReInitialize and cleared by Reset which tell if an ad URL
  // can be used in a valid bid. Used to check the bid for non-k-anonymous ads.
  base::RepeatingCallback<bool(const std::string&)> is_ad_excluded_;
  base::RepeatingCallback<bool(const std::string&)> is_component_ad_excluded_;

  std::vector<mojom::BidderWorkletBidPtr> bids_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_SET_BID_BINDINGS_H_
