// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_LAZY_FILLER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_LAZY_FILLER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "v8/include/v8-forward.h"

class GURL;

namespace auction_worklet {

class AuctionV8Logger;

class CONTENT_EXPORT InterestGroupLazyFiller : public PersistedLazyFiller {
 public:
  // `v8_helper` and `v8_logger` must outlive `this`.
  InterestGroupLazyFiller(AuctionV8Helper* v8_helper,
                          AuctionV8Logger* v8_logger);

  // All arguments must remain valid until Reset() is invoked.
  // `bidding_logic_url` and `bidder_worklet_non_shared_params` must not be
  // null.
  //
  // May be invoked multiple times on the same object, but Reset() must be
  // invoked between calls.
  void ReInitialize(const GURL* bidding_logic_url,
                    const GURL* bidding_wasm_helper_url,
                    const GURL* trusted_bidding_signals_url,
                    const mojom::BidderWorkletNonSharedParams*
                        bidder_worklet_non_shared_params);

  // Returns success/failure.
  //
  // `is_ad_excluded` and `is_ad_component_excluded` are used to filter the
  // `ads` and `adComponents` arrays, respectively.
  bool FillInObject(
      v8::Local<v8::Object> object,
      base::RepeatingCallback<bool(const std::string&)> is_ad_excluded,
      base::RepeatingCallback<bool(const std::string&)>
          is_ad_component_excluded,
      base::RepeatingCallback<bool(
          const std::string& ad_render_url,
          base::optional_ref<const std::string> buyer_reporting_id,
          base::optional_ref<const std::string> buyer_and_seller_reporting_id,
          base::optional_ref<const std::string>
              selected_buyer_and_seller_reporting_id)>
          is_reporting_id_set_excluded);

  void Reset() override;

 private:
  // Converts a vector of blink::InterestGroup::Ads into a v8 object, and writes
  // it to `object's` `name` field. Sets `deprecated_render_url_callback` as the
  // lazy callback for the deprecated `renderUrl` field of each entry. Returns
  // false on failure.
  bool CreateAdVector(
      v8::Local<v8::Object>& object,
      std::string_view name,
      base::RepeatingCallback<bool(const std::string&)> is_ad_excluded,
      base::RepeatingCallback<bool(const std::string&,
                                   base::optional_ref<const std::string>,
                                   base::optional_ref<const std::string>,
                                   base::optional_ref<const std::string>)>
          is_reporting_id_set_excluded,
      const std::vector<blink::InterestGroup::Ad>& ads,
      v8::Local<v8::ObjectTemplate>& lazy_filler_template);

  static void HandleUserBiddingSignals(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandleBiddingLogicUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  // Handles "biddingLogicUrl", which is deprecated.
  // TODO(crbug.com/40266734): Remove this method.
  static void HandleDeprecatedBiddingLogicUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandleBiddingWasmHelperUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  // Handles "BiddingWasmHelperUrl", which is deprecated.
  // TODO(crbug.com/40266734): Remove this method.
  static void HandleDeprecatedBiddingWasmHelperUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandleUpdateUrl(v8::Local<v8::Name> name,
                              const v8::PropertyCallbackInfo<v8::Value>& info);
  // Handles "updateUrl", which is deprecated.
  // TODO(crbug.com/40266734): Remove this method.
  static void HandleDeprecatedUpdateUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  // Handles "dailyUpdateUrl", which is deprecated.
  // TODO(crbug.com/40258629): Remove this method.
  static void HandleDeprecatedDailyUpdateUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandleTrustedBiddingSignalsUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  // Handles "trustedBiddingSignalsUrl", which is deprecated.
  // TODO(crbug.com/40266734): Remove this method.
  static void HandleDeprecatedTrustedBiddingSignalsUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  static void HandleTrustedBiddingSignalsKeys(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  static void HandlePriorityVector(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  // TODO(crbug.com/41490104): This field is deprecated in favor of
  // "enableBiddingSignalsPrioritization". Remove this function when it's
  // safe to remove the field.
  static void HandleUseBiddingSignalsPrioritization(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  // Handles "renderUrl" for the ads and ad components arrays, which is
  // deprecated.
  // TODO(crbug.com/40266734): Remove this method.
  static void HandleDeprecatedAdsRenderUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  raw_ptr<const GURL> bidding_logic_url_ = nullptr;
  raw_ptr<const GURL> bidding_wasm_helper_url_ = nullptr;
  raw_ptr<const GURL> trusted_bidding_signals_url_ = nullptr;
  raw_ptr<const mojom::BidderWorkletNonSharedParams>
      bidder_worklet_non_shared_params_ = nullptr;
  const raw_ptr<AuctionV8Logger> v8_logger_;
};

// TODO(crbug.com/40270420): Clean up support for deprecated seconds-based
// version after API users migrate.
enum class PrevWinsType { kSeconds, kMilliseconds };

class CONTENT_EXPORT BiddingBrowserSignalsLazyFiller
    : public PersistedLazyFiller {
 public:
  explicit BiddingBrowserSignalsLazyFiller(AuctionV8Helper* v8_helper);

  void ReInitialize(blink::mojom::BiddingBrowserSignals* bidder_browser_signals,
                    base::Time auction_start_time);

  // Returns success/failure.
  bool FillInObject(v8::Local<v8::Object> object);

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

  raw_ptr<blink::mojom::BiddingBrowserSignals> bidder_browser_signals_ =
      nullptr;
  base::Time auction_start_time_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_LAZY_FILLER_H_
