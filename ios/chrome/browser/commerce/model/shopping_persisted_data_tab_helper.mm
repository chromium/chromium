// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/shopping_persisted_data_tab_helper.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/commerce/core/shopping_service.h"
#import "ios/chrome/browser/commerce/model/price_alert_util.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
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
// re-fetched from ShoppingService.
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
  NOTREACHED() << "Unknown PriceDropLogId " << log_id;
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

void ShoppingPersistedDataTabHelper::GetPriceDrop(
    base::OnceCallback<void(std::optional<PriceDrop>)> callback) {
  if (!IsPriceAlertsEligibleForWebState(web_state_)) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  const GURL& url = web_state_->GetLastCommittedURL().is_valid()
                        ? web_state_->GetLastCommittedURL()
                        : web_state_->GetVisibleURL();
  if (!price_drop_ || price_drop_->url != url ||
      IsPriceDropStale(price_drop_->timestamp)) {
    ResetPriceDrop();
    commerce::ShoppingService* shopping_service =
        commerce::ShoppingServiceFactory::GetForProfile(
            ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
    if (!shopping_service) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    shopping_service->GetProductInfoForUrl(
        url, base::BindOnce(&ShoppingPersistedDataTabHelper::OnFetchProductInfo,
                            weak_factory_.GetWeakPtr(), std::move(callback)));
  } else if (price_drop_) {
    std::move(callback).Run(*price_drop_.get());
  }
}

void ShoppingPersistedDataTabHelper::OnFetchProductInfo(
    base::OnceCallback<void(std::optional<PriceDrop>)> callback,
    const GURL& url,
    const std::optional<const commerce::ProductInfo>& info) {
  if (!info.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  price_drop_ = CreatePriceDrop(
      info.value(), url,
      GetCurrencyFormatter(
          info->currency_code,
          GetApplicationContext()->GetApplicationLocaleStorage()->Get()));
  std::move(callback).Run(*price_drop_.get());
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

// static
bool ShoppingPersistedDataTabHelper::HasQualifiedPriceDrop(
    const std::optional<const commerce::ProductInfo>& info) {
  if (!info->previous_amount_micros.has_value() ||
      info.value().currency_code.empty()) {
    return false;
  }
  if (!IsQualifyingPriceDrop(info.value().amount_micros,
                             info.value().previous_amount_micros.value())) {
    return false;
  }
  return true;
}

// static
std::unique_ptr<ShoppingPersistedDataTabHelper::PriceDrop>
ShoppingPersistedDataTabHelper::CreatePriceDrop(
    const commerce::ProductInfo& info,
    const GURL& url,
    payments::CurrencyFormatter* currencyFormatter) {
  std::unique_ptr<ShoppingPersistedDataTabHelper::PriceDrop> price_drop =
      std::make_unique<ShoppingPersistedDataTabHelper::PriceDrop>();
  if (HasQualifiedPriceDrop(info)) {
    price_drop->current_price = base::SysUTF16ToNSString(
        FormatPrice(currencyFormatter, info.amount_micros));
    price_drop->previous_price = base::SysUTF16ToNSString(
        FormatPrice(currencyFormatter, info.previous_amount_micros.value()));
    price_drop->url = url;
    price_drop->timestamp = base::Time::Now();
  }
  if (info.offer_id.has_value()) {
    price_drop->offer_id = info.offer_id.value();
  }
  return price_drop;
}

void ShoppingPersistedDataTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!IsPriceAlertsEligibleForWebState(web_state)) {
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!navigation_context->GetUrl().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  ResetPriceDrop();
  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
  if (!shopping_service) {
    return;
  }
  shopping_service->GetProductInfoForUrl(
      navigation_context->GetUrl(),
      base::BindOnce(&ShoppingPersistedDataTabHelper::OnProductInfoReceived,
                     weak_factory_.GetWeakPtr()));
}

void ShoppingPersistedDataTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  web_state->RemoveObserver(this);
  web_state_ = nullptr;
}

void ShoppingPersistedDataTabHelper::OnProductInfoReceived(
    const GURL& url,
    const std::optional<const commerce::ProductInfo>& info) {
  if (info.has_value()) {
    price_drop_ = CreatePriceDrop(
        info.value(), url,
        GetCurrencyFormatter(
            info.value().currency_code,
            GetApplicationContext()->GetApplicationLocaleStorage()->Get()));
  }
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

void ShoppingPersistedDataTabHelper::ResetPriceDrop() {
  price_drop_ = nullptr;
}
