// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_PAGE_CLASSIFICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_PAGE_CLASSIFICATION_SERVICE_H_

#import <optional>
#import <vector>

#import "base/functional/callback.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/page_content_annotations/core/page_content_annotation_type.h"

namespace web {
class WebState;
}  // namespace web

// Result for a single classified category or vertical.
struct CategoryResult {
  // The classified category type.
  page_content_annotations::CategoryType category_type =
      page_content_annotations::CategoryType::kEducation;

  // Confidence or eligibility score in [0.0, 1.0].
  float score = 0.0f;

  // Whether the page meets all criteria and qualifies for contextual cues in
  // this category.
  bool is_eligible = false;

  bool operator==(const CategoryResult& other) const = default;
};

// Result struct holding classification outcomes from page classification
// services across evaluated categories/verticals.
struct PageClassificationResult {
  // Classification outcomes for all evaluated categories / verticals.
  std::vector<CategoryResult> category_results;

  // Returns the classification result for `type`, if evaluated.
  std::optional<CategoryResult> GetResultForCategory(
      page_content_annotations::CategoryType type) const {
    for (const auto& result : category_results) {
      if (result.category_type == type) {
        return result;
      }
    }
    return std::nullopt;
  }

  // Returns whether the page is eligible for `type`.
  bool IsEligibleForCategory(
      page_content_annotations::CategoryType type) const {
    auto result = GetResultForCategory(type);
    return result.has_value() && result->is_eligible;
  }

  bool operator==(const PageClassificationResult& other) const = default;
};

using PageClassificationCallback =
    base::OnceCallback<void(const PageClassificationResult& result)>;

// Abstract base interface for page classification services.
// Both on-device classifiers and vertical/OptimizationGuide
// heuristic classifiers implement this interface.
class PageClassificationService : public KeyedService {
 public:
  ~PageClassificationService() override = default;

  // Asynchronously evaluates classification for `web_state`.
  virtual void ClassifyWebState(web::WebState* web_state,
                                PageClassificationCallback callback) = 0;

  // Cancels any in-flight classification request for `web_state`.
  virtual void CancelClassification(web::WebState* web_state) = 0;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_PAGE_CLASSIFICATION_SERVICE_H_
