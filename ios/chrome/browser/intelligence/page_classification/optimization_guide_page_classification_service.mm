// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/optimization_guide_page_classification_service.h"

#import "base/functional/bind.h"
#import "base/time/time.h"
#import "components/commerce/core/shopping_service.h"
#import "components/optimization_guide/core/hints/optimization_guide_decider.h"
#import "components/optimization_guide/proto/hints.pb.h"
#import "components/optimization_guide/proto/page_entities_metadata.pb.h"
#import "ios/chrome/browser/intelligence/page_classification/education_eligibility_vertical.h"
#import "ios/chrome/browser/intelligence/page_classification/education_java_script_feature.h"
#import "ios/chrome/browser/intelligence/page_classification/shopping_eligibility_vertical.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace {

// Maximum time allowed for DOM feature extraction script to execute.
constexpr base::TimeDelta kDOMExtractionTimeout = base::Seconds(2);

}  // namespace

OptimizationGuidePageClassificationService::InFlightRequest::InFlightRequest(
    uint64_t id,
    const GURL& url,
    PageClassificationCallback cb)
    : request_id(id), expected_url(url), callback(std::move(cb)) {}

OptimizationGuidePageClassificationService::InFlightRequest::
    ~InFlightRequest() = default;

OptimizationGuidePageClassificationService::
    OptimizationGuidePageClassificationService(
        optimization_guide::OptimizationGuideDecider* opt_guide_decider,
        commerce::ShoppingService* shopping_service)
    : opt_guide_decider_(opt_guide_decider),
      shopping_service_(shopping_service) {
  if (opt_guide_decider_) {
    opt_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::PAGE_ENTITIES});
  }
}

OptimizationGuidePageClassificationService::
    ~OptimizationGuidePageClassificationService() = default;

void OptimizationGuidePageClassificationService::Shutdown() {
  web_state_observations_.RemoveAllObservations();
  active_requests_.clear();
  opt_guide_decider_ = nullptr;
  shopping_service_ = nullptr;
}

void OptimizationGuidePageClassificationService::WebStateDestroyed(
    web::WebState* web_state) {
  CancelClassification(web_state);
}

void OptimizationGuidePageClassificationService::ClassifyWebState(
    web::WebState* web_state,
    PageClassificationCallback callback) {
  if (!web_state) {
    std::move(callback).Run(PageClassificationResult());
    return;
  }

  const GURL& url = web_state->GetLastCommittedURL();
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(PageClassificationResult());
    return;
  }

  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  uint64_t request_id = ++next_request_id_;
  active_requests_[web_state_id] =
      std::make_unique<InFlightRequest>(request_id, url, std::move(callback));
  active_requests_[web_state_id]->web_state = web_state->GetWeakPtr();

  if (!web_state_observations_.IsObservingSource(web_state)) {
    web_state_observations_.AddObservation(web_state);
  }

  // 1. Initiate Education classification via Optimization Guide decider.
  if (opt_guide_decider_) {
    opt_guide_decider_->CanApplyOptimization(
        url, optimization_guide::proto::PAGE_ENTITIES,
        base::BindOnce(&OptimizationGuidePageClassificationService::
                           OnOptimizationGuideDecision,
                       weak_ptr_factory_.GetWeakPtr(), web_state->GetWeakPtr(),
                       web_state_id, request_id, url));
  } else {
    OnEducationEvaluationComplete(web_state_id, request_id, std::nullopt);
  }

  // 2. Initiate Shopping classification via ShoppingService.
  if (shopping_service_) {
    ShoppingEligibilityVertical::Evaluate(
        shopping_service_, url,
        base::BindOnce(&OptimizationGuidePageClassificationService::
                           OnShoppingEvaluationComplete,
                       weak_ptr_factory_.GetWeakPtr(), web_state_id,
                       request_id));
  } else {
    OnShoppingEvaluationComplete(web_state_id, request_id, std::nullopt);
  }
}

void OptimizationGuidePageClassificationService::CancelClassification(
    web::WebState* web_state) {
  if (!web_state) {
    return;
  }
  active_requests_.erase(web_state->GetUniqueIdentifier());
  if (web_state_observations_.IsObservingSource(web_state)) {
    web_state_observations_.RemoveObservation(web_state);
  }
}

void OptimizationGuidePageClassificationService::CheckRequestCompletion(
    web::WebStateID web_state_id) {
  auto it = active_requests_.find(web_state_id);
  if (it == active_requests_.end()) {
    return;
  }
  if (it->second->education_complete && it->second->shopping_complete) {
    if (it->second->web_state && web_state_observations_.IsObservingSource(
                                     it->second->web_state.get())) {
      web_state_observations_.RemoveObservation(it->second->web_state.get());
    }

    // Discard result if WebState was destroyed or navigated away to a different
    // URL before all evaluations completed.
    if (!it->second->web_state ||
        it->second->web_state->GetLastCommittedURL() !=
            it->second->expected_url) {
      active_requests_.erase(it);
      return;
    }

    PageClassificationResult result = std::move(it->second->result);
    auto callback = std::move(it->second->callback);
    active_requests_.erase(it);
    std::move(callback).Run(std::move(result));
  }
}

void OptimizationGuidePageClassificationService::OnEducationEvaluationComplete(
    web::WebStateID web_state_id,
    uint64_t request_id,
    std::optional<CategoryResult> education_result) {
  auto it = active_requests_.find(web_state_id);
  if (it == active_requests_.end() || it->second->request_id != request_id) {
    return;
  }
  it->second->education_complete = true;
  if (education_result.has_value()) {
    it->second->result.category_results.push_back(*education_result);
  }
  CheckRequestCompletion(web_state_id);
}

void OptimizationGuidePageClassificationService::OnShoppingEvaluationComplete(
    web::WebStateID web_state_id,
    uint64_t request_id,
    std::optional<CategoryResult> shopping_result) {
  auto it = active_requests_.find(web_state_id);
  if (it == active_requests_.end() || it->second->request_id != request_id) {
    return;
  }
  it->second->shopping_complete = true;
  if (shopping_result.has_value()) {
    it->second->result.category_results.push_back(*shopping_result);
  }
  CheckRequestCompletion(web_state_id);
}

void OptimizationGuidePageClassificationService::OnOptimizationGuideDecision(
    base::WeakPtr<web::WebState> web_state,
    web::WebStateID web_state_id,
    uint64_t request_id,
    const GURL& expected_url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (!web_state) {
    auto it = active_requests_.find(web_state_id);
    if (it != active_requests_.end() && it->second->request_id == request_id) {
      active_requests_.erase(it);
    }
    return;
  }

  auto it = active_requests_.find(web_state_id);
  if (it == active_requests_.end() || it->second->request_id != request_id ||
      web_state->GetLastCommittedURL() != expected_url) {
    return;
  }

  std::optional<optimization_guide::proto::PageEntitiesMetadata>
      page_entities_metadata;
  if (decision == optimization_guide::OptimizationGuideDecision::kTrue) {
    auto parsed_metadata =
        metadata
            .ParsedMetadata<optimization_guide::proto::PageEntitiesMetadata>();
    if (parsed_metadata) {
      page_entities_metadata = *parsed_metadata;
    }
  }

  if (!page_entities_metadata.has_value()) {
    OnEducationEvaluationComplete(web_state_id, request_id, std::nullopt);
    return;
  }

  // Synchronously evaluate whether the page matches any approved educational
  // categories or academic entity MIDs before running DOM extraction.
  std::optional<float> education_petacat_score =
      EducationEligibilityVertical::GetTopEducationEntityScore(
          *page_entities_metadata);
  if (!education_petacat_score.has_value()) {
    OnEducationEvaluationComplete(web_state_id, request_id, std::nullopt);
    return;
  }

  web::WebFramesManager* frames_manager =
      EducationJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state.get());
  web::WebFrame* main_frame =
      frames_manager ? frames_manager->GetMainWebFrame() : nullptr;
  if (!main_frame) {
    OnEducationEvaluationComplete(web_state_id, request_id, std::nullopt);
    return;
  }

  EducationJavaScriptFeature::GetInstance()->ExtractDOMFeatures(
      main_frame, kDOMExtractionTimeout,
      base::BindOnce(
          &OptimizationGuidePageClassificationService::OnDOMFeaturesExtracted,
          weak_ptr_factory_.GetWeakPtr(), web_state, web_state_id, request_id,
          expected_url, *education_petacat_score));
}

void OptimizationGuidePageClassificationService::OnDOMFeaturesExtracted(
    base::WeakPtr<web::WebState> web_state,
    web::WebStateID web_state_id,
    uint64_t request_id,
    const GURL& expected_url,
    float education_petacat_score,
    std::optional<EducationDOMFeatures> dom_features) {
  if (!web_state) {
    auto it = active_requests_.find(web_state_id);
    if (it != active_requests_.end() && it->second->request_id == request_id) {
      active_requests_.erase(it);
    }
    return;
  }

  auto it = active_requests_.find(web_state_id);
  if (it == active_requests_.end() || it->second->request_id != request_id ||
      web_state->GetLastCommittedURL() != expected_url) {
    return;
  }

  float score = 0.0f;
  bool is_eligible = false;
  if (dom_features.has_value()) {
    auto ees_score =
        EducationEligibilityVertical::ComputeEducationEligibilityScore(
            education_petacat_score, *dom_features);
    if (ees_score.has_value()) {
      score = *ees_score;
      is_eligible = true;
    }
  }
  CategoryResult edu_result{
      .category_type = page_content_annotations::CategoryType::kEducation,
      .score = score,
      .is_eligible = is_eligible,
  };

  OnEducationEvaluationComplete(web_state_id, request_id, edu_result);
}
