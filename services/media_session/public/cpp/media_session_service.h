// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_SESSION_SERVICE_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_SESSION_SERVICE_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace media_session {

namespace mojom {
class AudioFocusManager;
}  // namespace mojom

class COMPONENT_EXPORT(MEDIA_SESSION_CPP) MediaSessionService {
 public:
  virtual ~MediaSessionService() = default;

  virtual void BindAudioFocusManager(
      mojo::PendingReceiver<mojom::AudioFocusManager> receiver) = 0;
  virtual void BindAudioFocusManagerDebug(
      mojo::PendingReceiver<mojom::AudioFocusManagerDebug> receiver) = 0;
  virtual void BindMediaControllerManager(
      mojo::PendingReceiver<mojom::MediaControllerManager> receiver) = 0;

 protected:
  MediaSessionService() = default;
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_SESSION_SERVICE_H_
