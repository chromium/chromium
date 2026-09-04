// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/education_eligibility_vertical.h"

#import <string_view>

#import "components/optimization_guide/proto/page_entities_metadata.pb.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr std::string_view kTestCategoryPrefixes[] = {
    "/TestEducation",
    "/TestScience",
    "/TestAcademic",
    "/TestReference",
};

constexpr std::string_view kTestAcademicMIDs[] = {
    "/m/test_academic_mid_1",
    "/m/test_academic_mid_2",
    "/m/test_academic_mid_3",
};

}  // namespace

class EducationEligibilityVerticalTest : public PlatformTest {
 public:
  EducationEligibilityVerticalTest() = default;
};

// Tests that empty metadata returns std::nullopt.
TEST_F(EducationEligibilityVerticalTest, TestEmptyMetadata) {
  optimization_guide::proto::PageEntitiesMetadata metadata;
  EXPECT_EQ(std::nullopt,
            EducationEligibilityVertical::GetTopEducationEntityScore(metadata));
}

// Tests that default open-source provider returns nullopt for unconfigured
// categories.
TEST_F(EducationEligibilityVerticalTest, TestDefaultProviderReturnsNullOpt) {
  optimization_guide::proto::PageEntitiesMetadata metadata;
  auto* cat = metadata.add_categories();
  cat->set_category_id("/TestScience/Physics");
  cat->set_score(0.95f);

  EXPECT_EQ(std::nullopt,
            EducationEligibilityVertical::GetTopEducationEntityScore(metadata));
}

// Tests that non-educational categories and MIDs are rejected.
TEST_F(EducationEligibilityVerticalTest, TestNonEducationalCategories) {
  ScopedEducationCategoriesForTesting scoped_categories(kTestCategoryPrefixes,
                                                        kTestAcademicMIDs);
  optimization_guide::proto::PageEntitiesMetadata metadata;
  auto* cat1 = metadata.add_categories();
  cat1->set_category_id("/NonEducational/Apparel");
  cat1->set_score(0.95f);

  auto* cat2 = metadata.add_categories();
  cat2->set_category_id("/NonEducational/Games");
  cat2->set_score(0.90f);

  auto* entity = metadata.add_entities();
  entity->set_entity_id("/m/test_non_academic_mid");
  entity->set_score(99);

  EXPECT_EQ(std::nullopt,
            EducationEligibilityVertical::GetTopEducationEntityScore(metadata));
}

// Tests that educational categories with score below threshold are rejected.
TEST_F(EducationEligibilityVerticalTest, TestBelowConfidenceThreshold) {
  ScopedEducationCategoriesForTesting scoped_categories(kTestCategoryPrefixes,
                                                        kTestAcademicMIDs);
  optimization_guide::proto::PageEntitiesMetadata metadata;
  auto* cat = metadata.add_categories();
  cat->set_category_id("/TestScience/Physics");
  cat->set_score(0.64f);

  EXPECT_EQ(std::nullopt,
            EducationEligibilityVertical::GetTopEducationEntityScore(metadata));
}

// Tests that educational categories meeting confidence threshold are accepted.
TEST_F(EducationEligibilityVerticalTest, TestMatchingCategory) {
  ScopedEducationCategoriesForTesting scoped_categories(kTestCategoryPrefixes,
                                                        kTestAcademicMIDs);
  optimization_guide::proto::PageEntitiesMetadata metadata;
  auto* cat = metadata.add_categories();
  cat->set_category_id("/TestScience/Physics");
  cat->set_score(0.85f);

  auto result =
      EducationEligibilityVertical::GetTopEducationEntityScore(metadata);
  ASSERT_TRUE(result.has_value());
  EXPECT_FLOAT_EQ(0.85f, result.value());
}

// Tests that all approved taxonomy prefixes are recognized.
TEST_F(EducationEligibilityVerticalTest, TestApprovedPrefixes) {
  ScopedEducationCategoriesForTesting scoped_categories(kTestCategoryPrefixes,
                                                        kTestAcademicMIDs);
  const std::string_view kPrefixes[] = {
      "/TestEducation/General",
      "/TestScience/Biology",
      "/TestAcademic/University",
      "/TestReference/Encyclopedia",
  };

  for (std::string_view prefix : kPrefixes) {
    optimization_guide::proto::PageEntitiesMetadata metadata;
    auto* cat = metadata.add_categories();
    cat->set_category_id(std::string(prefix));
    cat->set_score(0.75f);

    auto result =
        EducationEligibilityVertical::GetTopEducationEntityScore(metadata);
    ASSERT_TRUE(result.has_value())
        << "Failed to match approved prefix: " << prefix;
    EXPECT_FLOAT_EQ(0.75f, result.value());
  }
}

// Tests that approved academic Knowledge Graph MIDs are recognized.
TEST_F(EducationEligibilityVerticalTest, TestAcademicEntityMIDs) {
  ScopedEducationCategoriesForTesting scoped_categories(kTestCategoryPrefixes,
                                                        kTestAcademicMIDs);
  optimization_guide::proto::PageEntitiesMetadata metadata;
  auto* entity1 = metadata.add_entities();
  entity1->set_entity_id("/m/test_academic_mid_1");
  entity1->set_score(100);

  auto* entity2 = metadata.add_entities();
  entity2->set_entity_id("/m/test_academic_mid_2");
  entity2->set_score(99);

  auto result =
      EducationEligibilityVertical::GetTopEducationEntityScore(metadata);
  ASSERT_TRUE(result.has_value());
  EXPECT_FLOAT_EQ(1.0f, result.value());
}

// Tests that entities (scaled 0-100) are evaluated correctly.
TEST_F(EducationEligibilityVerticalTest, TestPageEntitiesField) {
  ScopedEducationCategoriesForTesting scoped_categories(kTestCategoryPrefixes,
                                                        kTestAcademicMIDs);
  optimization_guide::proto::PageEntitiesMetadata metadata;
  auto* entity1 = metadata.add_entities();
  entity1->set_entity_id("/m/test_academic_mid_3");
  entity1->set_score(88);  // Scaled to 0.88f

  auto* entity2 = metadata.add_entities();
  entity2->set_entity_id("/m/test_non_academic_mid");
  entity2->set_score(95);

  auto result =
      EducationEligibilityVertical::GetTopEducationEntityScore(metadata);
  ASSERT_TRUE(result.has_value());
  EXPECT_FLOAT_EQ(0.88f, result.value());
}

// Tests that the highest confidence score is selected among multiple matches.
TEST_F(EducationEligibilityVerticalTest, TestHighestScoreSelected) {
  ScopedEducationCategoriesForTesting scoped_categories(kTestCategoryPrefixes,
                                                        kTestAcademicMIDs);
  optimization_guide::proto::PageEntitiesMetadata metadata;
  auto* cat1 = metadata.add_categories();
  cat1->set_category_id("/TestScience/Chemistry");
  cat1->set_score(0.70f);

  auto* cat2 = metadata.add_categories();
  cat2->set_category_id("/TestEducation/Encyclopedias");
  cat2->set_score(0.92f);

  auto* entity = metadata.add_entities();
  entity->set_entity_id("/m/test_academic_mid_1");
  entity->set_score(81);

  auto result =
      EducationEligibilityVertical::GetTopEducationEntityScore(metadata);
  ASSERT_TRUE(result.has_value());
  EXPECT_FLOAT_EQ(0.92f, result.value());
}

// Tests that DOM features with word count under minimum (< 250) are rejected.
TEST_F(EducationEligibilityVerticalTest,
       TestComputeEducationEligibilityScoreUnderMinWordCount) {
  EducationDOMFeatures features{.word_count = 249, .heading_count = 3};
  EXPECT_EQ(std::nullopt,
            EducationEligibilityVertical::ComputeEducationEligibilityScore(
                0.80f, features, /*min_score=*/0.1f));
}

// Tests that DOM features with zero headings are rejected.
TEST_F(EducationEligibilityVerticalTest,
       TestComputeEducationEligibilityScoreZeroHeadings) {
  EducationDOMFeatures features{.word_count = 500, .heading_count = 0};
  EXPECT_EQ(std::nullopt,
            EducationEligibilityVertical::ComputeEducationEligibilityScore(
                0.80f, features, /*min_score=*/0.1f));
}

// Tests that valid DOM features compute the expected weighted score.
TEST_F(EducationEligibilityVerticalTest,
       TestComputeEducationEligibilityScoreValid) {
  EducationDOMFeatures features{.word_count = 500, .heading_count = 2};
  // word_norm = 500 / 1000 = 0.5
  // heading_norm = 2 / 5 = 0.4
  // ECS = 0.7 * 0.5 + 0.3 * 0.4 = 0.35 + 0.12 = 0.47
  // combined = 1.0 * 0.47 = 0.47
  auto result = EducationEligibilityVertical::ComputeEducationEligibilityScore(
      1.0f, features, /*min_score=*/0.40f);
  ASSERT_TRUE(result.has_value());
  EXPECT_NEAR(0.47f, result.value(), 0.001f);
}

// Tests that large word count and heading count saturate at 1.0.
TEST_F(EducationEligibilityVerticalTest,
       TestComputeEducationEligibilityScoreSaturation) {
  EducationDOMFeatures features{.word_count = 2500, .heading_count = 12};
  auto result = EducationEligibilityVertical::ComputeEducationEligibilityScore(
      1.0f, features, /*min_score=*/0.40f);
  ASSERT_TRUE(result.has_value());
  EXPECT_FLOAT_EQ(1.0f, result.value());
}

// Tests combined Education Eligibility Score (EES = Petacat * ECS).
TEST_F(EducationEligibilityVerticalTest, TestComputeEducationEligibilityScore) {
  EducationDOMFeatures features{.word_count = 1000, .heading_count = 5};
  // ECS = 1.0
  // petacat = 0.85 -> EES = 0.85 >= 0.50 (accepted)
  auto result = EducationEligibilityVertical::ComputeEducationEligibilityScore(
      0.85f, features);
  ASSERT_TRUE(result.has_value());
  EXPECT_FLOAT_EQ(0.85f, result.value());

  // petacat = 0.65, features ECS = 0.47 -> EES = 0.3055 < 0.50 (rejected)
  EducationDOMFeatures low_features{.word_count = 500, .heading_count = 2};
  auto low_result =
      EducationEligibilityVertical::ComputeEducationEligibilityScore(
          0.65f, low_features);
  EXPECT_EQ(std::nullopt, low_result);
}
