// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_WEB_APP_SYSTEM_MEDIA_CONTROLS_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_WEB_APP_SYSTEM_MEDIA_CONTROLS_MANAGER_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "third_party/blink/public/mojom/mediasession/media_session.mojom.h"

namespace system_media_controls {
class SystemMediaControls;
}

namespace content {

class WebAppSystemMediaControls;

// WebAppSystemMediaControlsManager is a class that handles system media
// controls related metadata for use with instanced per dPWA system media
// controls. Primarily, it is used by other systems to find associated objects
// when only having one piece of information such as RequestId.
//
// WebAppSystemMediaControlsManager creates and owns WebAppSystemMediaControls
// objects. A single WebAppSystemMediaControls owns references to all the
// objects needed for a single dPWA to integrate with the OS' media playback
// system.
//
// NOTE: WebAppSystemMediaControls is not derived from SystemMediaControls.
// rather a WebAppSystemMediaControls owns and holds a SystemMediaControls.
// WebAppSystemMediaControls also contains other related classes. See
// web_app_system_media_controls.h.
//
// Usage: After this class is constructed, call Init(). The manager
// will connect itself to the AudioFocusManager to receive messages about
// focus changes.
class CONTENT_EXPORT WebAppSystemMediaControlsManager
    : public media_session::mojom::AudioFocusObserver {
 public:
  WebAppSystemMediaControlsManager();

  WebAppSystemMediaControlsManager(const WebAppSystemMediaControlsManager&) =
      delete;
  WebAppSystemMediaControlsManager& operator=(
      const WebAppSystemMediaControlsManager&) = delete;

  ~WebAppSystemMediaControlsManager() override;

  // Runs initialization steps such as connecting to AudioFocusManager
  void Init();

  // media_session::mojom::AudioFocusObserver implementation
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr state) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr state) override;
  void OnRequestIdReleased(const base::UnguessableToken& request_id) override;

  // Helpers
  void OnMojoError();

  // Retrieve the WebAppSystemMediaControls for |request_id|, returns nullptr if
  // not found.
  WebAppSystemMediaControls* GetControlsForRequestId(
      base::UnguessableToken request_id);
  // Retrieve the WebAppSystemMediaControls that contains
  // |system_media_controls|, returns nullptr if not found.
  WebAppSystemMediaControls* GetWebAppSystemMediaControlsForSystemMediaControls(
      system_media_controls::SystemMediaControls* system_media_controls);

  bool IsActive() { return !controls_map_.empty(); }

  // Retrieves a vector of all the WebAppSystemMediaControls associated with
  // this manager.
  std::vector<WebAppSystemMediaControls*> GetAllControls();

  // Dumps the stored metadata via DVLOG(1) for debugging.
  void LogDataForDebugging();

 private:
  void SkipMojoConnectionForTesting() {
    skip_mojo_connection_for_testing_ = true;
  }

  void TryConnectToAudioFocusManager();

  std::map<base::UnguessableToken, std::unique_ptr<WebAppSystemMediaControls>>
      controls_map_;

  // Used to receive updates about all media sessions, not just the active one.
  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};

  // Used to manage the AudioFocusObserver connection.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_manager_;

  bool initialized_ = false;
  bool skip_mojo_connection_for_testing_ = false;

  friend class WebAppSystemMediaControlsManagerTest;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_WEB_APP_SYSTEM_MEDIA_CONTROLS_MANAGER_H_
