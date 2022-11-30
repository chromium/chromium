// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_MEDIA_SESSION_SERVICE_IMPL_H_
#define SERVICES_MEDIA_SESSION_MEDIA_SESSION_SERVICE_IMPL_H_

#include <memory>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/cpp/media_session_service.h"

namespace media_session {

class AudioFocusManager;

class MediaSessionServiceImpl : public MediaSessionService {
 public:
  MediaSessionServiceImpl();
  ~MediaSessionServiceImpl() override;
  MediaSessionServiceImpl(const MediaSessionServiceImpl&) = delete;
  MediaSessionServiceImpl& operator=(const MediaSessionServiceImpl&) = delete;

  // MediaSessionService implementation:
  void BindAudioFocusManager(
      mojo::PendingReceiver<mojom::AudioFocusManager> receiver) override;
  void BindAudioFocusManagerDebug(
      mojo::PendingReceiver<mojom::AudioFocusManagerDebug> receiver) override;
  void BindMediaControllerManager(
      mojo::PendingReceiver<mojom::MediaControllerManager> receiver) override;

  const AudioFocusManager& audio_focus_manager_for_testing() const {
    return *audio_focus_manager_.get();
  }

 private:
  std::unique_ptr<AudioFocusManager> audio_focus_manager_;
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_MEDIA_SESSION_SERVICE_IMPL_H_
