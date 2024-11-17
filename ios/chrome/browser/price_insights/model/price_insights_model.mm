// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/model/price_insights_model.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/commerce/core/subscriptions/subscriptions_storage.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/price_insights/model/price_insights_feature.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// The histogram used to record the availability of ProductInfo.
const char kPriceInsightsModelProductInfo[] =
    "IOS.PriceInsights.Model.ProductInfo";

// The histogram used to record the availability of PriceInsightsInfo.
const char kPriceInsightsModelPriceInsightsInfo[] =
    "IOS.PriceInsights.Model.PriceInsightsInfo";

// The histogram used to record if the page can be tracked and user is eligible
// to track.
const char kPriceInsightsModelCanTrack[] = "IOS.PriceInsights.Model.CanTrack";

// The histogram used to record whether or not the page is being tracked.
const char kPriceInsightsModelIsSubscribed[] =
    "IOS.PriceInsights.Model.IsSubscribed";

std::string getHighConfidenceMomentsText() {
  std::string low_price_value = GetLowPriceParamValue();
  if (low_price_value == std::string(kLowPriceParamGoodDealNow)) {
    return l10n_util::GetStringUTF8(IDS_INSIGHTS_ICON_EXPANDED_TEXT_GOOD_DEAL);
  }
  if (low_price_value == std::string(kLowPriceParamSeePriceHistory)) {
    return l10n_util::GetStringUTF8(
        IDS_INSIGHTS_ICON_EXPANDED_TEXT_PRICE_HISTORY);
  }

  return l10n_util::GetStringUTF8(
      IDS_SHOPPING_INSIGHTS_ICON_EXPANDED_TEXT_LOW_PRICE);
}

}  // namespace

PriceInsightsModel::PriceInsightsModel() {}

PriceInsightsModel::~PriceInsightsModel() {}

void PriceInsightsModel::FetchConfigurationForWebState(
    web::WebState* web_state,
    FetchConfigurationForWebStateCallback callback) {
  const GURL& product_url = web_state->GetVisibleURL();
  callbacks_[product_url].push_back(std::move(callback));

  auto it = price_insights_executions_.find(product_url);

  // Avoid making API calls if the map already contains a value for the given
  // URL, which indicates an ongoing request.
  if (it != price_insights_executions_.end()) {
    return;
  }

  // PriceInsightsExecution execution;
  //  Add an empty PriceInsightsItemConfiguration if it doesn't exist in the
  //  configuration map. This will avoid overriding the
  //  PriceInsightsItemConfiguration when multiple API callbacks modify the
  //  config.
  price_insights_executions_[product_url] =
      std::make_unique<PriceInsightsExecution>();

  shopping_service_ = commerce::ShoppingServiceFactory::GetForProfile(
      ProfileIOS::FromBrowserState(web_state->GetBrowserState()));
  shopping_service_->GetProductInfoForUrl(
      product_url, base::BindOnce(&PriceInsightsModel::OnProductInfoUrlReceived,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void PriceInsightsModel::OnProductInfoUrlReceived(
    const GURL& url,
    const std::optional<const commerce::ProductInfo>& info) {
  bool has_valid_info =
      info.has_value() &&
      (!info->title.empty() || !info->product_cluster_title.empty());
  base::UmaHistogramBoolean(kPriceInsightsModelProductInfo, has_valid_info);
  if (!has_valid_info) {
    price_insights_executions_[url]->is_subscribed_processed = true;
    price_insights_executions_[url]->is_price_insights_info_processed = true;
    RunCallbacks(url);
    return;
  }

  price_insights_executions_[url]->config =
      std::make_unique<PriceInsightsItemConfiguration>();

  price_insights_executions_[url]->config->product_info = info.value();

  // Request price insights info only if a valid product info was received.
  shopping_service_->GetPriceInsightsInfoForUrl(
      url, base::BindOnce(&PriceInsightsModel::OnPriceInsightsInfoUrlReceived,
                          weak_ptr_factory_.GetWeakPtr()));

  bool can_track_price =
      shopping_service_->IsShoppingListEligible() && CanTrackPrice(info);
  base::UmaHistogramBoolean(kPriceInsightsModelCanTrack, can_track_price);
  price_insights_executions_[url]->config->can_price_track = can_track_price;
  if (can_track_price && info.value().product_cluster_id.has_value()) {
    uint64_t cluster_id = info.value().product_cluster_id.value();
    shopping_service_->IsSubscribed(
        commerce::BuildUserSubscriptionForClusterId(cluster_id),
        base::BindOnce(&PriceInsightsModel::OnIsSubscribedReceived,
                       weak_ptr_factory_.GetWeakPtr(), url));
    return;
  }

  price_insights_executions_[url]->is_subscribed_processed = true;
  RunCallbacks(url);
}

void PriceInsightsModel::OnPriceInsightsInfoUrlReceived(
    const GURL& url,
    const std::optional<commerce::PriceInsightsInfo>& info) {
  base::UmaHistogramBoolean(kPriceInsightsModelPriceInsightsInfo,
                            info.has_value());
  if (info.has_value()) {
    price_insights_executions_[url]->config->price_insights_info = info.value();
  }
  price_insights_executions_[url]->is_price_insights_info_processed = true;

  RunCallbacks(url);
}

void PriceInsightsModel::OnIsSubscribedReceived(const GURL& url,
                                                bool is_subscribed) {
  base::UmaHistogramBoolean(kPriceInsightsModelIsSubscribed, is_subscribed);
  price_insights_executions_[url]->config->is_subscribed = is_subscribed;
  price_insights_executions_[url]->is_subscribed_processed = true;

  RunCallbacks(url);
}

void PriceInsightsModel::RunCallbacks(const GURL& url) {
  if (HasPendingExecutions(url)) {
    return;
  }

  auto callbacks_it = callbacks_.find(url);
  DCHECK(callbacks_it != callbacks_.end());
  auto execution_it = price_insights_executions_.find(url);

  for (FetchConfigurationForWebStateCallback& callback : callbacks_it->second) {
    UpdatePriceInsightsItemConfig(url);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       execution_it->second->config == nullptr
                           ? nullptr
                           : std::make_unique<PriceInsightsItemConfiguration>(
                                 execution_it->second->config.get())));
  }

  callbacks_.erase(callbacks_it);
  price_insights_executions_.erase(execution_it);
}

bool PriceInsightsModel::HasPendingExecutions(const GURL& url) {
  if (!price_insights_executions_[url]->is_price_insights_info_processed) {
    return true;
  }

  if (!price_insights_executions_[url]->is_subscribed_processed) {
    return true;
  }

  return false;
}

void PriceInsightsModel::UpdatePriceInsightsItemConfig(const GURL& url) {
  auto execution_it = price_insights_executions_.find(url);
  if (!execution_it->second->config) {
    return;
  }

  if (!execution_it->second->config->price_insights_info.has_value() &&
      !execution_it->second->config->can_price_track) {
    execution_it->second->config = nullptr;
    return;
  }

  execution_it->second->config->entrypoint_image_name =
      base::SysNSStringToUTF8(kDownTrendSymbol);
  execution_it->second->config->image_type =
      ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol;
  execution_it->second->config->accessibility_label =
      l10n_util::GetStringUTF8(IDS_PRICE_INSIGHTS_ACCESSIBILITY);
  execution_it->second->config->iph_feature =
      &feature_engagement::kIPHiOSContextualPanelPriceInsightsFeature;
  execution_it->second->config->iph_entrypoint_used_event_name =
      feature_engagement::events::
          kIOSContextualPanelPriceInsightsEntrypointUsed;
  execution_it->second->config->iph_entrypoint_explicitly_dismissed =
      feature_engagement::events::
          kIOSContextualPanelPriceInsightsEntrypointExplicitlyDismissed;

  if (!execution_it->second->config->price_insights_info.has_value()) {
    execution_it->second->config->relevance =
        ContextualPanelItemConfiguration::low_relevance;
    return;
  }

  commerce::PriceInsightsInfo info =
      execution_it->second->config->price_insights_info.value();
  std::string message;
  switch (info.price_bucket) {
    case commerce::PriceBucket::kLowPrice: {
      message = getHighConfidenceMomentsText();
      break;
    }
    case commerce::PriceBucket::kHighPrice: {
      if (!IsPriceInsightsHighPriceEnabled()) {
        execution_it->second->config->relevance =
            ContextualPanelItemConfiguration::low_relevance;
        return;
      }

      execution_it->second->config->entrypoint_image_name =
          base::SysNSStringToUTF8(kUpTrendSymbol);

      if (!execution_it->second->config->can_price_track ||
          execution_it->second->config->is_subscribed) {
        execution_it->second->config->relevance =
            ContextualPanelItemConfiguration::low_relevance;
        return;
      }
      message =
          l10n_util::GetStringUTF8(IDS_INSIGHTS_ICON_PRICE_HIGH_EXPANDED_TEXT);
      break;
    }
    case commerce::PriceBucket::kTypicalPrice: {
      execution_it->second->config->relevance =
          ContextualPanelItemConfiguration::low_relevance;
      return;
    }
    case commerce::PriceBucket::kUnknown: {
      execution_it->second->config->relevance =
          ContextualPanelItemConfiguration::low_relevance;
      return;
    }
  }

  execution_it->second->config->relevance =
      ContextualPanelItemConfiguration::high_relevance;
  execution_it->second->config->accessibility_label = message;
  execution_it->second->config->entrypoint_message = message;
  execution_it->second->config->iph_title = message;
  execution_it->second->config->iph_text =
      l10n_util::GetStringUTF8(IDS_INSIGHTS_RICH_IPH_TEXT);
  execution_it->second->config->iph_image_name = "rich_iph_price_insights";
}

PriceInsightsItemConfiguration::PriceInsightsItemConfiguration()
    : ContextualPanelItemConfiguration(
          ContextualPanelItemType::PriceInsightsItem) {}

PriceInsightsItemConfiguration::~PriceInsightsItemConfiguration() = default;

PriceInsightsItemConfiguration::PriceInsightsItemConfiguration(
    PriceInsightsItemConfiguration* config)
    : ContextualPanelItemConfiguration(
          ContextualPanelItemType::PriceInsightsItem),
      can_price_track(config->can_price_track),
      is_subscribed(config->is_subscribed),
      product_info(config->product_info),
      price_insights_info(config->price_insights_info) {
  entrypoint_message = config->entrypoint_message;
  accessibility_label = config->accessibility_label;
  entrypoint_image_name = config->entrypoint_image_name;
  iph_feature = config->iph_feature;
  iph_entrypoint_used_event_name = config->iph_entrypoint_used_event_name;
  iph_entrypoint_explicitly_dismissed =
      config->iph_entrypoint_explicitly_dismissed;
  image_type = config->image_type;
  relevance = config->relevance;
  iph_title = config->iph_title;
  iph_text = config->iph_text;
  iph_image_name = config->iph_image_name;
}

PriceInsightsExecution::PriceInsightsExecution() = default;

PriceInsightsExecution::~PriceInsightsExecution() = default;
