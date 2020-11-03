// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_MEDIA_SESSION_SERVICE_H_
#define SERVICES_MEDIA_SESSION_MEDIA_SESSION_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_session_service.mojom.h"

namespace media_session {

class AudioFocusManager;

class MediaSessionService : public mojom::MediaSessionService {
 public:
  explicit MediaSessionService(
      mojo::PendingReceiver<mojom::MediaSessionService> receiver);
  ~MediaSessionService() override;

  const AudioFocusManager& audio_focus_manager_for_testing() const {
    return *audio_focus_manager_.get();
  }

 private:
  // mojom::MediaSessionService implementation:
  void BindAudioFocusManager(
      mojo::PendingReceiver<mojom::AudioFocusManager> receiver) override;
  void BindAudioFocusManagerDebug(
      mojo::PendingReceiver<mojom::AudioFocusManagerDebug> receiver) override;
  void BindMediaControllerManager(
      mojo::PendingReceiver<mojom::MediaControllerManager> receiver) override;

  mojo::Receiver<mojom::MediaSessionService> receiver_;
  std::unique_ptr<AudioFocusManager> audio_focus_manager_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionService);
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_MEDIA_SESSION_SERVICE_H_
