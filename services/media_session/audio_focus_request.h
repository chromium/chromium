// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_AUDIO_FOCUS_REQUEST_H_
#define SERVICES_MEDIA_SESSION_AUDIO_FOCUS_REQUEST_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace media_session {

using GetMediaImageBitmapCallback = base::OnceCallback<void(const SkBitmap&)>;

class AudioFocusManager;
struct EnforcementState;
class MediaController;

class AudioFocusRequest : public mojom::AudioFocusRequestClient {
 public:
  AudioFocusRequest(
      base::WeakPtr<AudioFocusManager> owner,
      mojo::PendingReceiver<mojom::AudioFocusRequestClient> receiver,
      mojo::PendingRemote<mojom::MediaSession> session,
      mojom::MediaSessionInfoPtr session_info,
      mojom::AudioFocusType audio_focus_type,
      const base::UnguessableToken& id,
      const std::string& source_name,
      const base::UnguessableToken& group_id,
      const base::UnguessableToken& identity);

  ~AudioFocusRequest() override;

  // mojom::AudioFocusRequestClient.
  void RequestAudioFocus(mojom::MediaSessionInfoPtr session_info,
                         mojom::AudioFocusType type,
                         RequestAudioFocusCallback callback) override;
  void AbandonAudioFocus() override;
  void MediaSessionInfoChanged(mojom::MediaSessionInfoPtr info) override;

  // The current audio focus type that this request has.
  mojom::AudioFocusType audio_focus_type() const { return audio_focus_type_; }
  void set_audio_focus_type(mojom::AudioFocusType type) {
    audio_focus_type_ = type;
  }

  // Returns whether the underyling media session is currently suspended.
  bool IsSuspended() const;

  // Returns the state of this audio focus request.
  mojom::AudioFocusRequestStatePtr ToAudioFocusRequestState() const;

  // Bind a mojo media controller to control the underlying media session.
  void BindToMediaController(
      mojo::PendingReceiver<mojom::MediaController> receiver);

  // Suspends the underlying media session.
  void Suspend(const EnforcementState& state);

  // If the underlying media session previously suspended this session then this
  // will resume it and apply any delayed action.
  void ReleaseTransientHold();

  // Perform a UI action (play/pause/stop). This may be delayed if the service
  // has transiently suspended the session.
  void PerformUIAction(mojom::MediaSessionAction action);

  // Retrieves the bitmap associated with a |image|.
  void GetMediaImageBitmap(const MediaImage& image,
                           int minimum_size_px,
                           int desired_size_px,
                           GetMediaImageBitmapCallback callback);

  mojom::MediaSession* ipc() { return session_.get(); }
  const mojom::MediaSessionInfoPtr& info() const { return session_info_; }
  const base::UnguessableToken& id() const { return id_; }
  const std::string& source_name() const { return source_name_; }
  const base::UnguessableToken& group_id() const { return group_id_; }
  const base::UnguessableToken& identity() const { return identity_; }

 private:
  void SetSessionInfo(mojom::MediaSessionInfoPtr session_info);
  void OnConnectionError();

  void OnImageDownloaded(GetMediaImageBitmapCallback callback,
                         const SkBitmap& bitmap);

  bool encountered_error_ = false;
  bool was_suspended_ = false;

  std::unique_ptr<MediaController> controller_;

  mojo::Remote<mojom::MediaSession> session_;
  mojom::MediaSessionInfoPtr session_info_;
  mojom::AudioFocusType audio_focus_type_;

  mojo::Receiver<mojom::AudioFocusRequestClient> receiver_;

  // The action to apply when the transient hold is released.
  base::Optional<mojom::MediaSessionAction> delayed_action_;

  // The ID of the audio focus request.
  base::UnguessableToken const id_;

  // The name of the source that created this audio focus request (used for
  // metrics).
  std::string const source_name_;

  // The group ID of the audio focus request.
  base::UnguessableToken const group_id_;

  // The identity that requested audio focus.
  base::UnguessableToken const identity_;

  // Weak pointer to the owning |AudioFocusManager| instance.
  const base::WeakPtr<AudioFocusManager> owner_;

  DISALLOW_COPY_AND_ASSIGN(AudioFocusRequest);
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_AUDIO_FOCUS_REQUEST_H_
