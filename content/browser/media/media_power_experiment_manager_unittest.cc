// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_power_experiment_manager.h"

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/version.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_controller.h"
#include "media/base/media_switches.h"
#include "media/base/mock_filters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MediaPowerExperimentManagerTest : public testing::Test {
 public:
  MediaPowerExperimentManagerTest()
      : manager_(std::make_unique<MediaPowerExperimentManager>()),
        player_id_1_(nullptr, 1),
        player_id_2_(nullptr, 2),
        cb_1_(base::BindRepeating([](bool* out, bool state) { *out = state; },
                                  &experiment_state_1_)),
        cb_2_(base::BindRepeating([](bool* out, bool state) { *out = state; },
                                  &experiment_state_2_)) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<MediaPowerExperimentManager> manager_;
  // Unique player IDs.  Note that we can't CreateMediaPlayerIdForTesting()
  // since it doesn't return unique IDs.
  MediaPlayerId player_id_1_;
  MediaPlayerId player_id_2_;

  bool experiment_state_1_ = false;
  bool experiment_state_2_ = false;

  MediaPowerExperimentManager::ExperimentCB cb_1_;
  MediaPowerExperimentManager::ExperimentCB cb_2_;
};

TEST_F(MediaPowerExperimentManagerTest, ExperimentStopsWhenPlayerStopped) {
  // Create a player, and verify that the the experiment starts, then stops
  // when it stops playing.
  EXPECT_FALSE(experiment_state_1_);
  manager_->PlayerStarted(player_id_1_, cb_1_);

  // Should call back, but not re-entrantly.
  EXPECT_FALSE(experiment_state_1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(experiment_state_1_);

  manager_->PlayerStopped(player_id_1_);
  EXPECT_TRUE(experiment_state_1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(experiment_state_1_);
}

TEST_F(MediaPowerExperimentManagerTest, ExperimentStopsWhenSecondPlayerStarts) {
  // If we add a second playing player, then the experiment should stop.
  manager_->PlayerStarted(player_id_1_, cb_1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(experiment_state_1_);
  EXPECT_FALSE(experiment_state_2_);

  manager_->PlayerStarted(player_id_2_, cb_2_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(experiment_state_1_);
  EXPECT_FALSE(experiment_state_2_);

  manager_->PlayerStopped(player_id_1_);
  manager_->PlayerStopped(player_id_2_);
}

TEST_F(MediaPowerExperimentManagerTest,
       ExperimentRestartsWhenFirstPlayerRemoved) {
  // If we add a second playing player, then removing the first should start it.
  manager_->PlayerStarted(player_id_1_, cb_1_);
  manager_->PlayerStarted(player_id_2_, cb_2_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(experiment_state_1_);
  EXPECT_FALSE(experiment_state_2_);

  manager_->PlayerStopped(player_id_1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(experiment_state_1_);
  EXPECT_TRUE(experiment_state_2_);

  manager_->PlayerStopped(player_id_2_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(experiment_state_2_);
}

TEST_F(MediaPowerExperimentManagerTest, NotificationsCanBeSkipped) {
  manager_->PlayerStarted(player_id_1_, cb_1_);
  manager_->PlayerStopped(player_id_1_,
                          MediaPowerExperimentManager::NotificationMode::kSkip);
  base::RunLoop().RunUntilIdle();

  // Since we skipped notification, the state should still be true.
  EXPECT_TRUE(experiment_state_1_);
}

}  // namespace content
