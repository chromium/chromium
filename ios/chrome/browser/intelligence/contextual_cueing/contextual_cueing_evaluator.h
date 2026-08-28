// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_EVALUATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_EVALUATOR_H_

#import <cstddef>
#import <optional>
#import <string>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "components/contextual_cueing/contextual_cueing_enums.h"
#import "components/page_content_annotations/core/page_content_annotation_type.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker.h"
#import "url/gurl.h"

namespace contextual_cueing {

// Evaluates whether a page qualifies for a contextual cue based on on-device
// category classification scores, MIME type, and impression frequency limits,
// 1:1 aligned with Desktop and Clank.
class ContextualCueingEvaluator {
 public:
  struct EvaluationConfig {
    EvaluationConfig();
    ~EvaluationConfig();
    EvaluationConfig(const EvaluationConfig&);
    EvaluationConfig& operator=(const EvaluationConfig&);

    // Category score thresholds (0.0 to 1.0) matching Desktop
    // `kShoppingClassifierThreshold` and `kEduClassifierThreshold`.
    float shopping_threshold = 0.50f;
    float education_threshold = 0.50f;

    // If true, filters out search engine results pages and root homepages
    // (matching Desktop IsUrlEligibleForCue).
    bool filter_search_and_homepages = true;

    // Supported MIME types.
    std::vector<std::string> allowed_mime_types;
  };

  struct EvaluationResult {
    ContextualCueingDecision decision =
        ContextualCueingDecision::kFailedCategoryClassification;
    std::optional<page_content_annotations::Category> top_category;

    bool is_eligible() const {
      return decision == ContextualCueingDecision::kSuccess;
    }
  };

  explicit ContextualCueingEvaluator(ContextualCueingCapTracker* cap_tracker);
  ContextualCueingEvaluator(ContextualCueingCapTracker* cap_tracker,
                            EvaluationConfig config);
  ~ContextualCueingEvaluator();

  // Static helper to check if a URL is eligible (HTTP/HTTPS, not Google Search,
  // not root homepage).
  static bool IsUrlEligibleForCue(const GURL& url);

  // Evaluates fast pre-checks: URL validity, search/homepages, CapTracker
  // cooldowns/spacing, and MIME type.
  ContextualCueingDecision EvaluatePageEligibility(
      const GURL& url,
      const std::string& mime_type = "text/html") const;

  // Evaluates on-device category classification scores against thresholds.
  EvaluationResult EvaluateCategoryScores(
      const std::vector<page_content_annotations::Category>& categories) const;

  // Evaluates the given page signals to decide whether to trigger a contextual
  // cue. Combines page eligibility and category score evaluation.
  EvaluationResult Evaluate(
      const GURL& url,
      const std::vector<page_content_annotations::Category>& categories,
      const std::string& mime_type = "text/html") const;

 private:
  raw_ptr<ContextualCueingCapTracker> cap_tracker_ = nullptr;
  const EvaluationConfig config_;
};

}  // namespace contextual_cueing

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_EVALUATOR_H_
