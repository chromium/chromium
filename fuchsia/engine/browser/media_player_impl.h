// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_MEDIA_PLAYER_IMPL_H_
#define FUCHSIA_ENGINE_BROWSER_MEDIA_PLAYER_IMPL_H_

#include <fuchsia/media/sessions2/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/interface_request.h>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "fuchsia/engine/web_engine_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace content {
class MediaSession;
}

class WEB_ENGINE_EXPORT MediaPlayerImpl
    : public fuchsia::media::sessions2::Player,
      public media_session::mojom::MediaSessionObserver {
 public:
  // |media_session| must out-live |this|.
  // |on_disconnect| will be invoked when |request| disconnects, and should
  // clean up |this|, and any references to it.
  MediaPlayerImpl(
      content::MediaSession* media_session,
      fidl::InterfaceRequest<fuchsia::media::sessions2::Player> request,
      base::OnceClosure on_disconnect);
  ~MediaPlayerImpl() final;

  MediaPlayerImpl(const MediaPlayerImpl&) = delete;
  MediaPlayerImpl& operator=(const MediaPlayerImpl&) = delete;

  // fuchsia::media::sessions2::Player implementation.
  void WatchInfoChange(WatchInfoChangeCallback info_change_callback) final;

  // fuchsia::media::sessions2::PlayerControl implementation.
  void Play() final;
  void Pause() final;
  void Stop() final;
  void Seek(zx_duration_t position) final;
  void SkipForward() final;
  void SkipReverse() final;
  void NextItem() final;
  void PrevItem() final;
  void SetPlaybackRate(float playback_rate) final;
  void SetRepeatMode(fuchsia::media::sessions2::RepeatMode repeat_mode) final;
  void SetShuffleMode(bool shuffle_on) final;
  void BindVolumeControl(
      fidl::InterfaceRequest<fuchsia::media::audio::VolumeControl>
          volume_control_request) final;

 private:
  // media_session::mojom::MediaSessionObserver implementation.
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr info) final;
  void MediaSessionMetadataChanged(
      const absl::optional<media_session::MediaMetadata>& metadata) final;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& action)
      final;
  void MediaSessionImagesChanged(
      const base::flat_map<media_session::mojom::MediaSessionImageType,
                           std::vector<media_session::MediaImage>>& images)
      final;
  void MediaSessionPositionChanged(
      const absl::optional<media_session::MediaPosition>& position) final;

  // Sends changes accumulated in |pending_info_delta_|, if any, to the
  // |pending_info_change_callback_|, if it is set.
  void MaybeSendPlayerInfoDelta();

  // Reports the specified |status| to the client and calls |on_disconnect_|.
  void ReportErrorAndDisconnect(zx_status_t status);

  content::MediaSession* const media_session_;

  // Invoked when |binding_| becomes disconnected.
  base::OnceClosure on_disconnect_;

  // Binding through which control requests are received from the client.
  fidl::Binding<fuchsia::media::sessions2::Player> binding_;

  // Binding through which notifications are received from the MediaSession.
  mojo::Receiver<media_session::mojom::MediaSessionObserver> observer_receiver_;

  // Pending PlayerInfo deltas and info-change callback.
  WatchInfoChangeCallback pending_info_change_callback_;
  fuchsia::media::sessions2::PlayerInfoDelta pending_info_delta_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_MEDIA_PLAYER_IMPL_H_
