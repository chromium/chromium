// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_player_renderer.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/task/post_task.h"
#include "content/browser/android/scoped_surface_request_manager.h"
#include "content/browser/media/android/media_player_renderer_web_contents_observer.h"
#include "content/browser/media/android/media_resource_getter_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "media/base/android/media_service_throttler.h"
#include "media/base/timestamp_constants.h"

// TODO(tguilbert): Remove this ID once MediaPlayerManager has been deleted
// and MediaPlayerBridge updated. See comment in header file.
constexpr int kUnusedAndIrrelevantPlayerId = 0;

namespace content {

namespace {

media::MediaUrlInterceptor* g_media_url_interceptor = nullptr;
const float kDefaultVolume = 1.0;

}  // namespace

MediaPlayerRenderer::MediaPlayerRenderer(int process_id,
                                         int routing_id,
                                         WebContents* web_contents)
    : render_process_id_(process_id),
      routing_id_(routing_id),
      has_error_(false),
      volume_(kDefaultVolume),
      weak_factory_(this) {
  DCHECK_EQ(static_cast<RenderFrameHostImpl*>(
                RenderFrameHost::FromID(process_id, routing_id))
                ->delegate()
                ->GetAsWebContents(),
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
                                     const media::PipelineStatusCB& init_cb) {
  DVLOG(1) << __func__;

  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  renderer_client_ = client;

  if (media_resource->GetType() != media::MediaResource::Type::URL) {
    DLOG(ERROR) << "MediaResource is not of Type URL";
    init_cb.Run(media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  base::TimeDelta creation_delay =
      media::MediaServiceThrottler::GetInstance()->GetDelayForClientCreation();

  if (creation_delay.is_zero()) {
    CreateMediaPlayer(media_resource->GetMediaUrlParams(), init_cb);
    return;
  }

  base::PostDelayedTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::Bind(&MediaPlayerRenderer::CreateMediaPlayer,
                 weak_factory_.GetWeakPtr(),
                 media_resource->GetMediaUrlParams(), init_cb),
      creation_delay);
}

void MediaPlayerRenderer::CreateMediaPlayer(
    const media::MediaUrlParams& url_params,
    const media::PipelineStatusCB& init_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Force the initialization of |media_resource_getter_| first. If it fails,
  // the RenderFrameHost may have been destroyed already.
  if (!GetMediaResourceGetter()) {
    DLOG(ERROR) << "Unable to retrieve MediaResourceGetter";
    init_cb.Run(media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  const std::string user_agent = GetContentClient()->GetUserAgent();

  media_player_.reset(new media::MediaPlayerBridge(
      kUnusedAndIrrelevantPlayerId, url_params.media_url,
      url_params.site_for_cookies, user_agent,
      false,  // hide_url_log
      this,
      base::Bind(&MediaPlayerRenderer::OnDecoderResourcesReleased,
                 weak_factory_.GetWeakPtr()),
      GURL(),  // frame_url
      true));  // allow_crendentials

  media_player_->Initialize();
  UpdateVolume();

  init_cb.Run(media::PIPELINE_OK);
}

void MediaPlayerRenderer::SetCdm(media::CdmContext* cdm_context,
                                 const media::CdmAttachedCB& cdm_attached_cb) {
  NOTREACHED();
}

void MediaPlayerRenderer::Flush(const base::Closure& flush_cb) {
  DVLOG(3) << __func__;
  flush_cb.Run();
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
  renderer_client_->OnBufferingStateChange(media::BUFFERING_HAVE_ENOUGH);
}

void MediaPlayerRenderer::SetPlaybackRate(double playback_rate) {
  if (has_error_)
    return;

  if (playback_rate == 0) {
    media_player_->Pause(true);
  } else {
    // MediaPlayerBridge's Start() is idempotent.
    media_player_->Start();

    // TODO(tguilbert): MediaPlayer's interface allows variable playback rate,
    // but is not currently exposed in the MediaPlayerBridge interface.
    // Investigate wether or not we want to add variable playback speed.
  }
}

void MediaPlayerRenderer::OnScopedSurfaceRequestCompleted(
    gl::ScopedJavaSurface surface) {
  DCHECK(surface_request_token_);
  surface_request_token_ = base::UnguessableToken();
  media_player_->SetVideoSurface(std::move(surface));
}

base::UnguessableToken MediaPlayerRenderer::InitiateScopedSurfaceRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CancelScopedSurfaceRequest();

  surface_request_token_ =
      ScopedSurfaceRequestManager::GetInstance()->RegisterScopedSurfaceRequest(
          base::Bind(&MediaPlayerRenderer::OnScopedSurfaceRequestCompleted,
                     weak_factory_.GetWeakPtr()));

  return surface_request_token_;
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
    StoragePartition* partition = host->GetStoragePartition();
    storage::FileSystemContext* file_system_context =
        partition ? partition->GetFileSystemContext() : nullptr;
    media_resource_getter_.reset(new MediaResourceGetterImpl(
        context, file_system_context, render_process_id_, routing_id_));
  }
  return media_resource_getter_.get();
}

media::MediaUrlInterceptor* MediaPlayerRenderer::GetMediaUrlInterceptor() {
  return g_media_url_interceptor;
}

void MediaPlayerRenderer::OnTimeUpdate(int player_id,
                                       base::TimeDelta current_timestamp,
                                       base::TimeTicks current_time_ticks) {}

void MediaPlayerRenderer::OnMediaMetadataChanged(int player_id,
                                                 base::TimeDelta duration,
                                                 int width,
                                                 int height,
                                                 bool success) {
  // Always try to propage the video size.
  // This call will no-op if |video_size_| is already current.
  OnVideoSizeChanged(kUnusedAndIrrelevantPlayerId, width, height);

  // For HLS streams, the reported duration may be zero for infinite streams.
  // See http://crbug.com/501213.
  if (duration.is_zero())
    duration = media::kInfiniteDuration;

  if (duration_ != duration) {
    duration_ = duration;
    renderer_client_->OnDurationChange(duration);
  }
}

void MediaPlayerRenderer::OnPlaybackComplete(int player_id) {
  renderer_client_->OnEnded();
}

void MediaPlayerRenderer::OnMediaInterrupted(int player_id) {}

void MediaPlayerRenderer::OnBufferingUpdate(int player_id, int percentage) {}

void MediaPlayerRenderer::OnSeekComplete(int player_id,
                                         const base::TimeDelta& current_time) {}

void MediaPlayerRenderer::OnError(int player_id, int error) {
  // Some errors are forwarded to the MediaPlayerListener, but are of no
  // importance to us. Ignore these errors, which are reported as
  // MEDIA_ERROR_INVALID_CODE by MediaPlayerListener.
  if (error ==
      media::MediaPlayerAndroid::MediaErrorType::MEDIA_ERROR_INVALID_CODE) {
    return;
  }

  LOG(ERROR) << __func__ << " Error: " << error;
  has_error_ = true;
  renderer_client_->OnError(media::PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED);
}

void MediaPlayerRenderer::OnVideoSizeChanged(int player_id,
                                             int width,
                                             int height) {
  // This method is called when we find a video size from metadata or when
  // |media_player|'s size actually changes.
  // We therefore may already have the latest video size.
  gfx::Size new_size = gfx::Size(width, height);
  if (video_size_ != new_size) {
    video_size_ = new_size;
    renderer_client_->OnVideoNaturalSizeChange(video_size_);
  }
}

media::MediaPlayerAndroid* MediaPlayerRenderer::GetPlayer(int player_id) {
  NOTREACHED();
  return nullptr;
}

bool MediaPlayerRenderer::RequestPlay(int player_id,
                                      base::TimeDelta duration,
                                      bool has_audio) {
  return true;
}

void MediaPlayerRenderer::OnUpdateAudioMutingState(bool muted) {
  web_contents_muted_ = muted;
  UpdateVolume();
}

void MediaPlayerRenderer::OnWebContentsDestroyed() {
  web_contents_observer_ = nullptr;
}

void MediaPlayerRenderer::OnDecoderResourcesReleased(int player_id) {
  // Since we are not using a pool of MediaPlayerAndroid instances, this
  // function is not relevant.
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
