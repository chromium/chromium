// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_player_renderer.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/android/scoped_surface_request_manager.h"
#include "content/browser/media/android/media_player_renderer_web_contents_observer.h"
#include "content/browser/media/android/media_resource_getter_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "media/base/android/media_service_throttler.h"
#include "media/base/timestamp_constants.h"

namespace content {

namespace {

media::MediaUrlInterceptor* g_media_url_interceptor = nullptr;
const float kDefaultVolume = 1.0;

}  // namespace

MediaPlayerRenderer::MediaPlayerRenderer(
    int process_id,
    int routing_id,
    WebContents* web_contents,
    mojo::PendingReceiver<RendererExtension> renderer_extension_receiver,
    mojo::PendingRemote<ClientExtension> client_extension_remote)
    : client_extension_(std::move(client_extension_remote)),
      render_process_id_(process_id),
      routing_id_(routing_id),
      has_error_(false),
      volume_(kDefaultVolume),
      renderer_extension_receiver_(this,
                                   std::move(renderer_extension_receiver)) {
  DCHECK_EQ(WebContents::FromRenderFrameHost(
                RenderFrameHost::FromID(process_id, routing_id)),
            web_contents);

  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  web_contents_muted_ = web_contents_impl && web_contents_impl->IsAudioMuted();

  if (web_contents) {
    MediaPlayerRendererWebContentsObserver::CreateForWebContents(web_contents);
    web_contents_observer_ =
        MediaPlayerRendererWebContentsObserver::FromWebContents(web_contents);
    if (web_contents_observer_)
      web_contents_observer_->AddMediaPlayerRenderer(this);
  }
}

MediaPlayerRenderer::~MediaPlayerRenderer() {
  CancelScopedSurfaceRequest();
  if (web_contents_observer_)
    web_contents_observer_->RemoveMediaPlayerRenderer(this);
}

void MediaPlayerRenderer::Initialize(media::MediaResource* media_resource,
                                     media::RendererClient* client,
                                     media::PipelineStatusCallback init_cb) {
  DVLOG(1) << __func__;

  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  renderer_client_ = client;

  if (media_resource->GetType() != media::MediaResource::Type::KUrl) {
    DLOG(ERROR) << "MediaResource is not of Type URL";
    std::move(init_cb).Run(media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  base::TimeDelta creation_delay =
      media::MediaServiceThrottler::GetInstance()->GetDelayForClientCreation();

  if (creation_delay.is_zero()) {
    CreateMediaPlayer(media_resource->GetMediaUrlParams(), std::move(init_cb));
    return;
  }

  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MediaPlayerRenderer::CreateMediaPlayer,
                     weak_factory_.GetWeakPtr(),
                     media_resource->GetMediaUrlParams(), std::move(init_cb)),
      creation_delay);
}

void MediaPlayerRenderer::CreateMediaPlayer(
    const media::MediaUrlParams& url_params,
    media::PipelineStatusCallback init_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Force the initialization of |media_resource_getter_| first. If it fails,
  // the RenderFrameHost may have been destroyed already.
  if (!GetMediaResourceGetter()) {
    DLOG(ERROR) << "Unable to retrieve MediaResourceGetter";
    std::move(init_cb).Run(media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  const std::string user_agent = GetContentClient()->browser()->GetUserAgent();

  media_player_ = std::make_unique<media::MediaPlayerBridge>(
      url_params.media_url, url_params.site_for_cookies,
      url_params.top_frame_origin, url_params.storage_access_api_status,
      user_agent,
      false,  // hide_url_log
      this,   // MediaPlayerBridge::Client
      url_params.allow_credentials, url_params.is_hls, url_params.headers);

  media_player_->Initialize();
  UpdateVolume();

  std::move(init_cb).Run(media::PIPELINE_OK);
}

void MediaPlayerRenderer::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {}

void MediaPlayerRenderer::Flush(base::OnceClosure flush_cb) {
  DVLOG(3) << __func__;
  std::move(flush_cb).Run();
}

void MediaPlayerRenderer::StartPlayingFrom(base::TimeDelta time) {
  // MediaPlayerBridge's Start() is idempotent, except when it has encountered
  // an error (in which case, calling Start() again is logged as a new error).
  if (has_error_)
    return;

  media_player_->SeekTo(time);
  media_player_->Start();

  // WMPI needs to receive a BUFFERING_HAVE_ENOUGH data before sending a
  // playback_rate > 0. The MediaPlayer manages its own buffering and will pause
  // internally if ever it runs out of data. Sending BUFFERING_HAVE_ENOUGH here
  // is always safe.
  //
  // NOTE: OnBufferingUpdate is triggered whenever the media has buffered or
  // played up to a % value between 1-100, and it's not a reliable indicator of
  // the buffering state.
  //
  // TODO(tguilbert): Investigate the effect of this call on UMAs.
  renderer_client_->OnBufferingStateChange(
      media::BUFFERING_HAVE_ENOUGH, media::BUFFERING_CHANGE_REASON_UNKNOWN);
}

void MediaPlayerRenderer::SetPlaybackRate(double playback_rate) {
  if (has_error_)
    return;

  if (playback_rate == 0) {
    media_player_->Pause();
  } else {
    media_player_->SetPlaybackRate(playback_rate);
    // MediaPlayerBridge's Start() is idempotent.
    media_player_->Start();
  }
}

void MediaPlayerRenderer::OnScopedSurfaceRequestCompleted(
    gl::ScopedJavaSurface surface) {
  DCHECK(surface_request_token_);
  surface_request_token_ = base::UnguessableToken();
  media_player_->SetVideoSurface(std::move(surface));
}

void MediaPlayerRenderer::InitiateScopedSurfaceRequest(
    InitiateScopedSurfaceRequestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CancelScopedSurfaceRequest();

  surface_request_token_ =
      ScopedSurfaceRequestManager::GetInstance()->RegisterScopedSurfaceRequest(
          base::BindOnce(&MediaPlayerRenderer::OnScopedSurfaceRequestCompleted,
                         weak_factory_.GetWeakPtr()));

  std::move(callback).Run(surface_request_token_);
}

void MediaPlayerRenderer::SetVolume(float volume) {
  volume_ = volume;
  UpdateVolume();
}

void MediaPlayerRenderer::UpdateVolume() {
  float volume = web_contents_muted_ ? 0 : volume_;
  if (media_player_)
    media_player_->SetVolume(volume);
}

base::TimeDelta MediaPlayerRenderer::GetMediaTime() {
  return media_player_->GetCurrentTime();
}

media::RendererType MediaPlayerRenderer::GetRendererType() {
  return media::RendererType::kMediaPlayer;
}

media::MediaResourceGetter* MediaPlayerRenderer::GetMediaResourceGetter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!media_resource_getter_.get()) {
    RenderProcessHost* host = RenderProcessHost::FromID(render_process_id_);

    // The RenderFrameHost/RenderProcessHost may have been destroyed already,
    // as there might be a delay between the frame closing and
    // MojoRendererService receiving a connection closing error.
    if (!host)
      return nullptr;

    BrowserContext* context = host->GetBrowserContext();
    media_resource_getter_ = std::make_unique<MediaResourceGetterImpl>(
        context, render_process_id_, routing_id_);
  }
  return media_resource_getter_.get();
}

media::MediaUrlInterceptor* MediaPlayerRenderer::GetMediaUrlInterceptor() {
  return g_media_url_interceptor;
}

void MediaPlayerRenderer::OnMediaDurationChanged(base::TimeDelta duration) {
  // For HLS streams, the reported duration may be zero for infinite streams.
  // See http://crbug.com/501213.
  if (duration.is_zero())
    duration = media::kInfiniteDuration;

  if (duration_ != duration) {
    duration_ = duration;
    client_extension_->OnDurationChange(duration);
  }
}

void MediaPlayerRenderer::OnPlaybackComplete() {
  renderer_client_->OnEnded();
}

void MediaPlayerRenderer::OnError(int error) {
  // Some errors are forwarded to the MediaPlayerListener, but are of no
  // importance to us. Ignore these errors, which are reported as
  // MEDIA_ERROR_INVALID_CODE by MediaPlayerListener.
  if (error ==
      media::MediaPlayerBridge::MediaErrorType::MEDIA_ERROR_INVALID_CODE) {
    return;
  }

  LOG(ERROR) << __func__ << " Error: " << error;
  has_error_ = true;
  renderer_client_->OnError(media::PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED);
}

void MediaPlayerRenderer::OnVideoSizeChanged(int width, int height) {
  // This method is called when we find a video size from metadata or when
  // |media_player|'s size actually changes.
  // We therefore may already have the latest video size.
  gfx::Size new_size = gfx::Size(width, height);
  if (video_size_ != new_size) {
    video_size_ = new_size;
    // Send via |client_extension_| instead of |renderer_client_|, so
    // MediaPlayerRendererClient can update its texture size.
    // MPRClient will then continue propagating changes via its RendererClient.
    client_extension_->OnVideoSizeChange(video_size_);
  }
}

void MediaPlayerRenderer::OnUpdateAudioMutingState(bool muted) {
  web_contents_muted_ = muted;
  UpdateVolume();
}

void MediaPlayerRenderer::OnWebContentsDestroyed() {
  web_contents_observer_ = nullptr;
}

// static
void MediaPlayerRenderer::RegisterMediaUrlInterceptor(
    media::MediaUrlInterceptor* media_url_interceptor) {
  g_media_url_interceptor = media_url_interceptor;
}

void MediaPlayerRenderer::CancelScopedSurfaceRequest() {
  if (!surface_request_token_)
    return;

  ScopedSurfaceRequestManager::GetInstance()->UnregisterScopedSurfaceRequest(
      surface_request_token_);
  surface_request_token_ = base::UnguessableToken();
}

}  // namespace content
