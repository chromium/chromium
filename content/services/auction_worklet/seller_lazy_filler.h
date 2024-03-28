// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SELLER_LAZY_FILLER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SELLER_LAZY_FILLER_H_

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class CONTENT_EXPORT AuctionConfigLazyFiller : public PersistedLazyFiller {
 public:
  // `v8_helper` and `auction_ad_config_non_shared_params` must outlive `this`.
  explicit AuctionConfigLazyFiller(AuctionV8Helper* v8_helper);

  void Reset() override;

  // Returns success/failure.
  bool FillInObject(const blink::AuctionConfig::NonSharedParams&
                        auction_ad_config_non_shared_params,
                    v8::Local<v8::Object> object);

 private:
  static void HandleInterestGroupBuyers(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandleDeprecatedRenderURLReplacements(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandlePerBuyerSignals(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandlePerBuyerTimeouts(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandlePerBuyerCumulativeTimeouts(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  void HandleTimeoutsImpl(const v8::PropertyCallbackInfo<v8::Value>& info,
                          const blink::AuctionConfig::MaybePromiseBuyerTimeouts&
                              maybe_promise_buyer_timeouts);

  static void HandlePerBuyerCurrencies(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandlePerBuyerPrioritySignals(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandleRequestedSize(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandleAllSlotsRequestedSizes(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  raw_ptr<const blink::AuctionConfig::NonSharedParams>
      auction_ad_config_non_shared_params_ = nullptr;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_LAZY_FILLER_H_
