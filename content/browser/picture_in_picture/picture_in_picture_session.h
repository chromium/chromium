// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_SESSION_H_
#define CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_SESSION_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/media_player_id.h"
#include "media/mojo/mojom/media_player.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"

namespace content {

class PictureInPictureServiceImpl;
class VideoPictureInPictureWindowControllerImpl;
class WebContentsImpl;

// The PictureInPictureSession is used for video Picture-in-Picture mode. It
// communicates with the PictureInPictureWindowController and the
// WebContents. It is created by the VideoPictureInPictureWindowControllerImpl
// which also deletes it. When created, the session will be expected to be
// active (in Picture-in-Picture) and when deleted, it will automatically exit
// Picture-in-Picture unless another session became active.
//
// The session MUST be stopped before its dtor runs to avoid unexpected
// deletion.
class PictureInPictureSession : public blink::mojom::PictureInPictureSession {
 public:
  PictureInPictureSession(
      PictureInPictureServiceImpl* service,
      const MediaPlayerId& player_id,
      mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> player_remote,
      mojo::PendingReceiver<blink::mojom::PictureInPictureSession> receiver,
      mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
          observer);
  ~PictureInPictureSession() override;

  // blink::mojom::PictureInPictureSession interface.
  void Stop(StopCallback callback) final;
  void Update(
      uint32_t player_id,
      mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> player_remote,
      const viz::SurfaceId& surface_id,
      const gfx::Size& natural_size,
      bool show_play_pause_button) final;

  void NotifyWindowResized(const gfx::Size& size);

  // Return an already bound mojo Remote for the MediaPlayer mojo interface.
  mojo::AssociatedRemote<media::mojom::MediaPlayer>& GetMediaPlayerRemote();

  // Returns the player that is currently in Picture-in-Picture.
  const std::optional<MediaPlayerId>& player_id() const { return player_id_; }

  // Stops the session without closing the window. It will prevent the session
  // to later trying to shutdown when the PictureInPictureWindowController is
  // notified.
  void Disconnect();

  // Shuts down the session. Called by the window controller when the window is
  // closed.
  void Shutdown();

  // Returns the PictureInPictureServiceImpl instance associated with this
  // session. It cannot be null.
  PictureInPictureServiceImpl* service() { return service_; }

 private:
  PictureInPictureSession() = delete;

  // Exits Picture-in-Picture, notifies the PictureInPictureWindowController of
  // change of active session and deletes self.
  void StopInternal(StopCallback callback);

  // Called when the |receiver_| hits a connection error.
  void OnConnectionError();

  // Called when |media_player_remote_| is disconnected, typically when the
  // media player is destroyed while the session is still active.
  void OnPlayerGone();

  // Returns the WebContentsImpl associated with this Picture-in-Picture
  // session. It relies on the WebContents associated with the |service_|.
  WebContentsImpl* GetWebContentsImpl();

  // Returns the Picture-in-Picture window controller associated with the
  // session.
  VideoPictureInPictureWindowControllerImpl& GetController();

  // Will notified The VideoPictureInPictureWindowControllerImpl who owns |this|
  // when it gets destroyed in order for |this| to be destroyed too. Indirectly
  // owns |this|.
  raw_ptr<PictureInPictureServiceImpl> service_;

  mojo::Receiver<blink::mojom::PictureInPictureSession> receiver_;

  std::optional<MediaPlayerId> player_id_;

  // Whether the session is currently stopping. The final stop of stopping is to
  // be destroyed so once its set to true it will never be set back to false and
  // the dtor will check that it's stopping.
  bool is_stopping_ = false;

  mojo::AssociatedRemote<media::mojom::MediaPlayer> media_player_remote_;
  mojo::Remote<blink::mojom::PictureInPictureSessionObserver> observer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_SESSION_H_
