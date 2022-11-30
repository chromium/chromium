// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/active_sampling.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/common/privacy_budget/test_ukm_recorder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"

namespace blink {

namespace {

class ScopedIdentifiabilityStudySettings {
 public:
  class TestProvider : public IdentifiabilityStudySettingsProvider {
   public:
    bool IsActive() const override { return true; }

    bool IsAnyTypeOrSurfaceBlocked() const override { return false; }

    bool IsSurfaceAllowed(IdentifiableSurface surface) const override {
      return true;
    }

    bool IsTypeAllowed(IdentifiableSurface::Type type) const override {
      return true;
    }

    bool ShouldActivelySample() const override { return true; }

    std::vector<std::string> FontFamiliesToActivelySample() const override {
      return {"Arial", "Helvetica"};
    }
  };

  ScopedIdentifiabilityStudySettings() {
    // Reload the config in the global study settings.
    blink::IdentifiabilityStudySettings::SetGlobalProvider(
        std::make_unique<TestProvider>());
  }

  ~ScopedIdentifiabilityStudySettings() {
    blink::IdentifiabilityStudySettings::ResetStateForTesting();
  }
};

}  // namespace

TEST(PrivacyBudgetActiveSamplingTest, ActivelySampleFonts) {
  base::test::TaskEnvironment task_environment_;
  ScopedIdentifiabilityStudySettings scoped_settings;

  base::RunLoop run_loop;
  test::TestUkmRecorder ukm_recorder;
  IdentifiabilityActiveSampler::ActivelySampleAvailableFonts(&ukm_recorder);

  const std::vector<ukm::mojom::UkmEntryPtr>& entries = ukm_recorder.entries();

  std::vector<uint64_t> reported_surface_keys;
  for (const auto& entry : entries) {
    for (const auto& metric : entry->metrics) {
      reported_surface_keys.push_back(metric.first);
    }
  }
  EXPECT_EQ(reported_surface_keys,
            std::vector<uint64_t>({
                9223784233214641190u,   // Arial
                10735872651981970214u,  // Helvetica
            }));
}

}  // namespace blink
