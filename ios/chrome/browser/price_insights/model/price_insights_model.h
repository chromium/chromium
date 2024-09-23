// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_MODEL_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_MODEL_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/commerce/core/commerce_types.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model.h"
#import "url/gurl.h"

namespace commerce {
class ShoppingService;
}  // namespace commerce
class PriceInsightsModelTest;

// In order to have GURL as a key in a hashmap, GURL hashing mechanism is
// needed.
struct GURLHash {
  size_t operator()(const GURL& url) const {
    return std::hash<std::string>()(url.spec());
  }
};

// This struct extends the ContextualPanelItemConfiguration and includes
// additional data specific to Price Insights.
struct PriceInsightsItemConfiguration
    : public ContextualPanelItemConfiguration {
  PriceInsightsItemConfiguration();
  ~PriceInsightsItemConfiguration();
  explicit PriceInsightsItemConfiguration(
      PriceInsightsItemConfiguration* config);

  // Indicates whether price tracking is enabled for the page.
  bool can_price_track = false;
  // Indicates whether the page is already being tracked.
  bool is_subscribed = false;
  // Product info
  std::optional<commerce::ProductInfo> product_info;
  // Price insights info
  std::optional<commerce::PriceInsightsInfo> price_insights_info;
};

// This struct includes the configuration for Price Insights item and flags
// indicating the processing state
struct PriceInsightsExecution {
  PriceInsightsExecution();
  ~PriceInsightsExecution();

  // Configuration for Price Insights
  std::unique_ptr<PriceInsightsItemConfiguration> config;
  // Indicates if price insights info has been processed.
  bool is_price_insights_info_processed = false;
  // Indicates if is_subscribed has been processed.
  bool is_subscribed_processed = false;
};

// Price Insights contextual panel model object responsible for managing Price
// Insights functionality.
class PriceInsightsModel final : public ContextualPanelModel,
                                 public KeyedService {
 public:
  PriceInsightsModel();
  PriceInsightsModel(const PriceInsightsModel&) = delete;
  PriceInsightsModel& operator=(const PriceInsightsModel&) = delete;
  ~PriceInsightsModel() override;

  // ContextualPanelModel:
  void FetchConfigurationForWebState(
      web::WebState* web_state,
      FetchConfigurationForWebStateCallback callback) override;

 private:
  friend class PriceInsightsModelTest;

  // Callback function called when product info URL is received.
  void OnProductInfoUrlReceived(
      const GURL& url,
      const std::optional<const commerce::ProductInfo>& info);
  // Callback function called when price insights info URL is received.
  void OnPriceInsightsInfoUrlReceived(
      const GURL& url,
      const std::optional<commerce::PriceInsightsInfo>& info);
  // Runs callbacks associated with a given URL.
  void RunCallbacks(const GURL& url);
  // Callback function called when IsSubscribed returns whether or not the
  // current page is being tracked.
  void OnIsSubscribedReceived(const GURL& url, bool is_subscribed);
  // Check if there are pending executions for the specified URL.
  bool HasPendingExecutions(const GURL& url);
  // Updates PriceInsightsItemConfiguration.
  void UpdatePriceInsightsItemConfig(const GURL& url);
  // Pointer to the shopping service.
  raw_ptr<commerce::ShoppingService> shopping_service_ = nullptr;
  // Map containing Price Insights execution status for each URL.
  std::unordered_map<GURL, std::unique_ptr<PriceInsightsExecution>, GURLHash>
      price_insights_executions_;
  // Map containing callbacks for each URL.
  std::unordered_map<GURL,
                     std::vector<FetchConfigurationForWebStateCallback>,
                     GURLHash>
      callbacks_;
  // Weak pointer.
  base::WeakPtrFactory<PriceInsightsModel> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_MODEL_H_
