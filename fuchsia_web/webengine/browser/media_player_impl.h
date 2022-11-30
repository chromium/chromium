// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_MEDIA_PLAYER_IMPL_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_MEDIA_PLAYER_IMPL_H_

#include <fuchsia/media/sessions2/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/interface_request.h>
#include <string>

#include "base/callback.h"
#include "fuchsia_web/webengine/web_engine_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class MediaSession;
}

class WEB_ENGINE_EXPORT MediaPlayerImpl final
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
  ~MediaPlayerImpl() override;

  MediaPlayerImpl(const MediaPlayerImpl&) = delete;
  MediaPlayerImpl& operator=(const MediaPlayerImpl&) = delete;

  // fuchsia::media::sessions2::Player implementation.
  void WatchInfoChange(WatchInfoChangeCallback info_change_callback) override;

  // fuchsia::media::sessions2::PlayerControl implementation.
  void Play() override;
  void Pause() override;
  void Stop() override;
  void Seek(zx_duration_t position) override;
  void SkipForward() override;
  void SkipReverse() override;
  void NextItem() override;
  void PrevItem() override;
  void SetPlaybackRate(float playback_rate) override;
  void SetRepeatMode(
      fuchsia::media::sessions2::RepeatMode repeat_mode) override;
  void SetShuffleMode(bool shuffle_on) override;
  void BindVolumeControl(
      fidl::InterfaceRequest<fuchsia::media::audio::VolumeControl>
          volume_control_request) override;

 private:
  // media_session::mojom::MediaSessionObserver implementation.
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr info) override;
  void MediaSessionMetadataChanged(
      const absl::optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& action)
      override;
  void MediaSessionImagesChanged(
      const base::flat_map<media_session::mojom::MediaSessionImageType,
                           std::vector<media_session::MediaImage>>& images)
      override;
  void MediaSessionPositionChanged(
      const absl::optional<media_session::MediaPosition>& position) override;

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

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_MEDIA_PLAYER_IMPL_H_
