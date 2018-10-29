// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_AUDIO_FOCUS_HELPER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_AUDIO_FOCUS_HELPER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace content {

// MediaInternalsAudioFocusHelper manages communication with the media session
// helper to populate the "audio focus" tab.
class MediaInternalsAudioFocusHelper
    : public media_session::mojom::AudioFocusObserver {
 public:
  MediaInternalsAudioFocusHelper();
  ~MediaInternalsAudioFocusHelper() override;

  // Sends all audio focus information to media internals.
  void SendAudioFocusState();

  // AudioFocusObserver implementation.
  void OnFocusGained(media_session::mojom::MediaSessionInfoPtr media_session,
                     media_session::mojom::AudioFocusType type) override;
  void OnFocusLost(
      media_session::mojom::MediaSessionInfoPtr media_session) override;

  // Sets whether we should listen to audio focus events.
  void SetEnabled(bool enabled);

 private:
  void EnsureServiceConnection();
  void OnMojoError();

  // Called when we receive the list of audio focus requests to display.
  void DidGetAudioFocusRequestList(
      std::vector<media_session::mojom::AudioFocusRequestStatePtr>);

  // Called when we receive audio focus debug info to display for a single
  // audio focus request.
  void DidGetAudioFocusDebugInfo(
      const std::string& id,
      media_session::mojom::MediaSessionDebugInfoPtr info);

  bool CanUpdate() const;
  void SerializeAndSendUpdate(const std::string& function,
                              const base::Value* value);

  // Holds a pointer to the media session service and it's debug interface.
  media_session::mojom::AudioFocusManagerPtr audio_focus_ptr_;
  media_session::mojom::AudioFocusManagerDebugPtr audio_focus_debug_ptr_;

  // Must only be accessed on the UI thread.
  base::DictionaryValue audio_focus_data_;

  bool enabled_ = false;

  mojo::Binding<media_session::mojom::AudioFocusObserver> binding_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaInternalsAudioFocusHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_AUDIO_FOCUS_HELPER_H_
