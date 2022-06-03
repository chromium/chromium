// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_SHOPPING_PERSISTED_DATA_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_COMMERCE_SHOPPING_PERSISTED_DATA_TAB_HELPER_H_

#import <Foundation/Foundation.h>
#include <map>

#include "base/time/time.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/payments/core/currency_formatter.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// This class acquires pricing data corresponding to the WebState's
// URL - should the URL be for a shopping website with an offer.
class ShoppingPersistedDataTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<ShoppingPersistedDataTabHelper> {
 public:
  ~ShoppingPersistedDataTabHelper() override;

  static void CreateForWebState(web::WebState* web_state);

  // Reduction in price for the offer corresponding to the
  // ShoppingPersistedDataTabHelper::WebState::URL - if it exists.
  class PriceDrop {
   public:
    PriceDrop()
        : current_price(nil),
          previous_price(nil),
          url(GURL(std::string())),
          timestamp(base::Time::UnixEpoch()) {}

    // Current price of the offer.
    NSString* current_price;
    // Previous price of the offer.
    NSString* previous_price;

   private:
    friend class ShoppingPersistedDataTabHelper;
    // URL corresponding to the price drop.
    GURL url;
    // Time price drop was acquired.
    base::Time timestamp;
  };

  // Return PriceDrop for the web::WebState corresponding to the
  // ShoppingPersistedDataTabHelper.
  const PriceDrop* GetPriceDrop();

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  friend class web::WebStateUserData<ShoppingPersistedDataTabHelper>;
  friend class ShoppingPersistedDataTabHelperTest;

  explicit ShoppingPersistedDataTabHelper(web::WebState* web_state);

  // Not all price drops should be made available - only when there is an
  // absolute price drop of 2 units or more and a relative price drop greater
  // than 10%.
  static BOOL IsQualifyingPriceDrop(int64_t current_price_micros,
                                    int64_t previous_price_micros);

  // Converts price from micros to a string according to the
  // |currency_formatter| currency code and locale.
  static std::u16string FormatPrice(
      payments::CurrencyFormatter* currency_formatter,
      long price_micros);

  // web::WebStateObserver overrides:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Callback from OptimizationGuide with PriceTrackingData proto.
  void OnOptimizationGuideResultReceived(
      const GURL& url,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Acquires payments::CurrencyFormatter from |currency_formatter_map_|
  payments::CurrencyFormatter* GetCurrencyFormatter(
      const std::string& currency_code,
      const std::string& locale_name);

  // Parses the proto acquired from OptimizationGuide and stores in
  // |price_drop_|.
  void ParseProto(
      const GURL& url,
      const absl::optional<commerce::PriceTrackingData>& price_metadata);

  // Resets |price_drop_| when it is determined to no longer be valid.
  void ResetPriceDrop();

  web::WebState* web_state_ = nullptr;

  // Caches payments::CurrencyFormatters per currency code
  std::map<std::string, payments::CurrencyFormatter> currency_formatter_map_;
  // Most recently seen price drop for the web::WebState, if any.
  std::unique_ptr<PriceDrop> price_drop_;

  WEB_STATE_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<ShoppingPersistedDataTabHelper> weak_factory_{this};
};
#endif  // IOS_CHROME_BROWSER_COMMERCE_SHOPPING_PERSISTED_DATA_TAB_HELPER_H_
