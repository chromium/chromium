// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_EDUCATION_ELIGIBILITY_VERTICAL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_EDUCATION_ELIGIBILITY_VERTICAL_H_

#import <optional>
#import <string_view>
#import <vector>

#import "base/containers/span.h"
#import "ios/chrome/browser/intelligence/page_classification/education_java_script_feature.h"

namespace optimization_guide::proto {
class PageEntitiesMetadata;
}  // namespace optimization_guide::proto

// Default minimum Petacat category confidence required for Education vertical.
inline constexpr float kDefaultEducationMinPetacatConfidence = 0.65f;

// Minimum readable words required for Education vertical gating.
inline constexpr int kMinEducationWordCount = 250;

// Minimum heading elements required for Education vertical gating.
inline constexpr int kMinEducationHeadingCount = 1;

// Saturation word count for normalized word density.
inline constexpr float kEducationWordCountSaturation = 1000.0f;

// Saturation heading count for normalized heading density.
inline constexpr float kEducationHeadingCountSaturation = 5.0f;

// Weight applied to word count in Education Content Score (ECS).
inline constexpr float kEducationWordWeight = 0.7f;

// Weight applied to heading count in Education Content Score (ECS).
inline constexpr float kEducationHeadingWeight = 0.3f;

// Default minimum combined Education Eligibility Score (EES = Petacat * ECS).
inline constexpr float kDefaultMinEducationEligibilityScore = 0.50f;

// Evaluates Education page eligibility (Petacat category and
// DOM heuristics with score fusion).
class EducationEligibilityVertical {
 public:
  EducationEligibilityVertical() = delete;

  // Evaluates `metadata` (both categories and entities) against approved
  // taxonomy prefixes and entity MIDs supplied by the provider, and returns the
  // highest confidence score among all matching educational entries, or
  // std::nullopt if none meet `min_confidence`.
  static std::optional<float> GetTopEducationEntityScore(
      const optimization_guide::proto::PageEntitiesMetadata& metadata,
      float min_confidence = kDefaultEducationMinPetacatConfidence);

  // Computes the combined Education Eligibility Score (EES = Petacat * ECS).
  // Returns std::nullopt if DOM readability preconditions (>= 250 words, >= 1
  // heading) fail, or if the fused score is below `min_score`.
  static std::optional<float> ComputeEducationEligibilityScore(
      float petacat_score,
      const EducationDOMFeatures& dom_features,
      float min_score = kDefaultMinEducationEligibilityScore);
};

// Scoped helper for overriding approved category prefixes and academic entity
// MIDs in unit tests.
class ScopedEducationCategoriesForTesting {
 public:
  ScopedEducationCategoriesForTesting(
      base::span<const std::string_view> category_prefixes,
      base::span<const std::string_view> academic_mids);
  ~ScopedEducationCategoriesForTesting();

  ScopedEducationCategoriesForTesting(
      const ScopedEducationCategoriesForTesting&) = delete;
  ScopedEducationCategoriesForTesting& operator=(
      const ScopedEducationCategoriesForTesting&) = delete;

 private:
  std::vector<std::string_view> previous_prefixes_;
  std::vector<std::string_view> previous_mids_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_EDUCATION_ELIGIBILITY_VERTICAL_H_
