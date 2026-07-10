// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/field_trial_synchronizer.h"

#include <memory>
#include <string>

#include "base/metrics/field_trial.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/runtime_field_trial_overrides.h"
#include "base/types/pass_key.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/variations/hashing.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
class VariationsService {
 public:
  static base::PassKey<VariationsService> CreatePassKeyForTesting() {
    return base::PassKey<VariationsService>();
  }
};
}  // namespace variations

namespace content {
namespace {

class FieldTrialSynchronizerTest : public testing::Test {
 public:
  constexpr static int32_t kAllocatorMemorySize = 1 << 20;  // 1 MiB

  FieldTrialSynchronizerTest()
      : scoped_variations_ids_provider_(
            variations::VariationsIdsProvider::Mode::kUseSignedInState) {
    FieldTrialSynchronizer::CreateInstance();
  }
  ~FieldTrialSynchronizerTest() override {
    FieldTrialSynchronizer::DeleteInstanceForTesting();
  }

  void SetUp() override {
    testing::Test::SetUp();

    memory_allocator_ = std::make_unique<base::LocalPersistentMemoryAllocator>(
        kAllocatorMemorySize, 0, "");
    FieldTrialSynchronizer::RegisterPersistentAllocatorForTesting(
        memory_allocator_.get());
    metrics::SystemProfileProto profile;
    profile.set_client_uuid("id");
    std::string serialized_profile;
    profile.SerializeToString(&serialized_profile);
    FieldTrialSynchronizer::SetSystemProfileForTesting(serialized_profile,
                                                       false);
  }

  void TearDown() override {
    FieldTrialSynchronizer::DeregisterPersistentAllocatorForTesting(
        memory_allocator_.get());
    base::RuntimeFieldTrialOverrides::GetInstance()->ResetForTesting();
    testing::Test::TearDown();
  }

  metrics::SystemProfileProto GetSystemProfile() {
    metrics::SystemProfileProto profile;
    metrics::PersistentSystemProfile::GetSystemProfile(*memory_allocator_,
                                                       &profile);
    return profile;
  }

  bool ProfileHasFieldTrial(const std::string& trial,
                            const std::string& group) {
    metrics::SystemProfileProto profile = GetSystemProfile();
    uint64_t trial_hash = variations::HashName(trial);
    uint64_t group_hash = variations::HashName(group);
    for (int i = 0; i < profile.field_trial_size(); ++i) {
      if (profile.field_trial(i).name_id() == trial_hash &&
          profile.field_trial(i).group_id() == group_hash) {
        return true;
      }
    }
    return false;
  }

 protected:
  content::BrowserTaskEnvironment task_env_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_;
  std::unique_ptr<base::PersistentMemoryAllocator> memory_allocator_;
};

TEST_F(FieldTrialSynchronizerTest, RuntimeOverrides) {
  auto* overrides = base::RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  // Create an overridden field trial.
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial("OverriddenTrial", "Group");
  EXPECT_FALSE(ProfileHasFieldTrial("OverriddenTrial", "Group"));
  trial->Activate();
  // Activating the trial should have added it to the profile.
  EXPECT_TRUE(ProfileHasFieldTrial("OverriddenTrial", "Group"));

  // Apply the runtime override, replacing the overridden trial.
  EXPECT_TRUE(overrides->ApplyRuntimeOverride(pass_key, "Killswitch",
                                              "Disabled", trial.get()));

  // The overridden trial should be removed from the profile, and the new
  // override added.
  EXPECT_FALSE(ProfileHasFieldTrial("OverriddenTrial", "Group"));
  EXPECT_TRUE(ProfileHasFieldTrial("Killswitch", "Disabled"));

  // Now, apply another override targeting the same overridden trial, replacing
  // the previous override.
  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key, "NewKillswitch", "NewDisabled", trial.get(),
      /*previous_override_trial_name=*/"Killswitch"));

  // The previous override "Killswitch" should be removed, and "NewKillswitch"
  // added.
  EXPECT_FALSE(ProfileHasFieldTrial("OverriddenTrial", "Group"));
  EXPECT_FALSE(ProfileHasFieldTrial("Killswitch", "Disabled"));
  EXPECT_TRUE(ProfileHasFieldTrial("NewKillswitch", "NewDisabled"));
}

}  // namespace
}  // namespace content
