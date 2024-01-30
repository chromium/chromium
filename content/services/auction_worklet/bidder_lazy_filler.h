// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_LAZY_FILLER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_LAZY_FILLER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class AuctionV8Logger;

class CONTENT_EXPORT InterestGroupLazyFiller : public LazyFiller {
 public:
  // `v8_helper` and `v8_logger` must outlive `this`.
  InterestGroupLazyFiller(AuctionV8Helper* v8_helper,
                          AuctionV8Logger* v8_logger);

  // All arguments must remain valid until Reset() is invoked.
  // Neither argument may be null.
  //
  // May be invoked multiple times on the same object, but Reset() must be
  // invoked between calls.
  void ReInitialize(const GURL* bidding_logic_url,
                    const mojom::BidderWorkletNonSharedParams*
                        bidder_worklet_non_shared_params);

  bool FillInObject(v8::Local<v8::Object> object) override;
  void Reset() override;

 private:
  static void HandleUserBiddingSignals(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandleBiddingLogicUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  // Handles "biddingLogicUrl", which is deprecated.
  // TODO(https://crbug.com/1432707): Remove this method.
  static void HandleDeprecatedBiddingLogicUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandleUpdateUrl(v8::Local<v8::Name> name,
                              const v8::PropertyCallbackInfo<v8::Value>& info);
  // Handles "updateUrl", which is deprecated.
  // TODO(https://crbug.com/1432707): Remove this method.
  static void HandleDeprecatedUpdateUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  // Handles "dailyUpdateUrl", which is deprecated.
  // TODO(https://crbug.com/1420080): Remove this method.
  static void HandleDeprecatedDailyUpdateUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandleTrustedBiddingSignalsKeys(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  static void HandlePriorityVector(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  // TODO(https://crbug.com/1517121): This field is deprecated in favor of
  // "enableBiddingSignalsPrioritization". Remove this function when it's
  // safe to remove the field.
  static void HandleUseBiddingSignalsPrioritization(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  raw_ptr<const GURL> bidding_logic_url_ = nullptr;
  raw_ptr<const mojom::BidderWorkletNonSharedParams>
      bidder_worklet_non_shared_params_ = nullptr;
  const raw_ptr<AuctionV8Logger> v8_logger_;
};

// TODO(crbug.com/1451034): Clean up support for deprecated seconds-based
// version after API users migrate.
enum class PrevWinsType { kSeconds, kMilliseconds };

class CONTENT_EXPORT BiddingBrowserSignalsLazyFiller : public LazyFiller {
 public:
  explicit BiddingBrowserSignalsLazyFiller(AuctionV8Helper* v8_helper);

  void ReInitialize(mojom::BiddingBrowserSignals* bidder_browser_signals,
                    base::Time auction_start_time);

  bool FillInObject(v8::Local<v8::Object> object) override;
  void Reset() override;

 private:
  static void HandlePrevWins(v8::Local<v8::Name> name,
                             const v8::PropertyCallbackInfo<v8::Value>& info);
  static void HandlePrevWinsMs(v8::Local<v8::Name> name,
                               const v8::PropertyCallbackInfo<v8::Value>& info);
  static void HandlePrevWinsInternal(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info,
      PrevWinsType prev_wins_type);

  raw_ptr<mojom::BiddingBrowserSignals> bidder_browser_signals_ = nullptr;
  base::Time auction_start_time_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_LAZY_FILLER_H_
