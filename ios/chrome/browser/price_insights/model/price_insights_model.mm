// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/model/price_insights_model.h"

#import "components/commerce/core/shopping_service.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
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
    const GURL& product_url,
    const std::optional<const commerce::ProductInfo>& product_info) {
  if (product_info.has_value()) {
    price_insights_executions_[product_url]->config->product_info =
        product_info.value();
    price_insights_executions_[product_url]->is_valid_config = true;
  }

  price_insights_executions_[product_url]->is_product_info_processed = true;
  RunCallbacks(product_url);
}

void PriceInsightsModel::RunCallbacks(const GURL& product_url) {
  if (!price_insights_executions_[product_url]->is_product_info_processed) {
    return;
  }

  auto callbacks_it = callbacks_.find(product_url);
  DCHECK(callbacks_it != callbacks_.end());
  auto execution_it = price_insights_executions_.find(product_url);
  for (FetchConfigurationForWebStateCallback& callback : callbacks_it->second) {
    if (execution_it->second->is_valid_config) {
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

PriceInsightsItemConfiguration::PriceInsightsItemConfiguration() = default;

PriceInsightsItemConfiguration::~PriceInsightsItemConfiguration() = default;

PriceInsightsItemConfiguration::PriceInsightsItemConfiguration(
    PriceInsightsItemConfiguration* config)
    : can_price_track(config->can_price_track),
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
