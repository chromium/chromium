// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_MEDIA_PLAYER_IMPL_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_MEDIA_PLAYER_IMPL_H_

#include <fidl/fuchsia.media.sessions2/cpp/fidl.h>

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "fuchsia_web/webengine/web_engine_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace content {
class MediaSession;
}

class WEB_ENGINE_EXPORT MediaPlayerImpl final
    : public fidl::Server<fuchsia_media_sessions2::Player>,
      public media_session::mojom::MediaSessionObserver {
 public:
  // |media_session| must out-live |this|.
  // |on_disconnect| will be invoked when |request| disconnects, and should
  // clean up |this|, and any references to it.
  MediaPlayerImpl(content::MediaSession* media_session,
                  fidl::ServerEnd<fuchsia_media_sessions2::Player> server_end,
                  base::OnceClosure on_disconnect);
  ~MediaPlayerImpl() override;

  MediaPlayerImpl(const MediaPlayerImpl&) = delete;
  MediaPlayerImpl& operator=(const MediaPlayerImpl&) = delete;

  // fuchsia_media_sessions2::Player implementation.
  void WatchInfoChange(WatchInfoChangeCompleter::Sync& completer) override;

  // fuchsia_media_sessions2::PlayerControl implementation.
  void Play(PlayCompleter::Sync& ignored_completer) override;
  void Pause(PauseCompleter::Sync& ignored_completer) override;
  void Stop(StopCompleter::Sync& ignored_completer) override;
  void Seek(SeekRequest& request,
            SeekCompleter::Sync& ignored_completer) override;
  void SkipForward(SkipForwardCompleter::Sync& ignored_completer) override;
  void SkipReverse(SkipReverseCompleter::Sync& ignored_completer) override;
  void NextItem(NextItemCompleter::Sync& ignored_completer) override;
  void PrevItem(PrevItemCompleter::Sync& ignored_completer) override;
  void SetPlaybackRate(
      SetPlaybackRateRequest& request,
      SetPlaybackRateCompleter::Sync& ignored_completer) override;
  void SetRepeatMode(SetRepeatModeRequest& request,
                     SetRepeatModeCompleter::Sync& ignored_completer) override;
  void SetShuffleMode(
      SetShuffleModeRequest& request,
      SetShuffleModeCompleter::Sync& ignored_completer) override;
  void BindVolumeControl(
      BindVolumeControlRequest& request,
      BindVolumeControlCompleter::Sync& ignored_completer) override;

 private:
  // media_session::mojom::MediaSessionObserver implementation.
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr info) override;
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& action)
      override;
  void MediaSessionImagesChanged(
      const base::flat_map<media_session::mojom::MediaSessionImageType,
                           std::vector<media_session::MediaImage>>& images)
      override;
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override;

  // Sends changes accumulated in |pending_info_delta_|, if any, to the
  // |pending_info_change_callback_|, if it is set.
  void MaybeSendPlayerInfoDelta();

  void OnBindingClosure(fidl::UnbindInfo info);

  // Reports the specified |status| to the client and calls |on_disconnect_|.
  void ReportErrorAndDisconnect(zx_status_t status);

  content::MediaSession* const media_session_;

  // Invoked when |binding_| becomes disconnected.
  base::OnceClosure on_disconnect_;

  // Binding through which control requests are received from the client.
  fidl::ServerBinding<fuchsia_media_sessions2::Player> binding_;

  // Binding through which notifications are received from the MediaSession.
  mojo::Receiver<media_session::mojom::MediaSessionObserver> observer_receiver_;

  // Pending PlayerInfo deltas and info-change callback.
  std::optional<WatchInfoChangeCompleter::Async> pending_info_change_callback_;
  fuchsia_media_sessions2::PlayerInfoDelta pending_info_delta_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_MEDIA_PLAYER_IMPL_H_
