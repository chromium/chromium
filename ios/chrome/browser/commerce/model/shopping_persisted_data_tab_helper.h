// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_MODEL_SHOPPING_PERSISTED_DATA_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_COMMERCE_MODEL_SHOPPING_PERSISTED_DATA_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#include <map>
#include <optional>

#import "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#import "components/commerce/core/commerce_types.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/payments/core/currency_formatter.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// Price drop data is logged at different stages during the user's session.
// These identifiers enable differentiation in the metrics based on when
// the price drop data was logged.
// These values are used in histograms.xml. If additional values are
// added, LocationIdentifier in histograms.xml will need to be updated.
enum PriceDropLogId {
  // Price Drop data is logged when the user opens the Tab Switcher.
  TAB_SWITCHER,
  // Price Drop data is logged at the end of a navigation.
  NAVIGATION_COMPLETE
};

// This class acquires pricing data corresponding to the WebState's
// URL - should the URL be for a shopping website with an offer.
class ShoppingPersistedDataTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<ShoppingPersistedDataTabHelper> {
 public:
  ~ShoppingPersistedDataTabHelper() override;

  // Reduction in price for the offer corresponding to the
  // ShoppingPersistedDataTabHelper::WebState::URL - if it exists.
  class PriceDrop {
   public:
    PriceDrop();
    ~PriceDrop();

    // Current price of the offer.
    NSString* current_price;
    // Previous price of the offer.
    NSString* previous_price;

   private:
    friend class ShoppingPersistedDataTabHelper;
    friend class BaseGridMediatorWithPriceDropIndicatorsTest;
    // Offer ID for the price drop
    std::optional<int64_t> offer_id;
    // URL corresponding to the price drop.
    GURL url;
    // Time price drop was acquired.
    base::Time timestamp;
  };

  // Return PriceDrop for the web::WebState corresponding to the
  // ShoppingPersistedDataTabHelper.
  void GetPriceDrop(
      base::OnceCallback<void(std::optional<PriceDrop>)> callback);

  // Callback when ProductInfo is re-fetched for reasons such as the
  // WebState's URL changing.
  void OnFetchProductInfo(
      base::OnceCallback<void(std::optional<PriceDrop>)> callback,
      const GURL& url,
      const std::optional<const commerce::ProductInfo>& info);

  // Log metrics for a given `price_drop_log_id`
  void LogMetrics(PriceDropLogId price_drop_log_id);

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  friend class web::WebStateUserData<ShoppingPersistedDataTabHelper>;
  friend class ShoppingPersistedDataTabHelperTest;
  friend class BaseGridMediatorWithPriceDropIndicatorsTest;

  explicit ShoppingPersistedDataTabHelper(web::WebState* web_state);

  // Not all price drops should be made available - only when there is an
  // absolute price drop of 2 units or more and a relative price drop greater
  // than 10%.
  static BOOL IsQualifyingPriceDrop(int64_t current_price_micros,
                                    int64_t previous_price_micros);

  // Converts price from micros to a string according to the
  // `currency_formatter` currency code and locale.
  static std::u16string FormatPrice(
      payments::CurrencyFormatter* currency_formatter,
      long price_micros);

  // True if price drop is greater than $2 and the relative
  // drop is greater than 10%.
  static bool HasQualifiedPriceDrop(
      const std::optional<const commerce::ProductInfo>& info);

  // Create a PriceDrop object using the data returned from ShoppingService
  static std::unique_ptr<ShoppingPersistedDataTabHelper::PriceDrop>
  CreatePriceDrop(const commerce::ProductInfo& info,
                  const GURL& url,
                  payments::CurrencyFormatter* currencyFormatter);

  // web::WebStateObserver overrides:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  void OnProductInfoReceived(
      const GURL& url,
      const std::optional<const commerce::ProductInfo>& info);

  // Acquires payments::CurrencyFormatter from `currency_formatter_map_`
  payments::CurrencyFormatter* GetCurrencyFormatter(
      const std::string& currency_code,
      const std::string& locale_name);

  // Resets `price_drop_` when it is determined to no longer be valid.
  void ResetPriceDrop();

  // Sets the `price_drop_`. Only intended for testing purposes.
  void SetPriceDropForTesting(std::unique_ptr<PriceDrop> price_drop) {
    price_drop_ = std::move(price_drop);
  }

  raw_ptr<web::WebState> web_state_ = nullptr;

  // Caches payments::CurrencyFormatters per currency code
  std::map<std::string, payments::CurrencyFormatter> currency_formatter_map_;
  // Most recently seen price drop for the web::WebState, if any.
  std::unique_ptr<PriceDrop> price_drop_;

  base::WeakPtrFactory<ShoppingPersistedDataTabHelper> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_COMMERCE_MODEL_SHOPPING_PERSISTED_DATA_TAB_HELPER_H_
