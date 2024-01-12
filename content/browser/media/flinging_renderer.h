// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_FLINGING_RENDERER_H_
#define CONTENT_BROWSER_MEDIA_FLINGING_RENDERER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "media/base/flinging_controller.h"
#include "media/base/media_resource.h"
#include "media/base/media_status_observer.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace content {

class FlingingRendererTest;
class RenderFrameHost;

// FlingingRenderer adapts from the media::Renderer interface to the
// MediaController interface. The MediaController is used to issue simple media
// playback commands. In this case, the media we are controlling should be an
// already existing RemotingCastSession, which should have been initiated by a
// blink::RemotePlayback object, using the PresentationService.
class CONTENT_EXPORT FlingingRenderer : public media::Renderer,
                                        media::MediaStatusObserver {
 public:
  using ClientExtension = media::mojom::FlingingRendererClientExtension;

  // Helper method to create a FlingingRenderer from an already existing
  // presentation ID.
  // Returns nullptr if there was an error getting the MediaControllor for the
  // given presentation ID.
  static std::unique_ptr<FlingingRenderer> Create(
      RenderFrameHost* render_frame_host,
      const std::string& presentation_id,
      mojo::PendingRemote<ClientExtension> client_extension);

  FlingingRenderer(const FlingingRenderer&) = delete;
  FlingingRenderer& operator=(const FlingingRenderer&) = delete;

  ~FlingingRenderer() override;

  // media::Renderer implementation
  void Initialize(media::MediaResource* media_resource,
                  media::RendererClient* client,
                  media::PipelineStatusCallback init_cb) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  media::RendererType GetRendererType() override;

  // media::MediaStatusObserver implementation.
  void OnMediaStatusUpdated(const media::MediaStatus& status) override;

 private:
  friend class FlingingRendererTest;
  using PlayState = media::MediaStatus::State;

  explicit FlingingRenderer(
      std::unique_ptr<media::FlingingController> controller,
      mojo::PendingRemote<ClientExtension> client_extension);

  void SetExpectedPlayState(PlayState state);

  // The play state that we expect the remote device to reach.
  // Updated whenever WMPI sends play/pause commands.
  PlayState expected_play_state_ = PlayState::kUnknown;

  // True when the remote device has reached the expected play state.
  // False when it is transitioning.
  bool play_state_is_stable_ = false;

  // The last "stable" play state received from the cast device.
  // Only updated when |play_state_is_stable_| is true.
  PlayState last_play_state_received_ = PlayState::kUnknown;

  raw_ptr<media::RendererClient> client_;

  mojo::Remote<ClientExtension> client_extension_;

  std::unique_ptr<media::FlingingController> controller_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_FLINGING_RENDERER_H_
