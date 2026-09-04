// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/optimization_guide_page_classification_service.h"

#import "base/functional/bind.h"
#import "base/time/time.h"
#import "components/optimization_guide/core/hints/optimization_guide_decider.h"
#import "components/optimization_guide/proto/hints.pb.h"
#import "components/optimization_guide/proto/page_entities_metadata.pb.h"
#import "ios/chrome/browser/intelligence/page_classification/education_eligibility_vertical.h"
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
        optimization_guide::OptimizationGuideDecider* opt_guide_decider)
    : opt_guide_decider_(opt_guide_decider) {
  if (opt_guide_decider_) {
    opt_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::PAGE_ENTITIES});
  }
}

OptimizationGuidePageClassificationService::
    ~OptimizationGuidePageClassificationService() = default;

void OptimizationGuidePageClassificationService::Shutdown() {
  active_requests_.clear();
  opt_guide_decider_ = nullptr;
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

  if (!opt_guide_decider_) {
    OnOptimizationGuideDecision(
        web_state->GetWeakPtr(), request_id, url,
        optimization_guide::OptimizationGuideDecision::kFalse,
        optimization_guide::OptimizationMetadata());
    return;
  }

  opt_guide_decider_->CanApplyOptimization(
      url, optimization_guide::proto::PAGE_ENTITIES,
      base::BindOnce(&OptimizationGuidePageClassificationService::
                         OnOptimizationGuideDecision,
                     weak_ptr_factory_.GetWeakPtr(), web_state->GetWeakPtr(),
                     request_id, url));
}

void OptimizationGuidePageClassificationService::CancelClassification(
    web::WebState* web_state) {
  if (!web_state) {
    return;
  }
  active_requests_.erase(web_state->GetUniqueIdentifier());
}

void OptimizationGuidePageClassificationService::CompleteRequest(
    web::WebStateID web_state_id,
    const PageClassificationResult& result) {
  auto it = active_requests_.find(web_state_id);
  if (it == active_requests_.end()) {
    return;
  }
  auto callback = std::move(it->second->callback);
  active_requests_.erase(it);
  std::move(callback).Run(result);
}

void OptimizationGuidePageClassificationService::OnOptimizationGuideDecision(
    base::WeakPtr<web::WebState> web_state,
    uint64_t request_id,
    const GURL& expected_url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (!web_state) {
    return;
  }

  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
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
    CompleteRequest(web_state_id, PageClassificationResult());
    return;
  }

  web::WebFramesManager* frames_manager =
      EducationJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state.get());
  web::WebFrame* main_frame =
      frames_manager ? frames_manager->GetMainWebFrame() : nullptr;
  if (!main_frame) {
    CompleteRequest(web_state_id, PageClassificationResult());
    return;
  }

  EducationJavaScriptFeature::GetInstance()->ExtractDOMFeatures(
      main_frame, kDOMExtractionTimeout,
      base::BindOnce(
          &OptimizationGuidePageClassificationService::OnDOMFeaturesExtracted,
          weak_ptr_factory_.GetWeakPtr(), web_state, request_id, expected_url,
          std::move(page_entities_metadata)));
}

void OptimizationGuidePageClassificationService::OnDOMFeaturesExtracted(
    base::WeakPtr<web::WebState> web_state,
    uint64_t request_id,
    const GURL& expected_url,
    std::optional<optimization_guide::proto::PageEntitiesMetadata>
        page_entities_metadata,
    std::optional<EducationDOMFeatures> dom_features) {
  if (!web_state) {
    return;
  }

  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  auto it = active_requests_.find(web_state_id);
  if (it == active_requests_.end() || it->second->request_id != request_id ||
      web_state->GetLastCommittedURL() != expected_url) {
    return;
  }

  PageClassificationResult result;

  if (page_entities_metadata.has_value()) {
    // 1. Evaluate Education vertical.
    std::optional<float> education_petacat_score =
        EducationEligibilityVertical::GetTopEducationEntityScore(
            *page_entities_metadata);
    if (education_petacat_score.has_value()) {
      float score = 0.0f;
      bool is_eligible = false;
      if (dom_features.has_value()) {
        auto ees_score =
            EducationEligibilityVertical::ComputeEducationEligibilityScore(
                *education_petacat_score, *dom_features);
        if (ees_score.has_value()) {
          score = *ees_score;
          is_eligible = true;
        }
      }
      result.category_results.push_back({
          .category_type = page_content_annotations::CategoryType::kEducation,
          .score = score,
          .is_eligible = is_eligible,
      });
    }
  }

  CompleteRequest(web_state_id, result);
}
