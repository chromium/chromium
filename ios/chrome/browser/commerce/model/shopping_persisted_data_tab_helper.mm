// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/shopping_persisted_data_tab_helper.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/optimization_guide/core/optimization_metadata.h"
#import "ios/chrome/browser/commerce/model/price_alert_util.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"

namespace {
const int kUnitsToMicros = 1000000;
const int kMinimumDropThresholdAbsolute = 2 * kUnitsToMicros;
const int kMinimumDropThresholdRelative = 10;
const int kMicrosToTwoDecimalPlaces = 10000;
const int kTwoDecimalPlacesMaximumThreshold = 10 * kUnitsToMicros;
const int kStaleThresholdHours = 1;
const base::TimeDelta kStaleDuration = base::Hours(kStaleThresholdHours);
const base::TimeDelta kActiveTabThreshold = base::Days(1);
const char kTabSwitcherMetricsString[] = "EnterTabSwitcher";
const char kFinishNavigationMetricsString[] = "NavigationComplete";
const char kActiveTabMetricsString[] = "ActiveTab";
const char kStaleTabMetricsString[] = "StaleTab";

// Returns true if a cached price drop has gone stale and should be
// re-fetched from OptimizationGuide.
BOOL IsPriceDropStale(base::Time price_drop_timestamp) {
  return base::Time::Now() - price_drop_timestamp > kStaleDuration;
}

const char* GetLogIdString(PriceDropLogId& log_id) {
  switch (log_id) {
    case TAB_SWITCHER:
      return kTabSwitcherMetricsString;
    case NAVIGATION_COMPLETE:
      return kFinishNavigationMetricsString;
  }
  NOTREACHED_IN_MIGRATION() << "Unknown PriceDropLogId " << log_id;
  return "";
}

const char* GetTabStatusString(base::Time time_last_accessed) {
  if (base::Time::Now() - time_last_accessed < kActiveTabThreshold) {
    return kActiveTabMetricsString;
  } else {
    return kStaleTabMetricsString;
  }
}

}  // namespace

ShoppingPersistedDataTabHelper::~ShoppingPersistedDataTabHelper() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

ShoppingPersistedDataTabHelper::PriceDrop::PriceDrop()
    : current_price(nil),
      previous_price(nil),
      offer_id(std::nullopt),
      url(GURL(std::string())),
      timestamp(base::Time::UnixEpoch()) {}

ShoppingPersistedDataTabHelper::PriceDrop::~PriceDrop() = default;

const ShoppingPersistedDataTabHelper::PriceDrop*
ShoppingPersistedDataTabHelper::GetPriceDrop() {
  if (!IsPriceAlertsEligible(web_state_->GetBrowserState())) {
    return nullptr;
  }
  const GURL& url = web_state_->GetLastCommittedURL().is_valid()
                        ? web_state_->GetLastCommittedURL()
                        : web_state_->GetVisibleURL();
  if (!price_drop_ || price_drop_->url != url ||
      IsPriceDropStale(price_drop_->timestamp)) {
    ResetPriceDrop();
    OptimizationGuideService* optimization_guide_service =
        OptimizationGuideServiceFactory::GetForProfile(
            ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
    if (!optimization_guide_service) {
      return nullptr;
    }
    optimization_guide::OptimizationMetadata metadata;
    if (optimization_guide_service->CanApplyOptimization(
            url, optimization_guide::proto::PRICE_TRACKING, &metadata) !=
        optimization_guide::OptimizationGuideDecision::kTrue) {
      return nullptr;
    }
    ParseProto(url, metadata.ParsedMetadata<commerce::PriceTrackingData>());
  }
  if (price_drop_) {
    return price_drop_.get();
  }
  return nullptr;
}

void ShoppingPersistedDataTabHelper::LogMetrics(PriceDropLogId log_id) {
  const char* tab_status = GetTabStatusString(web_state_->GetLastActiveTime());
  const char* log_id_string = GetLogIdString(log_id);
  base::UmaHistogramBoolean(
      base::StringPrintf("Commerce.PriceDrops.%s%s.ContainsPrice", tab_status,
                         log_id_string),
      price_drop_ && price_drop_->current_price);
  base::UmaHistogramBoolean(
      base::StringPrintf("Commerce.PriceDrops.%s%s.ContainsPriceDrop",
                         tab_status, log_id_string),
      price_drop_ && price_drop_->current_price && price_drop_->previous_price);
  base::UmaHistogramBoolean(
      base::StringPrintf("Commerce.PriceDrops.%s%s.IsProductDetailPage",
                         tab_status, log_id_string),
      price_drop_ && price_drop_->offer_id);
}

ShoppingPersistedDataTabHelper::ShoppingPersistedDataTabHelper(
    web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);

  OptimizationGuideService* optimization_guide_service =
      OptimizationGuideServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));

  if (!optimization_guide_service) {
    return;
  }

  optimization_guide_service->RegisterOptimizationTypes(
      {optimization_guide::proto::PRICE_TRACKING});
}

// static
BOOL ShoppingPersistedDataTabHelper::IsQualifyingPriceDrop(
    int64_t current_price_micros,
    int64_t previous_price_micros) {
  if (previous_price_micros - current_price_micros <
      kMinimumDropThresholdAbsolute) {
    return false;
  }
  if ((100 * current_price_micros) / previous_price_micros >
      (100 - kMinimumDropThresholdRelative)) {
    return false;
  }
  return true;
}

// static
std::u16string ShoppingPersistedDataTabHelper::FormatPrice(
    payments::CurrencyFormatter* currency_formatter,
    long price_micros) {
  currency_formatter->SetMaxFractionalDigits(
      price_micros >= kTwoDecimalPlacesMaximumThreshold ? 0 : 2);
  long twoDecimalPlaces = price_micros / kMicrosToTwoDecimalPlaces;
  std::u16string result = currency_formatter->Format(base::StringPrintf(
      "%s.%s", base::NumberToString(twoDecimalPlaces / 100).c_str(),
      base::NumberToString(twoDecimalPlaces % 100).c_str()));
  return result;
}

void ShoppingPersistedDataTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!IsPriceAlertsEligible(web_state->GetBrowserState())) {
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!navigation_context->GetUrl().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  ResetPriceDrop();
  OptimizationGuideService* optimization_guide_service =
      OptimizationGuideServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state->GetBrowserState()));
  if (!optimization_guide_service) {
    return;
  }

  optimization_guide_service->CanApplyOptimization(
      navigation_context->GetUrl(), optimization_guide::proto::PRICE_TRACKING,
      base::BindOnce(
          &ShoppingPersistedDataTabHelper::OnOptimizationGuideResultReceived,
          weak_factory_.GetWeakPtr(), navigation_context->GetUrl()));
}

void ShoppingPersistedDataTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  web_state->RemoveObserver(this);
  web_state_ = nullptr;
}

void ShoppingPersistedDataTabHelper::OnOptimizationGuideResultReceived(
    const GURL& url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    LogMetrics(NAVIGATION_COMPLETE);
    return;
  }

  ParseProto(url, metadata.ParsedMetadata<commerce::PriceTrackingData>());
  LogMetrics(NAVIGATION_COMPLETE);
}

payments::CurrencyFormatter*
ShoppingPersistedDataTabHelper::GetCurrencyFormatter(
    const std::string& currency_code,
    const std::string& locale_name) {
  // Create a currency formatter for `currency_code`, or if already created
  // return the cached version.
  std::pair<std::map<std::string, payments::CurrencyFormatter>::iterator, bool>
      emplace_result = currency_formatter_map_.emplace(
          std::piecewise_construct, std::forward_as_tuple(currency_code),
          std::forward_as_tuple(currency_code, locale_name));
  return &(emplace_result.first->second);
}

void ShoppingPersistedDataTabHelper::ParseProto(
    const GURL& url,
    const std::optional<commerce::PriceTrackingData>& price_metadata) {
  if (!price_metadata) {
    return;
  }
  // TODO(crbug.com/40205382) Change PriceDrop to PriceData.
  price_drop_ = std::make_unique<PriceDrop>();
  if (price_metadata->has_buyable_product() &&
      price_metadata->buyable_product().has_offer_id()) {
    price_drop_->offer_id = price_metadata->buyable_product().offer_id();
  }
  if (!price_metadata->has_product_update()) {
    return;
  }

  const auto& product_update = price_metadata->product_update();
  if (!product_update.has_old_price() || !product_update.has_new_price()) {
    return;
  }

  if (product_update.old_price().currency_code() !=
      product_update.new_price().currency_code()) {
    return;
  }

  if (!IsQualifyingPriceDrop(product_update.new_price().amount_micros(),
                             product_update.old_price().amount_micros())) {
    return;
  }

  // TODO(crbug.com/40794608) Filter out non-qualifying price drops (< 10% or
  // < 2 units).
  payments::CurrencyFormatter* currencyFormatter =
      GetCurrencyFormatter(product_update.old_price().currency_code(),
                           GetApplicationContext()->GetApplicationLocale());
  price_drop_->current_price = base::SysUTF16ToNSString(FormatPrice(
      currencyFormatter, product_update.new_price().amount_micros()));
  price_drop_->previous_price = base::SysUTF16ToNSString(FormatPrice(
      currencyFormatter, product_update.old_price().amount_micros()));
  price_drop_->url = url;
  price_drop_->timestamp = base::Time::Now();
  if (product_update.has_offer_id()) {
    price_drop_->offer_id = product_update.offer_id();
  }
}

void ShoppingPersistedDataTabHelper::ResetPriceDrop() {
  price_drop_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(ShoppingPersistedDataTabHelper)
