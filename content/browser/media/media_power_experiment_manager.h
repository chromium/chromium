// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_POWER_EXPERIMENT_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_POWER_EXPERIMENT_MANAGER_H_

#include <map>
#include <optional>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_player_id.h"
#include "media/base/video_codecs.h"

namespace content {

// Keeps track of all media players across all pages, and notifies them when
// they enter or leave an active experiment.
class CONTENT_EXPORT MediaPowerExperimentManager {
 public:
  // Callback to notify the client when an experiment starts / stops.
  using ExperimentCB = base::RepeatingCallback<void(bool)>;

  // Flags for handling notifications of stopped players.
  enum class NotificationMode {
    // If the stopped player is the current experiment, then notify it that it
    // no longer is.
    kNotify,

    // If the stopped player is the current experiment, then skip notification.
    // This is useful if the player is being destroyed.
    kSkip
  };

  MediaPowerExperimentManager();

  MediaPowerExperimentManager(const MediaPowerExperimentManager&) = delete;
  MediaPowerExperimentManager& operator=(const MediaPowerExperimentManager&) =
      delete;

  virtual ~MediaPowerExperimentManager();

  // May return nullptr if experiments aren't enabled.
  static MediaPowerExperimentManager* Instance();

  // Called when the given player begins playing.  |cb| will be called if it
  // becomes / stops being the only playing player, though never re-entrantly.
  virtual void PlayerStarted(const MediaPlayerId& player, ExperimentCB cb);

  // Called when the given player has stopped playing.  It is okay if it was
  // never started via PlayerStarted; we'll just ignore it.  If
  // |notification_mode| is kSkip, then we won't notify |player| that it's
  // stopping, if it's the current experiment.
  virtual void PlayerStopped(
      const MediaPlayerId& player,
      NotificationMode notification_mode = NotificationMode::kNotify);

 private:
  // Send start / stop notifications and update |current_experiment_player_|
  // based on whether an experiment should be running.
  void CheckExperimentState();

  // Set of all playing players that we know about.
  std::map<MediaPlayerId, ExperimentCB> players_;

  // If set, this is the player that has a running experiment.
  std::optional<MediaPlayerId> current_experiment_player_;
  ExperimentCB current_experiment_cb_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_POWER_EXPERIMENT_MANAGER_H_
