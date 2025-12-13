// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_process_synthetic_trial_syncer.h"

#include "base/test/task_environment.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/variations_crash_keys.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

std::vector<mojom::SyntheticTrialGroupPtr> Create(
    std::vector<std::pair<std::string, std::string>> trial_names) {
  std::vector<mojom::SyntheticTrialGroupPtr> groups;
  for (auto& it : trial_names) {
    groups.push_back(mojom::SyntheticTrialGroup::New(it.first, it.second));
  }
  return groups;
}

}  // namespace

class ChildProcessSyntheticTrialSyncerTest : public ::testing::Test {
 public:
  void SetUp() override {
    variations::InitCrashKeys();
    syncer_ = ChildProcessSyntheticTrialSyncer::CreateInstanceForTesting();
    base_state_ = variations::GetExperimentListInfo();
  }

  void TearDown() override {
    syncer_.reset();
    variations::ClearCrashKeysInstanceForTesting();
  }

  variations::ExperimentListInfo GetExperimentListInfo() {
    const auto current_state = variations::GetExperimentListInfo();

    // The base_state_ is captured at the time of construction
    // of this test fixture, it contains experiments defined in the field
    // trial testing config. This returns the difference between the current
    // state and that initial state to only capture the experiments
    // added/removed by this test fixture.
    return {
        .num_experiments =
            current_state.num_experiments - base_state_.num_experiments,
        .experiment_list = current_state.experiment_list.substr(
            base_state_.experiment_list.size()),
    };
  }

  void AddOrUpdateSyntheticTrialGroups(
      std::vector<std::pair<std::string, std::string>> groups) {
    syncer_->AddOrUpdateSyntheticTrialGroups(Create(groups));
  }

  void RemoveSyntheticTrialGroups(
      std::vector<std::pair<std::string, std::string>> groups) {
    syncer_->RemoveSyntheticTrialGroups(Create(groups));
  }

  std::string GetExperimentHash(std::string trial_name,
                                std::string group_name) {
    return variations::ActiveGroupToString(
        variations::MakeActiveGroupId(trial_name, group_name));
  }

 private:
  std::unique_ptr<ChildProcessSyntheticTrialSyncer> syncer_;
  base::test::TaskEnvironment task_environment_;
  variations::ExperimentListInfo base_state_;
};

TEST_F(ChildProcessSyntheticTrialSyncerTest, Basic) {
  AddOrUpdateSyntheticTrialGroups({{"A", "G1"}, {"B", "G2"}});

  auto info = GetExperimentListInfo();
  EXPECT_EQ(GetExperimentHash("A", "G1") + GetExperimentHash("B", "G2"),
            info.experiment_list);

  AddOrUpdateSyntheticTrialGroups({{"C", "G3"}});
  info = GetExperimentListInfo();
  EXPECT_EQ(3, info.num_experiments);
  EXPECT_EQ(GetExperimentHash("A", "G1") + GetExperimentHash("B", "G2") +
                GetExperimentHash("C", "G3"),
            info.experiment_list);

  AddOrUpdateSyntheticTrialGroups({{"A", "G4"}});
  info = GetExperimentListInfo();
  EXPECT_EQ(3, info.num_experiments);
  EXPECT_EQ(GetExperimentHash("A", "G4") + GetExperimentHash("B", "G2") +
                GetExperimentHash("C", "G3"),
            info.experiment_list);

  RemoveSyntheticTrialGroups({{"B", "G2"}});
  info = GetExperimentListInfo();
  EXPECT_EQ(2, info.num_experiments);
  EXPECT_EQ(GetExperimentHash("A", "G4") + GetExperimentHash("C", "G3"),
            info.experiment_list);

  RemoveSyntheticTrialGroups({{"C", "G3"}, {"A", "G4"}});
  info = GetExperimentListInfo();
  EXPECT_EQ(0, info.num_experiments);
}

TEST_F(ChildProcessSyntheticTrialSyncerTest, AddSameTrial) {
  AddOrUpdateSyntheticTrialGroups({{"A", "G1"}, {"A", "G2"}, {"A", "G3"}});
  auto info = GetExperimentListInfo();
  EXPECT_EQ(1, info.num_experiments);
  EXPECT_EQ(GetExperimentHash("A", "G3"), info.experiment_list);
}

TEST_F(ChildProcessSyntheticTrialSyncerTest, RemoveFromEmpty) {
  RemoveSyntheticTrialGroups({{"B", "G2"}, {"C", "G1"}});
  auto info = GetExperimentListInfo();
  EXPECT_EQ(0, info.num_experiments);
}

TEST_F(ChildProcessSyntheticTrialSyncerTest, RemoveWrongTrialGroup) {
  AddOrUpdateSyntheticTrialGroups({{"A", "G1"}, {"B", "G2"}});

  auto info = GetExperimentListInfo();
  EXPECT_EQ(2, info.num_experiments);
  EXPECT_EQ(GetExperimentHash("A", "G1") + GetExperimentHash("B", "G2"),
            info.experiment_list);

  RemoveSyntheticTrialGroups({{"B", "G4"}});
  EXPECT_EQ(2, info.num_experiments);
  EXPECT_EQ(GetExperimentHash("A", "G1") + GetExperimentHash("B", "G2"),
            info.experiment_list);
}

}  // namespace content
