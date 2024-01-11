// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_power_experiment_manager.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/media_switches.h"

namespace content {

MediaPowerExperimentManager::MediaPowerExperimentManager()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

MediaPowerExperimentManager::~MediaPowerExperimentManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MediaPowerExperimentManager::PlayerStarted(const MediaPlayerId& player_id,
                                                ExperimentCB cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  players_[player_id] = std::move(cb);
  CheckExperimentState();
}

void MediaPowerExperimentManager::PlayerStopped(
    const MediaPlayerId& player_id,
    NotificationMode notification_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If we're supposed to skip notification, then clear the current player if
  // it matches |player_id|, so that CheckExperimentState doesn't notify it.
  if (notification_mode == NotificationMode::kSkip &&
      current_experiment_player_ && *current_experiment_player_ == player_id) {
    current_experiment_player_.reset();
    current_experiment_cb_ = ExperimentCB();
    // Note that there will be no incoming player; there's exactly one and we're
    // removing it.
  }
  players_.erase(player_id);
  CheckExperimentState();
}

void MediaPowerExperimentManager::CheckExperimentState() {
  // See if an experiment should be running.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<MediaPlayerId> new_experiment_player;
  if (players_.size() == 1)
    new_experiment_player = players_.begin()->first;

  // If there's a current experiment running, and it's not the same as the
  // incoming one (if any), then stop it and clear the current player.
  if (current_experiment_player_ &&
      (!new_experiment_player ||
       *current_experiment_player_ != *new_experiment_player)) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(current_experiment_cb_, false));
    current_experiment_player_.reset();
    current_experiment_cb_ = ExperimentCB();
  }

  // If there's an incoming player that's not currently running, then notify it
  // and remember the current player.
  if (!current_experiment_player_ && new_experiment_player) {
    current_experiment_player_ = std::move(new_experiment_player);
    // We cache the callback so that, when this player is stopped, we can
    // notify it.  Otherwise, it's removed from the map by the time we're here.
    current_experiment_cb_ = players_.find(*current_experiment_player_)->second;
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(current_experiment_cb_, true));
  }
}

// static
MediaPowerExperimentManager* MediaPowerExperimentManager::Instance() {
  // Return nullptr unless an experiment is enabled that needs us.
  if (base::FeatureList::IsEnabled(media::kMediaPowerExperiment)) {
    static base::NoDestructor<MediaPowerExperimentManager> s_manager;
    return s_manager.get();
  }
  return nullptr;
}

}  // namespace content
