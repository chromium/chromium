// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_SESSION_H_
#define CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_SESSION_H_

#include "content/public/browser/media_player_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"

namespace content {

class PictureInPictureServiceImpl;
class PictureInPictureWindowControllerImpl;
class WebContentsImpl;

// The PicutreInPictureSession communicates with the
// PictureInPictureWindowController and the WebContents. It is created by the
// PictureInPictureService but deletes itself. When created, the session will
// enter Picture-in-Picture and when deleted, it will automatically exit
// Picture-in-Picture unless another session became active.
// The session MUST be stopped before its dtor runs to avoid unexpected
// deletion.
class PictureInPictureSession : public blink::mojom::PictureInPictureSession {
 public:
  PictureInPictureSession(
      PictureInPictureServiceImpl* service,
      const MediaPlayerId& player_id,
      const base::Optional<viz::SurfaceId>& surface_id,
      const gfx::Size& natural_size,
      bool show_play_pause_button,
      mojo::PendingReceiver<blink::mojom::PictureInPictureSession> receiver,
      mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
          observer,
      gfx::Size* window_size);
  ~PictureInPictureSession() override;

  // blink::mojom::PictureInPictureSession interface.
  void Stop(StopCallback callback) final;
  void Update(uint32_t player_id,
              const base::Optional<viz::SurfaceId>& surface_id,
              const gfx::Size& natural_size,
              bool show_play_pause_button) final;

  void NotifyWindowResized(const gfx::Size& size);

  // Returns the player that is currently in Picture-in-Picture. Returns nullopt
  // if there are none.
  const base::Optional<MediaPlayerId>& player_id() const { return player_id_; }

  // Shuts down the session. Called by the window controller when the window is
  // closed.
  void Shutdown();

 private:
  PictureInPictureSession() = delete;

  // Exits Picture-in-Picture, notifies the PictureInPictureWindowController of
  // change of active session and deletes self.
  void StopInternal(StopCallback callback);

  // Called when the |receiver_| hits a connection error.
  void OnConnectionError();

  // Returns the WebContentsImpl associated with this Picture-in-Picture
  // session. It relies on the WebContents associated with the |service_|.
  WebContentsImpl* GetWebContentsImpl();

  // Returns the Picture-in-Picture window controller associated with the
  // session.
  PictureInPictureWindowControllerImpl& GetController();

  // Owns |this|.
  PictureInPictureServiceImpl* service_;

  mojo::Receiver<blink::mojom::PictureInPictureSession> receiver_;

  base::Optional<MediaPlayerId> player_id_;

  // Whether the session is currently stopping. The final stop of stopping is to
  // be destroyed so once its set to true it will never be set back to false and
  // the dtor will check that it's stopping.
  bool is_stopping_ = false;

  mojo::Remote<blink::mojom::PictureInPictureSessionObserver> observer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_SESSION_H_
