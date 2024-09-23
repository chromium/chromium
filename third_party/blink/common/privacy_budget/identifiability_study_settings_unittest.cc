// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include <memory>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_test_utils.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings_provider.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace blink {

TEST(IdentifiabilityStudySettingsTest, DisabledProvider) {
  CallCounts counts{.response_for_is_active = false};

  IdentifiabilityStudySettings settings(
      std::make_unique<CountingSettingsProvider>(&counts));
  EXPECT_EQ(1, counts.count_of_is_active);
  EXPECT_EQ(1, counts.count_of_is_any_type_or_surface_blocked);

  EXPECT_FALSE(settings.IsActive());
  EXPECT_EQ(1, counts.count_of_is_active);
  EXPECT_FALSE(settings.ShouldSampleSurface(IdentifiableSurface()));
  EXPECT_EQ(1, counts.count_of_is_active);
  EXPECT_FALSE(
      settings.ShouldSampleType(IdentifiableSurface::Type::kCanvasReadback));

  // None of these should have been called.
  EXPECT_EQ(0, counts.count_of_is_surface_allowed);
  EXPECT_EQ(0, counts.count_of_is_type_allowed);
}

TEST(IdentifiabilityStudySettingsTest, MetaExperimentActive) {
  CallCounts counts{.response_for_is_meta_experiment_active = true};

  IdentifiabilityStudySettings settings(
      std::make_unique<CountingSettingsProvider>(&counts));

  // No other calls should be made.
  EXPECT_TRUE(settings.IsActive());
  EXPECT_TRUE(settings.ShouldSampleSurface(IdentifiableSurface()));
  EXPECT_TRUE(
      settings.ShouldSampleType(IdentifiableSurface::Type::kWebFeature));

  EXPECT_EQ(1, counts.count_of_is_meta_experiment_active);
  EXPECT_EQ(1, counts.count_of_is_active);
  EXPECT_EQ(1, counts.count_of_is_any_type_or_surface_blocked);
  EXPECT_EQ(0, counts.count_of_is_surface_allowed);
  EXPECT_EQ(0, counts.count_of_is_type_allowed);
}

TEST(IdentifiabilityStudySettingsTest,
     MetaExperimentActiveWithBlockedTypeOrSurface) {
  CallCounts counts{
      .response_for_is_meta_experiment_active = true,
      .response_for_is_active = true,
      .response_for_is_anything_blocked = true,
      .response_for_is_allowed = false,
  };

  IdentifiabilityStudySettings settings(
      std::make_unique<CountingSettingsProvider>(&counts));

  // No other calls should be made.
  EXPECT_TRUE(settings.IsActive());
  EXPECT_TRUE(settings.ShouldSampleSurface(IdentifiableSurface()));
  EXPECT_TRUE(
      settings.ShouldSampleType(IdentifiableSurface::Type::kWebFeature));

  EXPECT_EQ(1, counts.count_of_is_meta_experiment_active);
  EXPECT_EQ(1, counts.count_of_is_active);
  EXPECT_EQ(1, counts.count_of_is_any_type_or_surface_blocked);
  EXPECT_EQ(0, counts.count_of_is_surface_allowed);
  EXPECT_EQ(0, counts.count_of_is_type_allowed);
}

TEST(IdentifiabilityStudySettingsTest, IsActiveButNothingIsBlocked) {
  CallCounts counts{.response_for_is_meta_experiment_active = false,
                    .response_for_is_active = true,
                    .response_for_is_anything_blocked = false,

                    // Note that this contradicts the above, but it shouldn't
                    // matter since Is*Blocked() should not be called at all.
                    .response_for_is_allowed = true};

  IdentifiabilityStudySettings settings(
      std::make_unique<CountingSettingsProvider>(&counts));

  // No other calls should be made.
  EXPECT_TRUE(settings.IsActive());
  EXPECT_TRUE(settings.ShouldSampleSurface(IdentifiableSurface()));
  EXPECT_TRUE(
      settings.ShouldSampleType(IdentifiableSurface::Type::kWebFeature));

  EXPECT_EQ(1, counts.count_of_is_active);
  EXPECT_EQ(1, counts.count_of_is_any_type_or_surface_blocked);
  EXPECT_EQ(0, counts.count_of_is_surface_allowed);
  EXPECT_EQ(0, counts.count_of_is_type_allowed);
}

TEST(IdentifiabilityStudySettingsTest, IsSurfaceOrTypeBlocked) {
  CallCounts counts{.response_for_is_meta_experiment_active = false,
                    .response_for_is_active = true,
                    .response_for_is_anything_blocked = true,
                    .response_for_is_allowed = false};

  IdentifiabilityStudySettings settings(
      std::make_unique<CountingSettingsProvider>(&counts));

  // No other calls should be made.
  EXPECT_TRUE(settings.IsActive());
  EXPECT_FALSE(settings.ShouldSampleSurface(IdentifiableSurface()));
  EXPECT_FALSE(
      settings.ShouldSampleType(IdentifiableSurface::Type::kWebFeature));

  EXPECT_EQ(1, counts.count_of_is_active);
  EXPECT_EQ(1, counts.count_of_is_any_type_or_surface_blocked);
  EXPECT_EQ(1, counts.count_of_is_surface_allowed);
  EXPECT_EQ(1, counts.count_of_is_type_allowed);
}

TEST(IdentifiabilityStudySettingsTest, DefaultSettings) {
  auto* default_settings = IdentifiabilityStudySettings::Get();
  EXPECT_FALSE(default_settings->IsActive());
  EXPECT_FALSE(default_settings->ShouldSampleSurface(IdentifiableSurface()));
  EXPECT_FALSE(default_settings->ShouldSampleType(
      IdentifiableSurface::Type::kWebFeature));
}

TEST(IdentifiabilityStudySettingsTest, StaticSetProvider) {
  CallCounts counts{.response_for_is_meta_experiment_active = false,
                    .response_for_is_active = true,
                    .response_for_is_anything_blocked = true,
                    .response_for_is_allowed = true};

  IdentifiabilityStudySettings::SetGlobalProvider(
      std::make_unique<CountingSettingsProvider>(&counts));
  auto* settings = IdentifiabilityStudySettings::Get();
  EXPECT_TRUE(settings->IsActive());
  EXPECT_TRUE(settings->ShouldSampleSurface(IdentifiableSurface()));
  EXPECT_EQ(1, counts.count_of_is_surface_allowed);

  IdentifiabilityStudySettings::ResetStateForTesting();

  auto* default_settings = IdentifiabilityStudySettings::Get();
  EXPECT_FALSE(default_settings->IsActive());
}

}  // namespace blink
