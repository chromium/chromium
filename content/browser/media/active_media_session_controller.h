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

// Intakes media events (such as media key presses) and controls the active
// media session.
// TODO(crbug.com/40943396) Consider renaming this class in the world of
// instanced system media controls.
class CONTENT_EXPORT ActiveMediaSessionController
    : public media_session::mojom::MediaControllerObserver,
      public ui::MediaKeysListener::Delegate {
 public:
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

  // Receives events from the SystemMediaControls.
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

  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote_;

  // Used to control the active session.
  mojo::Remote<media_session::mojom::MediaController> media_controller_remote_;

  // Used to check whether a play/pause key should play or pause (based on
  // current playback state).
  media_session::mojom::MediaSessionInfoPtr session_info_;

  // Used to check which actions are currently supported.
  base::flat_set<media_session::mojom::MediaSessionAction> actions_;

  // Used to receive updates to the active media controller.
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};

  // Stores the current playback position.
  std::optional<media_session::MediaPosition> position_;

  // Stores the media session (if any specific one) this active media session
  // controller is associated with. If this is null, this AMSC follows around
  // the active media session automatically and will receive events for it.
  base::UnguessableToken request_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ACTIVE_MEDIA_SESSION_CONTROLLER_H_
