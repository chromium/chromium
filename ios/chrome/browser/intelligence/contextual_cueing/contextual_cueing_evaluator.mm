// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_evaluator.h"

#import <algorithm>

#import "base/strings/string_util.h"
#import "components/contextual_cueing/contextual_cueing_utils.h"
#import "components/google/core/common/google_util.h"

namespace contextual_cueing {

#pragma mark - EvaluationConfig

ContextualCueingEvaluator::EvaluationConfig::EvaluationConfig()
    : allowed_mime_types{"text/html", "text/plain"} {}

ContextualCueingEvaluator::EvaluationConfig::~EvaluationConfig() = default;

ContextualCueingEvaluator::EvaluationConfig::EvaluationConfig(
    const EvaluationConfig&) = default;

ContextualCueingEvaluator::EvaluationConfig&
ContextualCueingEvaluator::EvaluationConfig::operator=(
    const EvaluationConfig&) = default;

#pragma mark - ContextualCueingEvaluator

ContextualCueingEvaluator::ContextualCueingEvaluator(
    ContextualCueingCapTrackerService* cap_tracker_service)
    : ContextualCueingEvaluator(cap_tracker_service, EvaluationConfig()) {}

ContextualCueingEvaluator::ContextualCueingEvaluator(
    ContextualCueingCapTrackerService* cap_tracker_service,
    EvaluationConfig config)
    : cap_tracker_service_(cap_tracker_service), config_(std::move(config)) {}

ContextualCueingEvaluator::~ContextualCueingEvaluator() = default;

// static
bool ContextualCueingEvaluator::IsUrlEligibleForCue(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  if (google_util::IsGoogleSearchUrl(url) || IsHomepageUrl(url)) {
    return false;
  }
  return true;
}

ContextualCueingDecision ContextualCueingEvaluator::EvaluatePageEligibility(
    const GURL& url,
    const std::string& mime_type) const {
  // URL eligibility check.
  if (!url.SchemeIsHTTPOrHTTPS() ||
      (config_.filter_search_and_homepages && !IsUrlEligibleForCue(url))) {
    return ContextualCueingDecision::kUrlNotEligible;
  }

  // Frequency caps and backoff cooldowns (matching Desktop CanShowCue).
  if (cap_tracker_service_) {
    ContextualCueingDecision cap_decision =
        cap_tracker_service_->CanShowNudge(url);
    if (cap_decision != ContextualCueingDecision::kSuccess) {
      return cap_decision;
    }
  }

  // MIME type check.
  if (!config_.allowed_mime_types.empty()) {
    bool mime_allowed = false;
    for (const auto& allowed : config_.allowed_mime_types) {
      if (base::StartsWith(mime_type, allowed,
                           base::CompareCase::INSENSITIVE_ASCII)) {
        mime_allowed = true;
        break;
      }
    }
    if (!mime_allowed) {
      return ContextualCueingDecision::kUrlNotEligible;
    }
  }

  return ContextualCueingDecision::kSuccess;
}

ContextualCueingEvaluator::EvaluationResult
ContextualCueingEvaluator::EvaluateCategoryScores(
    const std::vector<page_content_annotations::Category>& categories) const {
  EvaluationResult result;

  // Category classification and threshold check (1:1 with Desktop
  // `GlicCueTarget`).
  if (categories.empty()) {
    result.decision = ContextualCueingDecision::kFailedCategoryClassification;
    return result;
  }

  bool passes_edu = false;
  bool passes_shopping = false;
  std::optional<page_content_annotations::Category> best_eligible_category;
  for (const auto& category : categories) {
    if (category.category_type ==
            page_content_annotations::CategoryType::kEducation &&
        category.score > config_.education_threshold) {
      passes_edu = true;
      if (!best_eligible_category ||
          category.score > best_eligible_category->score) {
        best_eligible_category = category;
      }
    } else if (category.category_type ==
                   page_content_annotations::CategoryType::kShopping &&
               category.score > config_.shopping_threshold) {
      passes_shopping = true;
      if (!best_eligible_category ||
          category.score > best_eligible_category->score) {
        best_eligible_category = category;
      }
    }
  }

  if (!(passes_edu || passes_shopping)) {
    result.decision = ContextualCueingDecision::kFailedCategoryClassification;
    return result;
  }

  result.top_category = best_eligible_category;
  result.decision = ContextualCueingDecision::kSuccess;
  return result;
}

ContextualCueingEvaluator::EvaluationResult ContextualCueingEvaluator::Evaluate(
    const GURL& url,
    const std::vector<page_content_annotations::Category>& categories,
    const std::string& mime_type) const {
  EvaluationResult result;

  ContextualCueingDecision page_decision =
      EvaluatePageEligibility(url, mime_type);
  if (page_decision != ContextualCueingDecision::kSuccess) {
    result.decision = page_decision;
    return result;
  }

  return EvaluateCategoryScores(categories);
}

}  // namespace contextual_cueing
