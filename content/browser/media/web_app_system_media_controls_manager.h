// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_WEB_APP_SYSTEM_MEDIA_CONTROLS_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_WEB_APP_SYSTEM_MEDIA_CONTROLS_MANAGER_H_

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace system_media_controls {
class SystemMediaControls;
}

namespace content {

class WebAppSystemMediaControls;

// Used to fire telemetry about instanced PWA controls usage.
// This enum is used to back a histogram. Do not remove or reorder members.
enum class WebAppSystemMediaControlsEvent {
  kPwaPlayingMedia = 0,
  kPwaSmcNext = 1,
  kPwaSmcPrevious = 2,
  kPwaSmcPlay = 3,
  kPwaSmcPause = 4,
  kPwaSmcPlayPause = 5,
  kPwaSmcStop = 6,
  kPwaSmcSeek = 7,
  kPwaSmcSeekTo = 8,
  kMaxValue = kPwaSmcSeekTo,
};

// A simple observer interface for tests to be notified when events are
// received by the WebAppSystemmediaControlsManager.
class WebAppSystemMediaControlsManagerObserver {
 public:
  virtual void OnWebAppAdded(base::UnguessableToken request_id) {}
};

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

  // Retrieve the WebAppSystemMediaControls for `request_id`, returns nullptr if
  // not found.
  WebAppSystemMediaControls* GetControlsForRequestId(
      base::UnguessableToken request_id);
  // Retrieve the WebAppSystemMediaControls that contains
  // `system_media_controls`, returns nullptr if not found.
  WebAppSystemMediaControls* GetWebAppSystemMediaControlsForSystemMediaControls(
      system_media_controls::SystemMediaControls* system_media_controls);

  bool IsActive() { return !controls_map_.empty(); }

  // Retrieves a vector of all the WebAppSystemMediaControls associated with
  // this manager.
  std::vector<WebAppSystemMediaControls*> GetAllControls();

  // This lets chrome/browser tests get notified when SMCBridge is created.
  void SetOnSystemMediaControlsBridgeCreatedCallbackForTesting(
      base::RepeatingCallback<void()> callback);

 private:
  void SkipMojoConnectionForTesting() {
    skip_mojo_connection_for_testing_ = true;
  }

  void TryConnectToAudioFocusManager();

  // This method allows friended tests to register themselves as observers
  // to be notified of events happening in this manager.
  void SetObserverForTesting(
      WebAppSystemMediaControlsManagerObserver* observer) {
    test_observer_ = observer;
  }

  std::map<base::UnguessableToken, std::unique_ptr<WebAppSystemMediaControls>>
      controls_map_;

  // Used to receive updates about all media sessions, not just the active one.
  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};

  // Used to manage the AudioFocusObserver connection.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_manager_;

  bool initialized_ = false;
  bool skip_mojo_connection_for_testing_ = false;
  bool always_assume_web_app_for_testing_ = false;

  raw_ptr<WebAppSystemMediaControlsManagerObserver> test_observer_;

  base::RepeatingCallback<void()>
      on_system_media_controls_bridge_created_callback_for_testing_;

  friend class WebAppSystemMediaControlsManagerTest;
  friend class WebAppSystemMediaControlsBrowserTest;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_WEB_APP_SYSTEM_MEDIA_CONTROLS_MANAGER_H_
