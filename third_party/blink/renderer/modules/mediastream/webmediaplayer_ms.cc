// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/mediastream/webmediaplayer_ms.h"

#include <stddef.h>
#include <limits>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/layers/video_frame_provider_client_impl.h"
#include "cc/layers/video_layer.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_content_type.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "media/base/video_transformation.h"
#include "media/base/video_types.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_audio_track.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_element_source_utils.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_renderer.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_video_renderer.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_surface_layer_bridge.h"
#include "third_party/blink/public/web/modules/media/webmediaplayer_util.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_renderer_factory.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_local_frame_wrapper.h"
#include "third_party/blink/renderer/modules/mediastream/webmediaplayer_ms_compositor.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace {

enum class RendererReloadAction {
  KEEP_RENDERER,
  REMOVE_RENDERER,
  NEW_RENDERER
};

bool IsPlayableTrack(const blink::WebMediaStreamTrack& track) {
  return !track.IsNull() && !track.Source().IsNull() &&
         track.Source().GetReadyState() !=
             blink::WebMediaStreamSource::kReadyStateEnded;
}

}  // namespace

namespace WTF {

template <>
struct CrossThreadCopier<viz::SurfaceId>
    : public CrossThreadCopierPassThrough<viz::SurfaceId> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<media::VideoTransformation>
    : public CrossThreadCopierPassThrough<media::VideoTransformation> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

#if defined(OS_WIN)
// Since we do not have native GMB support in Windows, using GMBs can cause a
// CPU regression. This is more apparent and can have adverse affects in lower
// resolution content which are defined by these thresholds, see
// https://crbug.com/835752.
// static
const gfx::Size WebMediaPlayerMS::kUseGpuMemoryBufferVideoFramesMinResolution =
    gfx::Size(1920, 1080);
#endif  // defined(OS_WIN)

// FrameDeliverer is responsible for delivering frames received on
// the IO thread by calling of EnqueueFrame() method of |compositor_|.
//
// It is created on the main thread, but methods should be called and class
// should be destructed on the IO thread.
class WebMediaPlayerMS::FrameDeliverer {
 public:
  using RepaintCB =
      WTF::CrossThreadRepeatingFunction<void(scoped_refptr<media::VideoFrame>)>;
  FrameDeliverer(const base::WeakPtr<WebMediaPlayerMS>& player,
                 RepaintCB enqueue_frame_cb,
                 scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
                 scoped_refptr<base::TaskRunner> worker_task_runner,
                 media::GpuVideoAcceleratorFactories* gpu_factories)
      : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        player_(player),
        enqueue_frame_cb_(std::move(enqueue_frame_cb)),
        media_task_runner_(media_task_runner) {
    DETACH_FROM_THREAD(io_thread_checker_);

    if (gpu_factories && gpu_factories->ShouldUseGpuMemoryBuffersForVideoFrames(
                             true /* for_media_stream */)) {
      gpu_memory_buffer_pool_.reset(new media::GpuMemoryBufferVideoFramePool(
          media_task_runner, worker_task_runner, gpu_factories));
    }
  }

  ~FrameDeliverer() {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    if (gpu_memory_buffer_pool_) {
      DropCurrentPoolTasks();
      media_task_runner_->DeleteSoon(FROM_HERE,
                                     gpu_memory_buffer_pool_.release());
    }
  }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> frame) {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

// On Android, stop passing frames.
#if defined(OS_ANDROID)
    if (render_frame_suspended_)
      return;
#endif  // defined(OS_ANDROID)

    if (!gpu_memory_buffer_pool_) {
      EnqueueFrame(std::move(frame));
      return;
    }

#if defined(OS_WIN)
    const bool skip_creating_gpu_memory_buffer =
        frame->visible_rect().width() <
            kUseGpuMemoryBufferVideoFramesMinResolution.width() ||
        frame->visible_rect().height() <
            kUseGpuMemoryBufferVideoFramesMinResolution.height();
#else
    const bool skip_creating_gpu_memory_buffer = false;
#endif  // defined(OS_WIN)

    // If |render_frame_suspended_|, we can keep passing the frames to keep the
    // latest frame in compositor up to date. However, creating GMB backed
    // frames is unnecessary, because the frames are not going to be shown for
    // the time period.
    if (render_frame_suspended_ || skip_creating_gpu_memory_buffer) {
      EnqueueFrame(std::move(frame));
      // If there are any existing MaybeCreateHardwareFrame() calls, we do not
      // want those frames to be placed after the current one, so just drop
      // them.
      DropCurrentPoolTasks();
      return;
    }

    // |gpu_memory_buffer_pool_| deletion is going to be posted to
    // |media_task_runner_|. base::Unretained() usage is fine since
    // |gpu_memory_buffer_pool_| outlives the task.
    //
    // TODO(crbug.com/964947): Converting this to PostCrossThreadTask requires
    // re-binding a CrossThreadOnceFunction instance.
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &media::GpuMemoryBufferVideoFramePool::MaybeCreateHardwareFrame,
            base::Unretained(gpu_memory_buffer_pool_.get()), std::move(frame),
            media::BindToCurrentLoop(
                base::BindOnce(&FrameDeliverer::EnqueueFrame,
                               weak_factory_for_pool_.GetWeakPtr()))));
  }

  void SetRenderFrameSuspended(bool render_frame_suspended) {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    render_frame_suspended_ = render_frame_suspended;
  }

  RepaintCB GetRepaintCallback() {
    return CrossThreadBindRepeating(&FrameDeliverer::OnVideoFrame,
                                    weak_factory_.GetWeakPtr());
  }

 private:
  friend class WebMediaPlayerMS;

  void EnqueueFrame(scoped_refptr<media::VideoFrame> frame) {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

    {
      bool tracing_enabled = false;
      TRACE_EVENT_CATEGORY_GROUP_ENABLED("media", &tracing_enabled);
      if (tracing_enabled) {
        base::TimeTicks render_time;
        if (frame->metadata()->GetTimeTicks(
                media::VideoFrameMetadata::REFERENCE_TIME, &render_time)) {
          TRACE_EVENT1("media", "EnqueueFrame", "Ideal Render Instant",
                       render_time.ToInternalValue());
        } else {
          TRACE_EVENT0("media", "EnqueueFrame");
        }
      }
    }

    enqueue_frame_cb_.Run(std::move(frame));
  }

  void DropCurrentPoolTasks() {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    DCHECK(gpu_memory_buffer_pool_);

    if (!weak_factory_for_pool_.HasWeakPtrs())
      return;

    //  |gpu_memory_buffer_pool_| deletion is going to be posted to
    //  |media_task_runner_|. CrossThreadUnretained() usage is fine since
    //  |gpu_memory_buffer_pool_| outlives the task.
    PostCrossThreadTask(
        *media_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &media::GpuMemoryBufferVideoFramePool::Abort,
            CrossThreadUnretained(gpu_memory_buffer_pool_.get())));
    weak_factory_for_pool_.InvalidateWeakPtrs();
  }

  bool render_frame_suspended_ = false;

  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const base::WeakPtr<WebMediaPlayerMS> player_;
  RepaintCB enqueue_frame_cb_;

  // Pool of GpuMemoryBuffers and resources used to create hardware frames.
  std::unique_ptr<media::GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool_;
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // Used for DCHECKs to ensure method calls are executed on the correct thread.
  THREAD_CHECKER(io_thread_checker_);

  base::WeakPtrFactory<FrameDeliverer> weak_factory_for_pool_{this};
  base::WeakPtrFactory<FrameDeliverer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FrameDeliverer);
};

WebMediaPlayerMS::WebMediaPlayerMS(
    WebLocalFrame* frame,
    WebMediaPlayerClient* client,
    WebMediaPlayerDelegate* delegate,
    std::unique_ptr<media::MediaLog> media_log,
    std::unique_ptr<WebMediaStreamRendererFactory> factory,
    scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    scoped_refptr<base::TaskRunner> worker_task_runner,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    const WebString& sink_id,
    CreateSurfaceLayerBridgeCB create_bridge_callback,
    std::unique_ptr<WebVideoFrameSubmitter> submitter,
    WebMediaPlayer::SurfaceLayerMode surface_layer_mode)
    : internal_frame_(std::make_unique<MediaStreamInternalFrameWrapper>(frame)),
      network_state_(WebMediaPlayer::kNetworkStateEmpty),
      ready_state_(WebMediaPlayer::kReadyStateHaveNothing),
      buffered_(static_cast<size_t>(0)),
      client_(client),
      delegate_(delegate),
      delegate_id_(0),
      paused_(true),
      video_transformation_(media::kNoTransformation),
      media_log_(std::move(media_log)),
      renderer_factory_(std::move(factory)),
      main_render_task_runner_(std::move(main_render_task_runner)),
      io_task_runner_(std::move(io_task_runner)),
      compositor_task_runner_(std::move(compositor_task_runner)),
      media_task_runner_(std::move(media_task_runner)),
      worker_task_runner_(std::move(worker_task_runner)),
      gpu_factories_(gpu_factories),
      initial_audio_output_device_id_(sink_id),
      volume_(1.0),
      volume_multiplier_(1.0),
      should_play_upon_shown_(false),
      create_bridge_callback_(std::move(create_bridge_callback)),
      submitter_(std::move(submitter)),
      surface_layer_mode_(surface_layer_mode) {
  DVLOG(1) << __func__;
  DCHECK(client);
  DCHECK(delegate_);
  weak_this_ = weak_factory_.GetWeakPtr();
  delegate_id_ = delegate_->AddObserver(this);

  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_CREATED));
}

WebMediaPlayerMS::~WebMediaPlayerMS() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!web_stream_.IsNull())
    web_stream_.RemoveObserver(this);

  // Destruct compositor resources in the proper order.
  get_client()->SetCcLayer(nullptr);
  if (video_layer_) {
    DCHECK(surface_layer_mode_ != WebMediaPlayer::SurfaceLayerMode::kAlways);
    video_layer_->StopUsingProvider();
  }

  if (frame_deliverer_)
    io_task_runner_->DeleteSoon(FROM_HERE, frame_deliverer_.release());

  if (compositor_)
    compositor_->StopUsingProvider();

  if (video_frame_provider_)
    video_frame_provider_->Stop();

  if (audio_renderer_)
    audio_renderer_->Stop();

  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));

  delegate_->PlayerGone(delegate_id_);
  delegate_->RemoveObserver(delegate_id_);
}

WebMediaPlayer::LoadTiming WebMediaPlayerMS::Load(
    LoadType load_type,
    const WebMediaPlayerSource& source,
    CorsMode /*cors_mode*/) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // TODO(acolwell): Change this to DCHECK_EQ(load_type, LoadTypeMediaStream)
  // once Blink-side changes land.
  DCHECK_NE(load_type, kLoadTypeMediaSource);
  web_stream_ = GetWebMediaStreamFromWebMediaPlayerSource(source);
  if (!web_stream_.IsNull())
    web_stream_.AddObserver(this);

  compositor_ = base::MakeRefCounted<WebMediaPlayerMSCompositor>(
      compositor_task_runner_, io_task_runner_, web_stream_,
      std::move(submitter_), surface_layer_mode_, weak_this_);

  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);
  std::string stream_id =
      web_stream_.IsNull() ? std::string() : web_stream_.Id().Utf8();
  media_log_->AddEvent(media_log_->CreateLoadEvent(stream_id));

  frame_deliverer_.reset(new WebMediaPlayerMS::FrameDeliverer(
      weak_this_,
      CrossThreadBindRepeating(&WebMediaPlayerMSCompositor::EnqueueFrame,
                               compositor_),
      media_task_runner_, worker_task_runner_, gpu_factories_));
  video_frame_provider_ = renderer_factory_->GetVideoRenderer(
      web_stream_,
      ConvertToBaseCallback(frame_deliverer_->GetRepaintCallback()),
      io_task_runner_, main_render_task_runner_);

  if (internal_frame_->web_frame()) {
    WebURL url = source.GetAsURL();
    // Report UMA and RAPPOR metrics.
    ReportMetrics(load_type, url, *internal_frame_->web_frame(),
                  media_log_.get());
  }

  audio_renderer_ = renderer_factory_->GetAudioRenderer(
      web_stream_, internal_frame_->web_frame(),
      initial_audio_output_device_id_);

  if (!audio_renderer_)
    WebRtcLogMessage("Warning: Failed to instantiate audio renderer.");

  if (!video_frame_provider_ && !audio_renderer_) {
    SetNetworkState(WebMediaPlayer::kNetworkStateNetworkError);
    return WebMediaPlayer::LoadTiming::kImmediate;
  }

  if (audio_renderer_) {
    audio_renderer_->SetVolume(volume_);
    audio_renderer_->Start();

    // Store the ID of audio track being played in |current_video_track_id_|
    if (!web_stream_.IsNull()) {
      WebVector<WebMediaStreamTrack> audio_tracks = web_stream_.AudioTracks();
      DCHECK_GT(audio_tracks.size(), 0U);
      current_audio_track_id_ = audio_tracks[0].Id();
    }
  }

  if (video_frame_provider_) {
    video_frame_provider_->Start();

    // Store the ID of video track being played in |current_video_track_id_|
    if (!web_stream_.IsNull()) {
      WebVector<WebMediaStreamTrack> video_tracks = web_stream_.VideoTracks();
      DCHECK_GT(video_tracks.size(), 0U);
      current_video_track_id_ = video_tracks[0].Id();
    }
  }
  // When associated with an <audio> element, we don't want to wait for the
  // first video fram to become available as we do for <video> elements
  // (<audio> elements can also be assigned video tracks).
  // For more details, see https://crbug.com/738379
  if (audio_renderer_ &&
      (client_->IsAudioElement() || !video_frame_provider_)) {
    // This is audio-only mode.
    SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);
    SetReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);
  }

  return WebMediaPlayer::LoadTiming::kImmediate;
}

void WebMediaPlayerMS::OnWebLayerUpdated() {}

void WebMediaPlayerMS::RegisterContentsLayer(cc::Layer* layer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(bridge_);

  bridge_->SetContentsOpaque(opaque_);
  client_->SetCcLayer(layer);
}

void WebMediaPlayerMS::UnregisterContentsLayer(cc::Layer* layer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // |client_| will unregister its cc::Layer if given a nullptr.
  client_->SetCcLayer(nullptr);
}

void WebMediaPlayerMS::OnSurfaceIdUpdated(viz::SurfaceId surface_id) {
  // TODO(726619): Handle the behavior when Picture-in-Picture mode is
  // disabled.
  // The viz::SurfaceId may be updated when the video begins playback or when
  // the size of the video changes.
  if (client_)
    client_->OnPictureInPictureStateChange();
}

void WebMediaPlayerMS::TrackAdded(const WebMediaStreamTrack& track) {
  Reload();
}

void WebMediaPlayerMS::TrackRemoved(const WebMediaStreamTrack& track) {
  Reload();
}

void WebMediaPlayerMS::ActiveStateChanged(bool is_active) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // The case when the stream becomes active is handled by TrackAdded().
  if (is_active)
    return;

  // This makes the media element elegible to be garbage collected. Otherwise,
  // the element will be considered active and will never be garbage
  // collected.
  SetNetworkState(kNetworkStateIdle);

  // Stop the audio renderer to free up resources that are not required for an
  // inactive stream. This is useful if the media element is not garbage
  // collected.
  // Note that the video renderer should not be stopped because the ended video
  // track is expected to produce a black frame after becoming inactive.
  if (audio_renderer_)
    audio_renderer_->Stop();
}

int WebMediaPlayerMS::GetDelegateId() {
  return delegate_id_;
}

base::Optional<viz::SurfaceId> WebMediaPlayerMS::GetSurfaceId() {
  if (bridge_)
    return bridge_->GetSurfaceId();
  return base::nullopt;
}

base::WeakPtr<WebMediaPlayer> WebMediaPlayerMS::AsWeakPtr() {
  return weak_this_;
}

void WebMediaPlayerMS::Reload() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (web_stream_.IsNull())
    return;

  ReloadVideo();
  ReloadAudio();
}

void WebMediaPlayerMS::ReloadVideo() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!web_stream_.IsNull());
  WebVector<WebMediaStreamTrack> video_tracks = web_stream_.VideoTracks();

  RendererReloadAction renderer_action = RendererReloadAction::KEEP_RENDERER;
  if (video_tracks.empty()) {
    if (video_frame_provider_)
      renderer_action = RendererReloadAction::REMOVE_RENDERER;
    current_video_track_id_ = WebString();
  } else if (video_tracks[0].Id() != current_video_track_id_ &&
             IsPlayableTrack(video_tracks[0])) {
    renderer_action = RendererReloadAction::NEW_RENDERER;
    current_video_track_id_ = video_tracks[0].Id();
  }

  switch (renderer_action) {
    case RendererReloadAction::NEW_RENDERER:
      if (video_frame_provider_)
        video_frame_provider_->Stop();

      SetNetworkState(kNetworkStateLoading);
      video_frame_provider_ = renderer_factory_->GetVideoRenderer(
          web_stream_,
          ConvertToBaseCallback(frame_deliverer_->GetRepaintCallback()),
          io_task_runner_, main_render_task_runner_);
      DCHECK(video_frame_provider_);
      video_frame_provider_->Start();
      break;

    case RendererReloadAction::REMOVE_RENDERER:
      video_frame_provider_->Stop();
      video_frame_provider_ = nullptr;
      break;

    default:
      return;
  }

  DCHECK_NE(renderer_action, RendererReloadAction::KEEP_RENDERER);
  if (!paused_) {
    // TODO(crbug.com/964494): Remove this explicit conversion.
    WebSize natural_size = NaturalSize();
    gfx::Size gfx_size(natural_size.height, natural_size.width);
    delegate_->DidPlayerSizeChange(delegate_id_, gfx_size);
  }
}

void WebMediaPlayerMS::ReloadAudio() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!web_stream_.IsNull());
  if (!internal_frame_->web_frame())
    return;

  WebVector<WebMediaStreamTrack> audio_tracks = web_stream_.AudioTracks();

  RendererReloadAction renderer_action = RendererReloadAction::KEEP_RENDERER;
  if (audio_tracks.empty()) {
    if (audio_renderer_)
      renderer_action = RendererReloadAction::REMOVE_RENDERER;
    current_audio_track_id_ = WebString();
  } else if (audio_tracks[0].Id() != current_audio_track_id_ &&
             IsPlayableTrack(audio_tracks[0])) {
    renderer_action = RendererReloadAction::NEW_RENDERER;
    current_audio_track_id_ = audio_tracks[0].Id();
  }

  switch (renderer_action) {
    case RendererReloadAction::NEW_RENDERER:
      if (audio_renderer_)
        audio_renderer_->Stop();

      SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
      audio_renderer_ = renderer_factory_->GetAudioRenderer(
          web_stream_, internal_frame_->web_frame(),
          initial_audio_output_device_id_);

      // |audio_renderer_| can be null in tests.
      if (!audio_renderer_)
        break;

      audio_renderer_->SetVolume(volume_);
      audio_renderer_->Start();
      audio_renderer_->Play();
      break;

    case RendererReloadAction::REMOVE_RENDERER:
      audio_renderer_->Stop();
      audio_renderer_ = nullptr;
      break;

    default:
      break;
  }
}

void WebMediaPlayerMS::Play() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PLAY));
  if (!paused_)
    return;

  if (video_frame_provider_)
    video_frame_provider_->Resume();

  compositor_->StartRendering();

  if (audio_renderer_)
    audio_renderer_->Play();

  if (HasVideo()) {
    // TODO(crbug.com/964494): Remove this explicit conversion.
    WebSize natural_size = NaturalSize();
    gfx::Size gfx_size(natural_size.height, natural_size.width);
    delegate_->DidPlayerSizeChange(delegate_id_, gfx_size);
  }

  // |delegate_| expects the notification only if there is at least one track
  // actually playing. A media stream might have none since tracks can be
  // removed from the stream.
  if (HasAudio() || HasVideo()) {
    // TODO(perkj, magjed): We use OneShot focus type here so that it takes
    // audio focus once it starts, and then will not respond to further audio
    // focus changes. See https://crbug.com/596516 for more details.
    delegate_->DidPlay(delegate_id_, HasVideo(), HasAudio(),
                       media::MediaContentType::OneShot);
  }

  delegate_->SetIdle(delegate_id_, false);
  paused_ = false;
}

void WebMediaPlayerMS::Pause() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  should_play_upon_shown_ = false;
  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PAUSE));
  if (paused_)
    return;

  if (video_frame_provider_)
    video_frame_provider_->Pause();

  compositor_->StopRendering();
  compositor_->ReplaceCurrentFrameWithACopy();

  if (audio_renderer_)
    audio_renderer_->Pause();

  delegate_->DidPause(delegate_id_);
  delegate_->SetIdle(delegate_id_, true);

  paused_ = true;
}

void WebMediaPlayerMS::Seek(double seconds) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void WebMediaPlayerMS::SetRate(double rate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void WebMediaPlayerMS::SetVolume(double volume) {
  DVLOG(1) << __func__ << "(volume=" << volume << ")";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  volume_ = volume;
  if (audio_renderer_.get())
    audio_renderer_->SetVolume(volume_ * volume_multiplier_);
  delegate_->DidPlayerMutedStatusChange(delegate_id_, volume == 0.0);
}

void WebMediaPlayerMS::SetLatencyHint(double seconds) {
  // WebRTC latency has separate latency APIs, focused more on network jitter
  // and implemented inside the WebRTC stack.
  // https://webrtc.org/experiments/rtp-hdrext/playout-delay/
  // https://henbos.github.io/webrtc-timing/#dom-rtcrtpreceiver-playoutdelayhint
}

void WebMediaPlayerMS::OnRequestPictureInPicture() {
  if (!bridge_)
    ActivateSurfaceLayerForVideo();

  DCHECK(bridge_);
  DCHECK(bridge_->GetSurfaceId().is_valid());
}

void WebMediaPlayerMS::SetSinkId(
    const WebString& sink_id,
    WebSetSinkIdCompleteCallback completion_callback) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  media::OutputDeviceStatusCB callback =
      ConvertToOutputDeviceStatusCB(std::move(completion_callback));
  if (audio_renderer_) {
    audio_renderer_->SwitchOutputDevice(sink_id.Utf8(), std::move(callback));
  } else {
    std::move(callback).Run(media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
  }
}

void WebMediaPlayerMS::SetPreload(WebMediaPlayer::Preload preload) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool WebMediaPlayerMS::HasVideo() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !!video_frame_provider_;
}

bool WebMediaPlayerMS::HasAudio() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !!audio_renderer_;
}

WebSize WebMediaPlayerMS::NaturalSize() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!video_frame_provider_)
    return WebSize();

  if (video_transformation_.rotation == media::VIDEO_ROTATION_90 ||
      video_transformation_.rotation == media::VIDEO_ROTATION_270) {
    const gfx::Size& current_size = compositor_->GetCurrentSize();
    return WebSize(current_size.height(), current_size.width());
  }
  return WebSize(compositor_->GetCurrentSize());
}

WebSize WebMediaPlayerMS::VisibleRect() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scoped_refptr<media::VideoFrame> video_frame = compositor_->GetCurrentFrame();
  if (!video_frame)
    return WebSize();

  const gfx::Rect& visible_rect = video_frame->visible_rect();
  if (video_transformation_.rotation == media::VIDEO_ROTATION_90 ||
      video_transformation_.rotation == media::VIDEO_ROTATION_270) {
    return WebSize(visible_rect.height(), visible_rect.width());
  }
  return WebSize(visible_rect.width(), visible_rect.height());
}

bool WebMediaPlayerMS::Paused() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return paused_;
}

bool WebMediaPlayerMS::Seeking() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return false;
}

double WebMediaPlayerMS::Duration() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return std::numeric_limits<double>::infinity();
}

double WebMediaPlayerMS::CurrentTime() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const base::TimeDelta current_time = compositor_->GetCurrentTime();
  if (current_time.ToInternalValue() != 0)
    return current_time.InSecondsF();
  else if (audio_renderer_.get())
    return audio_renderer_->GetCurrentRenderTime().InSecondsF();
  return 0.0;
}

WebMediaPlayer::NetworkState WebMediaPlayerMS::GetNetworkState() const {
  DVLOG(2) << __func__ << ", state:" << network_state_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerMS::GetReadyState() const {
  DVLOG(1) << __func__ << ", state:" << ready_state_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return ready_state_;
}

WebMediaPlayer::SurfaceLayerMode WebMediaPlayerMS::GetVideoSurfaceLayerMode()
    const {
  return surface_layer_mode_;
}

WebString WebMediaPlayerMS::GetErrorMessage() const {
  return WebString::FromUTF8(media_log_->GetErrorMessage());
}

WebTimeRanges WebMediaPlayerMS::Buffered() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return buffered_;
}

WebTimeRanges WebMediaPlayerMS::Seekable() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return WebTimeRanges();
}

bool WebMediaPlayerMS::DidLoadingProgress() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return true;
}

void WebMediaPlayerMS::Paint(cc::PaintCanvas* canvas,
                             const WebRect& rect,
                             cc::PaintFlags& flags,
                             int already_uploaded_id,
                             VideoFrameUploadMetadata* out_metadata) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const scoped_refptr<media::VideoFrame> frame = compositor_->GetCurrentFrame();

  viz::ContextProvider* provider = nullptr;
  if (frame && frame->HasTextures()) {
    provider = Platform::Current()->SharedMainThreadContextProvider();
    // GPU Process crashed.
    if (!provider)
      return;
  }
  const gfx::RectF dest_rect(rect.x, rect.y, rect.width, rect.height);
  video_renderer_.Paint(frame, canvas, dest_rect, flags, video_transformation_,
                        provider);
}

bool WebMediaPlayerMS::WouldTaintOrigin() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return false;
}

double WebMediaPlayerMS::MediaTimeForTimeValue(double timeValue) const {
  return base::TimeDelta::FromSecondsD(timeValue).InSecondsF();
}

unsigned WebMediaPlayerMS::DecodedFrameCount() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return static_cast<unsigned>(compositor_->total_frame_count());
}

unsigned WebMediaPlayerMS::DroppedFrameCount() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return static_cast<unsigned>(compositor_->dropped_frame_count());
}

uint64_t WebMediaPlayerMS::AudioDecodedByteCount() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return 0;
}

uint64_t WebMediaPlayerMS::VideoDecodedByteCount() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return 0;
}

bool WebMediaPlayerMS::HasAvailableVideoFrame() const {
  return has_first_frame_;
}

void WebMediaPlayerMS::OnFrameHidden() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // This method is called when the RenderFrame is sent to background or
  // suspended. During undoable tab closures OnHidden() may be called back to
  // back, so we can't rely on |render_frame_suspended_| being false here.
  if (frame_deliverer_) {
    PostCrossThreadTask(
        *io_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&FrameDeliverer::SetRenderFrameSuspended,
                            CrossThreadUnretained(frame_deliverer_.get()),
                            true));
  }

  PostCrossThreadTask(
      *compositor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebMediaPlayerMSCompositor::SetIsPageVisible,
                          CrossThreadUnretained(compositor_.get()), false));

// On Android, substitute the displayed VideoFrame with a copy to avoid holding
// onto it unnecessarily.
#if defined(OS_ANDROID)
  if (!paused_)
    compositor_->ReplaceCurrentFrameWithACopy();
#endif  // defined(OS_ANDROID)
}

void WebMediaPlayerMS::OnFrameClosed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

// On Android, pause the video completely for this time period.
#if defined(OS_ANDROID)
  if (!paused_) {
    Pause();
    should_play_upon_shown_ = true;
  }

  delegate_->PlayerGone(delegate_id_);
#endif  // defined(OS_ANDROID)

  if (frame_deliverer_) {
    PostCrossThreadTask(
        *io_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&FrameDeliverer::SetRenderFrameSuspended,
                            CrossThreadUnretained(frame_deliverer_.get()),
                            true));
  }
}

void WebMediaPlayerMS::OnFrameShown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (frame_deliverer_) {
    PostCrossThreadTask(
        *io_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&FrameDeliverer::SetRenderFrameSuspended,
                            CrossThreadUnretained(frame_deliverer_.get()),
                            false));
  }

  PostCrossThreadTask(
      *compositor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebMediaPlayerMSCompositor::SetIsPageVisible,
                          CrossThreadUnretained(compositor_.get()), true));

// On Android, resume playback on visibility. play() clears
// |should_play_upon_shown_|.
#if defined(OS_ANDROID)
  if (should_play_upon_shown_)
    Play();
#endif  // defined(OS_ANDROID)
}

void WebMediaPlayerMS::OnIdleTimeout() {}

void WebMediaPlayerMS::OnPlay() {
  // TODO(perkj, magjed): It's not clear how WebRTC should work with an
  // MediaSession, until these issues are resolved, disable session controls.
  // https://crbug.com/595297.
}

void WebMediaPlayerMS::OnPause() {
  // TODO(perkj, magjed): See TODO in OnPlay().
}

void WebMediaPlayerMS::OnMuted(bool muted) {
  client_->RequestMuted(muted);
}

void WebMediaPlayerMS::OnSeekForward(double seconds) {
  // TODO(perkj, magjed): See TODO in OnPlay().
}

void WebMediaPlayerMS::OnSeekBackward(double seconds) {
  // TODO(perkj, magjed): See TODO in OnPlay().
}

void WebMediaPlayerMS::OnVolumeMultiplierUpdate(double multiplier) {
  // TODO(perkj, magjed): See TODO in OnPlay().
}

void WebMediaPlayerMS::OnBecamePersistentVideo(bool value) {
  get_client()->OnBecamePersistentVideo(value);
}

bool WebMediaPlayerMS::CopyVideoTextureToPlatformTexture(
    gpu::gles2::GLES2Interface* gl,
    unsigned target,
    unsigned int texture,
    unsigned internal_format,
    unsigned format,
    unsigned type,
    int level,
    bool premultiply_alpha,
    bool flip_y,
    int already_uploaded_id,
    VideoFrameUploadMetadata* out_metadata) {
  TRACE_EVENT0("media", "copyVideoTextureToPlatformTexture");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  scoped_refptr<media::VideoFrame> video_frame = compositor_->GetCurrentFrame();

  if (!video_frame.get() || !video_frame->HasTextures())
    return false;

  auto* provider = Platform::Current()->SharedMainThreadContextProvider();
  // GPU Process crashed.
  if (!provider)
    return false;

  return video_renderer_.CopyVideoFrameTexturesToGLTexture(
      provider, gl, video_frame.get(), target, texture, internal_format, format,
      type, level, premultiply_alpha, flip_y);
}

bool WebMediaPlayerMS::CopyVideoYUVDataToPlatformTexture(
    gpu::gles2::GLES2Interface* gl,
    unsigned target,
    unsigned int texture,
    unsigned internal_format,
    unsigned format,
    unsigned type,
    int level,
    bool premultiply_alpha,
    bool flip_y,
    int already_uploaded_id,
    VideoFrameUploadMetadata* out_metadata) {
  TRACE_EVENT0("media", "copyVideoYUVDataToPlatformTexture");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  scoped_refptr<media::VideoFrame> video_frame = compositor_->GetCurrentFrame();

  if (!video_frame)
    return false;
  if (video_frame->HasTextures())
    return false;

  auto* provider = Platform::Current()->SharedMainThreadContextProvider();
  // GPU Process crashed.
  if (!provider)
    return false;

  return video_renderer_.CopyVideoFrameYUVDataToGLTexture(
      provider, gl, *video_frame, target, texture, internal_format, format,
      type, level, premultiply_alpha, flip_y);
}

bool WebMediaPlayerMS::TexImageImpl(TexImageFunctionID functionID,
                                    unsigned target,
                                    gpu::gles2::GLES2Interface* gl,
                                    unsigned int texture,
                                    int level,
                                    int internalformat,
                                    unsigned format,
                                    unsigned type,
                                    int xoffset,
                                    int yoffset,
                                    int zoffset,
                                    bool flip_y,
                                    bool premultiply_alpha) {
  TRACE_EVENT0("media", "texImageImpl");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const scoped_refptr<media::VideoFrame> video_frame =
      compositor_->GetCurrentFrame();
  if (!video_frame || !video_frame->IsMappable() ||
      video_frame->HasTextures() ||
      video_frame->format() != media::PIXEL_FORMAT_Y16) {
    return false;
  }

  if (functionID == kTexImage2D) {
    auto* provider = Platform::Current()->SharedMainThreadContextProvider();
    // GPU Process crashed.
    if (!provider)
      return false;
    return media::PaintCanvasVideoRenderer::TexImage2D(
        target, texture, gl, provider->ContextCapabilities(), video_frame.get(),
        level, internalformat, format, type, flip_y, premultiply_alpha);
  } else if (functionID == kTexSubImage2D) {
    return media::PaintCanvasVideoRenderer::TexSubImage2D(
        target, gl, video_frame.get(), level, format, type, xoffset, yoffset,
        flip_y, premultiply_alpha);
  }
  return false;
}

void WebMediaPlayerMS::ActivateSurfaceLayerForVideo() {
  // Note that we might or might not already be in VideoLayer mode.
  DCHECK(!bridge_);

  // If we're in VideoLayer mode, then get rid of the layer.
  if (video_layer_) {
    client_->SetCcLayer(nullptr);
    video_layer_ = nullptr;
  }

  bridge_ = std::move(create_bridge_callback_)
                .Run(this, compositor_->GetUpdateSubmissionStateCallback());
  bridge_->CreateSurfaceLayer();
  bridge_->SetContentsOpaque(opaque_);

  PostCrossThreadTask(
      *compositor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebMediaPlayerMSCompositor::EnableSubmission,
                          compositor_, bridge_->GetSurfaceId(),
                          bridge_->GetLocalSurfaceIdAllocationTime(),
                          video_transformation_, IsInPictureInPicture()));

  // If the element is already in Picture-in-Picture mode, it means that it
  // was set in this mode prior to this load, with a different
  // WebMediaPlayerImpl. The new player needs to send its id, size and
  // surface id to the browser process to make sure the states are properly
  // updated.
  // TODO(872056): the surface should be activated but for some reason, it
  // does not. It is possible that this will no longer be needed after 872056
  // is fixed.
  if (client_->DisplayType() ==
      WebMediaPlayer::DisplayType::kPictureInPicture) {
    OnSurfaceIdUpdated(bridge_->GetSurfaceId());
  }
}

void WebMediaPlayerMS::OnFirstFrameReceived(media::VideoRotation video_rotation,
                                            bool is_opaque) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  has_first_frame_ = true;
  OnRotationChanged(video_rotation);
  OnOpacityChanged(is_opaque);

  if (surface_layer_mode_ == WebMediaPlayer::SurfaceLayerMode::kAlways ||
      (surface_layer_mode_ == WebMediaPlayer::SurfaceLayerMode::kOnDemand &&
       client_->DisplayType() ==
           WebMediaPlayer::DisplayType::kPictureInPicture)) {
    ActivateSurfaceLayerForVideo();
  }

  SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);
  SetReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);
  TriggerResize();
  ResetCanvasCache();
}

void WebMediaPlayerMS::OnOpacityChanged(bool is_opaque) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  opaque_ = is_opaque;
  if (!bridge_) {
    // Opacity can be changed during the session without resetting
    // |video_layer_|.
    video_layer_->SetContentsOpaque(opaque_);
  } else {
    DCHECK(bridge_);
    bridge_->SetContentsOpaque(opaque_);
  }
}

void WebMediaPlayerMS::OnRotationChanged(media::VideoRotation video_rotation) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  video_transformation_ = {video_rotation, 0};

  if (!bridge_) {
    // Keep the old |video_layer_| alive until SetCcLayer() is called with a new
    // pointer, as it may use the pointer from the last call.
    auto new_video_layer =
        cc::VideoLayer::Create(compositor_.get(), video_rotation);
    get_client()->SetCcLayer(new_video_layer.get());
    video_layer_ = std::move(new_video_layer);
  }
}

bool WebMediaPlayerMS::IsInPictureInPicture() const {
  DCHECK(client_);
  return (!client_->IsInAutoPIP() &&
          client_->DisplayType() ==
              WebMediaPlayer::DisplayType::kPictureInPicture);
}

void WebMediaPlayerMS::RepaintInternal() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  get_client()->Repaint();
}

void WebMediaPlayerMS::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  get_client()->NetworkStateChanged();
}

void WebMediaPlayerMS::SetReadyState(WebMediaPlayer::ReadyState state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ready_state_ = state;
  // Always notify to ensure client has the latest value.
  get_client()->ReadyStateChanged();
}

media::PaintCanvasVideoRenderer*
WebMediaPlayerMS::GetPaintCanvasVideoRenderer() {
  return &video_renderer_;
}

void WebMediaPlayerMS::ResetCanvasCache() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  video_renderer_.ResetCache();
}

void WebMediaPlayerMS::TriggerResize() {
  if (HasVideo())
    get_client()->SizeChanged();

  // TODO(crbug.com/964494): Remove this explicit conversion.
  WebSize natural_size = NaturalSize();
  gfx::Size gfx_size(natural_size.height, natural_size.width);
  delegate_->DidPlayerSizeChange(delegate_id_, gfx_size);
}

void WebMediaPlayerMS::SetGpuMemoryBufferVideoForTesting(
    media::GpuMemoryBufferVideoFramePool* gpu_memory_buffer_pool) {
  CHECK(frame_deliverer_);
  frame_deliverer_->gpu_memory_buffer_pool_.reset(gpu_memory_buffer_pool);
}

void WebMediaPlayerMS::OnDisplayTypeChanged(
    WebMediaPlayer::DisplayType display_type) {
  if (!bridge_)
    return;

  PostCrossThreadTask(
      *compositor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &WebMediaPlayerMSCompositor::SetForceSubmit,
          CrossThreadUnretained(compositor_.get()),
          display_type == WebMediaPlayer::DisplayType::kPictureInPicture));
}

}  // namespace blink
