// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SELLER_LAZY_FILLER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SELLER_LAZY_FILLER_H_

#include "base/memory/raw_ptr.h"
#include "base/types/optional_ref.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_v8_logger.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class CONTENT_EXPORT SellerBrowserSignalsLazyFiller
    : public PersistedLazyFiller {
 public:
  // `v8_helper` and  `v8_logger` must outlive `this`.
  explicit SellerBrowserSignalsLazyFiller(AuctionV8Helper* v8_helper,
                                          AuctionV8Logger* v8_logger);

  // Returns success/failure. `browser_signal_render_url` must live until
  // Reset() is called.
  bool FillInObject(const GURL& browser_signal_render_url,
                    v8::Local<v8::Object> object);

  void Reset() override;

 private:
  static void HandleDeprecatedRenderUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  raw_ptr<const GURL> browser_signal_render_url_ = nullptr;

  const raw_ptr<AuctionV8Logger> v8_logger_;
};

class CONTENT_EXPORT AuctionConfigLazyFiller : public PersistedLazyFiller {
 public:
  // `v8_helper`, `v8_logger` must outlive `this`.
  explicit AuctionConfigLazyFiller(AuctionV8Helper* v8_helper,
                                   AuctionV8Logger* v8_logger);

  void Reset() override;

  // Returns success/failure. `auction_ad_config_non_shared_params`,
  // `decision_logic_url`, and `trusted_scoring_signals_url` must live until
  // Reset() is called.
  bool FillInObject(const blink::AuctionConfig::NonSharedParams&
                        auction_ad_config_non_shared_params,
                    base::optional_ref<const GURL> decision_logic_url,
                    base::optional_ref<const GURL> trusted_scoring_signals_url,
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

  static void HandleDeprecatedDecisionLogicUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandleDeprecatedTrustedScoringSignalsUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  raw_ptr<const blink::AuctionConfig::NonSharedParams>
      auction_ad_config_non_shared_params_ = nullptr;
  raw_ptr<const GURL> decision_logic_url_ = nullptr;
  raw_ptr<const GURL> trusted_scoring_signals_url_ = nullptr;

  const raw_ptr<AuctionV8Logger> v8_logger_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_SELLER_LAZY_FILLER_H_
