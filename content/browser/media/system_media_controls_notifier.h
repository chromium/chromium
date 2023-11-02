// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SYSTEM_MEDIA_CONTROLS_NOTIFIER_H_
#define CONTENT_BROWSER_MEDIA_SYSTEM_MEDIA_CONTROLS_NOTIFIER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "base/timer/timer.h"
#endif  // BUILDFLAG(IS_WIN)

namespace system_media_controls {
class SystemMediaControls;
}  // namespace system_media_controls

namespace content {

// The SystemMediaControlsNotifier connects to the SystemMediaControls API and
// keeps it informed of the current media playback state and metadata. It
// observes changes to the active Media Session and updates the
// SystemMediaControls accordingly.
class CONTENT_EXPORT SystemMediaControlsNotifier
    : public media_session::mojom::MediaControllerObserver,
      public media_session::mojom::MediaControllerImageObserver {
 public:
  explicit SystemMediaControlsNotifier(
      system_media_controls::SystemMediaControls* system_media_controls);

  SystemMediaControlsNotifier(const SystemMediaControlsNotifier&) = delete;
  SystemMediaControlsNotifier& operator=(const SystemMediaControlsNotifier&) =
      delete;

  ~SystemMediaControlsNotifier() override;

  // media_session::mojom::MediaControllerObserver implementation.
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const absl::optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override;
  void MediaSessionChanged(
      const absl::optional<base::UnguessableToken>& request_id) override;
  void MediaSessionPositionChanged(
      const absl::optional<media_session::MediaPosition>& position) override;

  // media_session::mojom::MediaControllerImageObserver implementation.
  void MediaControllerImageChanged(
      ::media_session::mojom::MediaSessionImageType type,
      const SkBitmap& bitmap) override;

 private:
  friend class SystemMediaControlsNotifierTest;

  // We want to hide the controls on the lock screen on Windows in certain
  // cases. We don't want this functionality on other OSes.
#if BUILDFLAG(IS_WIN)
  // Polls the current idle state of the system.
  void CheckLockState();

  // Called when the idle state changes from unlocked to locked.
  void OnScreenLocked();

  // Called when the idle state changes from locked to unlocked.
  void OnScreenUnlocked();

  // Helper functions for dealing with the timer that hides the System Media
  // Transport Controls on the lock screen 5 seconds after the user pauses.
  void StartHideSmtcTimer();
  void StopHideSmtcTimer();
  void HideSmtcTimerFired();

  bool screen_locked_ = false;
  base::RepeatingTimer lock_polling_timer_;
  base::OneShotTimer hide_smtc_timer_;
#endif  // BUILDFLAG(IS_WIN)

  // Our connection to the System Media Controls. We don't own it since it's a
  // global instance.
  const raw_ptr<system_media_controls::SystemMediaControls>
      system_media_controls_;

  // Tracks current media session state/metadata.
  mojo::Remote<media_session::mojom::MediaController> media_controller_;
  media_session::mojom::MediaSessionInfoPtr session_info_ptr_;

  // Used to receive updates to the active media controller.
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};
  mojo::Receiver<media_session::mojom::MediaControllerImageObserver>
      media_controller_image_observer_receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SYSTEM_MEDIA_CONTROLS_NOTIFIER_H_
