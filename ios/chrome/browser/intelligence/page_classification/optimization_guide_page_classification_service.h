// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_OPTIMIZATION_GUIDE_PAGE_CLASSIFICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_OPTIMIZATION_GUIDE_PAGE_CLASSIFICATION_SERVICE_H_

#import <memory>
#import <optional>

#import "base/containers/flat_map.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "ios/chrome/browser/intelligence/page_classification/education_java_script_feature.h"
#import "ios/chrome/browser/intelligence/page_classification/page_classification_service.h"
#import "ios/web/public/web_state_id.h"
#import "ios/web/public/web_state_observer.h"
#import "url/gurl.h"

namespace commerce {
class ShoppingService;
}  // namespace commerce

namespace optimization_guide {
class OptimizationGuideDecider;
enum class OptimizationGuideDecision;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace web {
class WebState;
}  // namespace web

// Profile-scoped implementation of `PageClassificationService` that evaluates
// vertical page classification using OptimizationGuide metadata (Petacat
// taxonomy and Knowledge Graph MIDs) combined with isolated-world DOM
// structural heuristics (word count and heading count) and commerce signals.
class OptimizationGuidePageClassificationService
    : public PageClassificationService,
      public web::WebStateObserver {
 public:
  OptimizationGuidePageClassificationService(
      optimization_guide::OptimizationGuideDecider* opt_guide_decider,
      commerce::ShoppingService* shopping_service = nullptr);
  ~OptimizationGuidePageClassificationService() override;

  OptimizationGuidePageClassificationService(
      const OptimizationGuidePageClassificationService&) = delete;
  OptimizationGuidePageClassificationService& operator=(
      const OptimizationGuidePageClassificationService&) = delete;

  // KeyedService:
  void Shutdown() override;

  // PageClassificationService:
  void ClassifyWebState(web::WebState* web_state,
                        PageClassificationCallback callback) override;
  void CancelClassification(web::WebState* web_state) override;

  // web::WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  // Tracks the pending state of an asynchronous classification request for a
  // WebState while awaiting OptimizationGuide decisions and DOM feature
  // extraction results.
  struct InFlightRequest {
    InFlightRequest(uint64_t id,
                    const GURL& url,
                    PageClassificationCallback cb);
    ~InFlightRequest();
    InFlightRequest(const InFlightRequest&) = delete;
    InFlightRequest& operator=(const InFlightRequest&) = delete;

    // Unique monotonically increasing request identifier to disambiguate
    // multiple requests for the same WebState.
    uint64_t request_id = 0;

    // The committed URL at the time classification was initiated. Used to guard
    // against ABA navigation races (drops results if the WebState navigated
    // away before async processing finished).
    GURL expected_url;

    // Callback to execute once classification completes or fails.
    PageClassificationCallback callback;

    // Accumulated classification result from each vertical.
    PageClassificationResult result;

    // Track completion of parallel vertical evaluations.
    bool education_complete = false;
    bool shopping_complete = false;

    // Weak pointer to the WebState being classified.
    base::WeakPtr<web::WebState> web_state;
  };

  // Checks whether all vertical evaluations for `web_state_id` are complete,
  // and if so, delivers `result` via callback and removes the request.
  void CheckRequestCompletion(web::WebStateID web_state_id);

  void OnEducationEvaluationComplete(
      web::WebStateID web_state_id,
      uint64_t request_id,
      std::optional<CategoryResult> education_result);

  void OnShoppingEvaluationComplete(
      web::WebStateID web_state_id,
      uint64_t request_id,
      std::optional<CategoryResult> shopping_result);

  void OnOptimizationGuideDecision(
      base::WeakPtr<web::WebState> web_state,
      web::WebStateID web_state_id,
      uint64_t request_id,
      const GURL& expected_url,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  void OnDOMFeaturesExtracted(base::WeakPtr<web::WebState> web_state,
                              web::WebStateID web_state_id,
                              uint64_t request_id,
                              const GURL& expected_url,
                              float education_petacat_score,
                              std::optional<EducationDOMFeatures> dom_features);

  raw_ptr<optimization_guide::OptimizationGuideDecider> opt_guide_decider_ =
      nullptr;
  raw_ptr<commerce::ShoppingService> shopping_service_ = nullptr;
  uint64_t next_request_id_ = 0;
  base::flat_map<web::WebStateID, std::unique_ptr<InFlightRequest>>
      active_requests_;
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};

  base::WeakPtrFactory<OptimizationGuidePageClassificationService>
      weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_OPTIMIZATION_GUIDE_PAGE_CLASSIFICATION_SERVICE_H_
