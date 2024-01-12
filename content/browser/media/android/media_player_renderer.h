// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_PLAYER_RENDERER_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_PLAYER_RENDERER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"
#include "media/base/android/media_player_bridge.h"
#include "media/base/media_log.h"
#include "media/base/media_resource.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace content {

class WebContents;
class MediaPlayerRendererWebContentsObserver;

// MediaPlayerRenderer bridges the media::Renderer and Android MediaPlayer
// interfaces. It owns a MediaPlayerBridge, which exposes c++ methods to call
// into a native Android MediaPlayer.
//
// Each MediaPlayerRenderer is associated with one MediaPlayerRendererClient,
// living in WMPI in the Renderer process.
class CONTENT_EXPORT MediaPlayerRenderer
    : public media::Renderer,
      public media::mojom::MediaPlayerRendererExtension,
      public media::MediaPlayerBridge::Client {
 public:
  using RendererExtension = media::mojom::MediaPlayerRendererExtension;
  using ClientExtension = media::mojom::MediaPlayerRendererClientExtension;

  // Permits embedders to handle custom urls.
  static void RegisterMediaUrlInterceptor(
      media::MediaUrlInterceptor* media_url_interceptor);

  MediaPlayerRenderer(
      int process_id,
      int routing_id,
      WebContents* web_contents,
      mojo::PendingReceiver<RendererExtension> renderer_extension_receiver,
      mojo::PendingRemote<ClientExtension> client_extension_remote);

  MediaPlayerRenderer(const MediaPlayerRenderer&) = delete;
  MediaPlayerRenderer& operator=(const MediaPlayerRenderer&) = delete;

  ~MediaPlayerRenderer() override;

  // media::Renderer implementation
  void Initialize(media::MediaResource* media_resource,
                  media::RendererClient* client,
                  media::PipelineStatusCallback init_cb) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;

  // N.B: MediaPlayerBridge doesn't support variable playback rates (but it
  // could be exposed from MediaPlayer in the future). For the moment:
  // - If |playback_rate| is 0, we pause the video.
  // - For other |playback_rate| values, we start playing at 1x speed.
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  media::RendererType GetRendererType() override;

  // media::MediaPlayerBridge::Client implementation
  media::MediaResourceGetter* GetMediaResourceGetter() override;
  media::MediaUrlInterceptor* GetMediaUrlInterceptor() override;
  void OnMediaDurationChanged(base::TimeDelta duration) override;
  void OnPlaybackComplete() override;
  void OnError(int error) override;
  void OnVideoSizeChanged(int width, int height) override;

  void OnUpdateAudioMutingState(bool muted);
  void OnWebContentsDestroyed();

  // media::mojom::MediaPlayerRendererExtension implementation.
  //
  // Registers a request in the content::ScopedSurfaceRequestManager, and
  // returns the token associated to the request. The token can then be used to
  // complete the request via the gpu::ScopedSurfaceRequestConduit.
  // A completed request will call back to OnScopedSurfaceRequestCompleted().
  //
  // NOTE: If a request is already pending, calling this method again will
  // safely cancel the pending request before registering a new one.
  void InitiateScopedSurfaceRequest(
      InitiateScopedSurfaceRequestCallback callback) override;

  void OnScopedSurfaceRequestCompleted(gl::ScopedJavaSurface surface);

 private:
  void CreateMediaPlayer(const media::MediaUrlParams& params,
                         media::PipelineStatusCallback init_cb);

  // Cancels the pending request started by InitiateScopedSurfaceRequest(), if
  // it exists. No-ops otherwise.
  void CancelScopedSurfaceRequest();

  void UpdateVolume();

  mojo::Remote<ClientExtension> client_extension_;

  // Identifiers to find the RenderFrameHost that created |this|.
  // NOTE: We store these IDs rather than a RenderFrameHost* because we do not
  // know when the RenderFrameHost is destroyed.
  int render_process_id_;
  int routing_id_;

  raw_ptr<media::RendererClient> renderer_client_;

  std::unique_ptr<media::MediaPlayerBridge> media_player_;

  // Current duration of the media.
  base::TimeDelta duration_;

  // Indicates if a serious error has been encountered by the |media_player_|.
  bool has_error_;

  gfx::Size video_size_;

  base::UnguessableToken surface_request_token_;

  std::unique_ptr<media::MediaResourceGetter> media_resource_getter_;

  bool web_contents_muted_;
  raw_ptr<MediaPlayerRendererWebContentsObserver> web_contents_observer_;
  float volume_;

  mojo::Receiver<MediaPlayerRendererExtension> renderer_extension_receiver_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaPlayerRenderer> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_PLAYER_RENDERER_H_
