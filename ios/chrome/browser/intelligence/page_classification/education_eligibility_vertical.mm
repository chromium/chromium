// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/education_eligibility_vertical.h"

#import <algorithm>
#import <string_view>
#import <utility>
#import <vector>

#import "base/no_destructor.h"
#import "base/strings/string_util.h"
#import "components/optimization_guide/proto/page_entities_metadata.pb.h"
#import "ios/public/provider/chrome/browser/intelligence/page_classification_api.h"

namespace {

std::vector<std::string_view>& GetTestingCategoryPrefixes() {
  static base::NoDestructor<std::vector<std::string_view>> prefixes;
  return *prefixes;
}

std::vector<std::string_view>& GetTestingAcademicMIDs() {
  static base::NoDestructor<std::vector<std::string_view>> mids;
  return *mids;
}

bool IsEligibleCategoryInternal(
    std::string_view category_id,
    float category_score,
    base::span<const std::string_view> education_category_prefixes,
    float min_confidence) {
  if (category_score < min_confidence) {
    return false;
  }
  for (std::string_view prefix : education_category_prefixes) {
    if (base::StartsWith(category_id, prefix)) {
      return true;
    }
  }
  return false;
}

bool IsEligibleEntityInternal(std::string_view entity_id,
                              float entity_score,
                              base::span<const std::string_view> academic_mids,
                              float min_confidence) {
  if (entity_score < min_confidence) {
    return false;
  }
  for (std::string_view mid : academic_mids) {
    if (entity_id == mid) {
      return true;
    }
  }
  return false;
}

std::optional<float> ComputeEducationContentScoreInternal(
    const EducationDOMFeatures& features) {
  if (features.word_count < kMinEducationWordCount ||
      features.heading_count < kMinEducationHeadingCount) {
    return std::nullopt;
  }

  const float word_norm =
      std::min(1.0f, static_cast<float>(features.word_count) /
                         kEducationWordCountSaturation);
  const float heading_norm =
      std::min(1.0f, static_cast<float>(features.heading_count) /
                         kEducationHeadingCountSaturation);

  return (kEducationWordWeight * word_norm) +
         (kEducationHeadingWeight * heading_norm);
}

}  // namespace

ScopedEducationCategoriesForTesting::ScopedEducationCategoriesForTesting(
    base::span<const std::string_view> category_prefixes,
    base::span<const std::string_view> academic_mids)
    : previous_prefixes_(std::move(GetTestingCategoryPrefixes())),
      previous_mids_(std::move(GetTestingAcademicMIDs())) {
  GetTestingCategoryPrefixes() = std::vector<std::string_view>(
      category_prefixes.begin(), category_prefixes.end());
  GetTestingAcademicMIDs() =
      std::vector<std::string_view>(academic_mids.begin(), academic_mids.end());
}

ScopedEducationCategoriesForTesting::~ScopedEducationCategoriesForTesting() {
  GetTestingCategoryPrefixes() = std::move(previous_prefixes_);
  GetTestingAcademicMIDs() = std::move(previous_mids_);
}

std::optional<float> EducationEligibilityVertical::GetTopEducationEntityScore(
    const optimization_guide::proto::PageEntitiesMetadata& metadata,
    float min_confidence) {
  std::vector<std::string_view> provider_prefixes;
  base::span<const std::string_view> category_prefixes;
  if (!GetTestingCategoryPrefixes().empty()) {
    category_prefixes = GetTestingCategoryPrefixes();
  } else {
    provider_prefixes = ios::provider::GetEducationCategoryPrefixes();
    category_prefixes = provider_prefixes;
  }

  std::vector<std::string_view> provider_mids;
  base::span<const std::string_view> academic_mids;
  if (!GetTestingAcademicMIDs().empty()) {
    academic_mids = GetTestingAcademicMIDs();
  } else {
    provider_mids = ios::provider::GetAcademicEntityMIDs();
    academic_mids = provider_mids;
  }

  float highest_score = 0.0f;
  bool found_match = false;

  // Evaluate categorized topics (scores in range [0.0, 1.0]).
  for (const auto& category : metadata.categories()) {
    if (IsEligibleCategoryInternal(category.category_id(), category.score(),
                                   category_prefixes, min_confidence)) {
      found_match = true;
      highest_score = std::max(highest_score, category.score());
    }
  }

  // Evaluate recognized entities (scores in range [0, 100]).
  for (const auto& entity : metadata.entities()) {
    float entity_score = static_cast<float>(entity.score()) / 100.0f;
    if (IsEligibleEntityInternal(entity.entity_id(), entity_score,
                                 academic_mids, min_confidence)) {
      found_match = true;
      highest_score = std::max(highest_score, entity_score);
    }
  }

  if (!found_match) {
    return std::nullopt;
  }
  return highest_score;
}

std::optional<float>
EducationEligibilityVertical::ComputeEducationEligibilityScore(
    float petacat_score,
    const EducationDOMFeatures& dom_features,
    float min_score) {
  if (petacat_score <= 0.0f) {
    return std::nullopt;
  }

  const auto content_score = ComputeEducationContentScoreInternal(dom_features);
  if (!content_score.has_value()) {
    return std::nullopt;
  }

  const float combined_score = petacat_score * content_score.value();
  if (combined_score < min_score) {
    return std::nullopt;
  }

  return combined_score;
}
