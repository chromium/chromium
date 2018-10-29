// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_MEDIA_CONTROLLER_H_
#define SERVICES_MEDIA_SESSION_MEDIA_CONTROLLER_H_

#include <memory>

#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace media_session {

// MediaController provides a control surface over Mojo for controlling a
// specific MediaSession. If |session_| is nullptr then all commands will be
// dropped. MediaController is also a MediaSessionObserver and will forward
// events to added observers.
class MediaController : public mojom::MediaController,
                        public mojom::MediaSessionObserver {
 public:
  MediaController();
  ~MediaController() override;

  // mojom::MediaController overrides.
  void Suspend() override;
  void Resume() override;
  void ToggleSuspendResume() override;
  void AddObserver(mojom::MediaSessionObserverPtr) override;
  void PreviousTrack() override;
  void NextTrack() override;

  // mojom::MediaSessionObserver overrides.
  void MediaSessionInfoChanged(mojom::MediaSessionInfoPtr) override;

  void SetMediaSession(mojom::MediaSession*);
  void ClearMediaSession();

  void BindToInterface(mojom::MediaControllerRequest);
  void FlushForTesting();

 private:
  // Holds mojo bindings for mojom::MediaController.
  mojo::BindingSet<mojom::MediaController> bindings_;

  // The current info for the |session_|.
  mojom::MediaSessionInfoPtr session_info_;

  // Raw pointer to the local proxy. This is used for sending control events to
  // the underlying MediaSession.
  mojom::MediaSession* session_ = nullptr;

  // Observers that are observing |session_|.
  mojo::InterfacePtrSet<mojom::MediaSessionObserver> observers_;

  // Binding for |this| to act as an observer to |session_|.
  mojo::Binding<mojom::MediaSessionObserver> session_binding_{this};

  // Protects |session_| as it is not thread safe.
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(MediaController);
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_MEDIA_CONTROLLER_H_
