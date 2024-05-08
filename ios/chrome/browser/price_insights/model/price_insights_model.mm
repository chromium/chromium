// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/model/price_insights_model.h"

#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/commerce/core/subscriptions/subscriptions_storage.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/web/public/web_state.h"

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
  price_insights_executions_[product_url]->config =
      std::make_unique<PriceInsightsItemConfiguration>();

  shopping_service_ = commerce::ShoppingServiceFactory::GetForBrowserState(
      web_state->GetBrowserState());
  shopping_service_->GetProductInfoForUrl(
      product_url, base::BindOnce(&PriceInsightsModel::OnProductInfoUrlReceived,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void PriceInsightsModel::OnProductInfoUrlReceived(
    const GURL& url,
    const std::optional<const commerce::ProductInfo>& info) {
  if (!info.has_value()) {
    price_insights_executions_[url]->is_subscribed_processed = true;
    price_insights_executions_[url]->is_price_insights_info_processed = true;
    RunCallbacks(url, false);
    return;
  }

  price_insights_executions_[url]->config->product_info = info.value();

  // Request price insights info only if a valid product info was received.
  shopping_service_->GetPriceInsightsInfoForUrl(
      url, base::BindOnce(&PriceInsightsModel::OnPriceInsightsInfoUrlReceived,
                          weak_ptr_factory_.GetWeakPtr()));

  bool can_track_price = CanTrackPrice(info);
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
}

void PriceInsightsModel::OnPriceInsightsInfoUrlReceived(
    const GURL& url,
    const std::optional<commerce::PriceInsightsInfo>& info) {
  if (info.has_value()) {
    price_insights_executions_[url]->config->price_insights_info = info.value();
  }
  price_insights_executions_[url]->is_price_insights_info_processed = true;

  RunCallbacks(url, true);
}

void PriceInsightsModel::OnIsSubscribedReceived(const GURL& url,
                                                bool is_subscribed) {
  price_insights_executions_[url]->config->is_subscribed = is_subscribed;
  price_insights_executions_[url]->is_subscribed_processed = true;

  RunCallbacks(url, true);
}

void PriceInsightsModel::RunCallbacks(const GURL& url, bool with_valid_config) {
  if (HasPendingExecutions(url)) {
    return;
  }

  auto callbacks_it = callbacks_.find(url);
  DCHECK(callbacks_it != callbacks_.end());
  auto execution_it = price_insights_executions_.find(url);

  for (FetchConfigurationForWebStateCallback& callback : callbacks_it->second) {
    if (with_valid_config) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         std::make_unique<PriceInsightsItemConfiguration>(
                             execution_it->second->config.get())));
    } else {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    }
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
  image_type = config->image_type;
  relevance = config->relevance;
}

PriceInsightsExecution::PriceInsightsExecution() = default;

PriceInsightsExecution::~PriceInsightsExecution() = default;
