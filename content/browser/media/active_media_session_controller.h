// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ACTIVE_MEDIA_SESSION_CONTROLLER_H_
#define CONTENT_BROWSER_MEDIA_ACTIVE_MEDIA_SESSION_CONTROLLER_H_

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "ui/base/accelerators/media_keys_listener.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {

// Used by the system media controls to intake media events (such as media key
// presses or click events from the OS) and control a media session.
class CONTENT_EXPORT ActiveMediaSessionController
    : public media_session::mojom::MediaControllerObserver,
      public ui::MediaKeysListener::Delegate {
 public:
  // When `request_id` is non-null, creates a controller for that specific media
  // session. Used for dPWAs to control each app's media session individually.
  // When `request_id` is null, creates a controller that automatically follows
  // and controls the "active" session. Used for the browser's media sessions,
  // and all scenarios where kWebAppSystemMediaControls is NOT supported - Linux
  // always, and macOS/Windows when the feature flag is off.
  explicit ActiveMediaSessionController(base::UnguessableToken request_id);
  ActiveMediaSessionController(const ActiveMediaSessionController&) = delete;
  ActiveMediaSessionController& operator=(const ActiveMediaSessionController&) =
      delete;
  ~ActiveMediaSessionController() override;

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override {}
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override;
  void MediaSessionChanged(
      const std::optional<base::UnguessableToken>& request_id) override {}
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override;

  // ui::MediaKeysListener::Delegate:
  void OnMediaKeysAccelerator(const ui::Accelerator& accelerator) override;

  // Receives OS events from the SystemMediaControls.
  void OnNext();
  void OnPrevious();
  void OnPlay();
  void OnPause();
  void OnPlayPause();
  void OnStop();
  void OnSeek(const base::TimeDelta& time);
  void OnSeekTo(const base::TimeDelta& time);

  void FlushForTesting();
  void SetMediaControllerForTesting(
      mojo::Remote<media_session::mojom::MediaController> controller) {
    media_controller_remote_ = std::move(controller);
  }

 private:
  // Used for converting between MediaSessionAction and KeyboardCode.
  media_session::mojom::MediaSessionAction KeyCodeToMediaSessionAction(
      ui::KeyboardCode key_code) const;

  // Returns nullopt if the action is not supported via hardware keys (e.g.
  // SeekBackward).
  std::optional<ui::KeyboardCode> MediaSessionActionToKeyCode(
      media_session::mojom::MediaSessionAction action) const;

  void MaybePerformAction(media_session::mojom::MediaSessionAction action);
  bool SupportsAction(media_session::mojom::MediaSessionAction action) const;
  void PerformAction(media_session::mojom::MediaSessionAction action);

  // Used to create the media controller.
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote_;

  // Used to control the media session.
  mojo::Remote<media_session::mojom::MediaController> media_controller_remote_;

  // Used to check whether a play/pause key should play or pause (based on
  // current playback state).
  media_session::mojom::MediaSessionInfoPtr session_info_;

  // Used to check which actions are currently supported.
  base::flat_set<media_session::mojom::MediaSessionAction> actions_;

  // Used to receive updates from the media session.
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};

  // Stores the current playback position.
  std::optional<media_session::MediaPosition> position_;

  // Stores the media session (if any) this active media session controller is
  // associated with.
  // Null if this controller was created to automatically follow and control
  // the "active" session.
  base::UnguessableToken request_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ACTIVE_MEDIA_SESSION_CONTROLLER_H_
