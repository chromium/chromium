// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/model/price_insights_model.h"

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/web/public/web_state.h"

PriceInsightsModel::PriceInsightsModel() {}

PriceInsightsModel::~PriceInsightsModel() {}

void PriceInsightsModel::FetchConfigurationForWebState(
    web::WebState* web_state,
    FetchConfigurationForWebStateCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), nullptr));
}
