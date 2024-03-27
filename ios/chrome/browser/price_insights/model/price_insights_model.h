// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_MODEL_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_MODEL_H_

#import "components/commerce/core/commerce_types.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model.h"

struct PriceInsightsItemConfiguration
    : public ContextualPanelItemConfiguration {
  bool can_price_track;
  bool is_subscribed;
  commerce::ProductInfo product_info;
  commerce::PriceInsightsInfo price_insights_info;
};

// Contextual panel model object for Price Insights
class PriceInsightsModel : public ContextualPanelModel, public KeyedService {
 public:
  PriceInsightsModel();
  PriceInsightsModel(const PriceInsightsModel&) = delete;
  PriceInsightsModel& operator=(const PriceInsightsModel&) = delete;
  ~PriceInsightsModel() override;

  // ContextualPanelModel:
  void FetchConfigurationForWebState(
      web::WebState* web_state,
      FetchConfigurationForWebStateCallback callback) override;
};

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_MODEL_H_
