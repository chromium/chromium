// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_AUDIO_FOCUS_HELPER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_AUDIO_FOCUS_HELPER_H_

#include <map>
#include <string_view>

#include "base/values.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace content {

// MediaInternalsAudioFocusHelper manages communication with the media session
// helper to populate the "audio focus" tab.
class MediaInternalsAudioFocusHelper
    : public media_session::mojom::AudioFocusObserver {
 public:
  MediaInternalsAudioFocusHelper();

  MediaInternalsAudioFocusHelper(const MediaInternalsAudioFocusHelper&) =
      delete;
  MediaInternalsAudioFocusHelper& operator=(
      const MediaInternalsAudioFocusHelper&) = delete;

  ~MediaInternalsAudioFocusHelper() override;

  // Sends all audio focus information to media internals.
  void SendAudioFocusState();

  // AudioFocusObserver implementation.
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnRequestIdReleased(const base::UnguessableToken&) override {}

  // Sets whether we should listen to audio focus events.
  void SetEnabled(bool enabled);

 private:
  bool EnsureServiceConnection();
  void OnMojoError();
  void OnDebugMojoError();

  // Called when we receive the list of audio focus requests to display.
  void DidGetAudioFocusRequestList(
      std::vector<media_session::mojom::AudioFocusRequestStatePtr>);

  // Called when we receive audio focus debug info to display for a single
  // audio focus request.
  void DidGetAudioFocusDebugInfo(
      const std::string& id,
      media_session::mojom::MediaSessionDebugInfoPtr info);

  void SerializeAndSendUpdate(std::string_view function,
                              const base::Value::Dict& value);

  // Build the name of the request to display and inject values from |state|.
  std::string BuildNameString(
      const media_session::mojom::AudioFocusRequestStatePtr& state,
      const std::string& provided_name) const;

  // Inject |state| values to display in the state information.
  std::string BuildStateString(
      const media_session::mojom::AudioFocusRequestStatePtr& state,
      const std::string& provided_state) const;

  // Holds a remote to the media session service and it's debug interface.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_;
  mojo::Remote<media_session::mojom::AudioFocusManagerDebug> audio_focus_debug_;

  // Must only be accessed on the UI thread.
  base::Value::Dict audio_focus_data_;
  std::map<std::string, media_session::mojom::AudioFocusRequestStatePtr>
      request_state_;

  bool enabled_ = false;

  mojo::Receiver<media_session::mojom::AudioFocusObserver> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_AUDIO_FOCUS_HELPER_H_
