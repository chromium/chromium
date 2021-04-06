// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_impl.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/layers/video_layer.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/encryption_scheme.h"
#include "media/base/limits.h"
#include "media/base/media_content_type.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_url_demuxer.h"
#include "media/base/memory_dump_provider_proxy.h"
#include "media/base/text_renderer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/blink/texttrack_impl.h"
#include "media/blink/url_index.h"
#include "media/blink/video_decode_stats_reporter.h"
#include "media/blink/webcontentdecryptionmodule_impl.h"
#include "media/blink/webinbandtexttrack_impl.h"
#include "media/blink/webmediasource_impl.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/memory_data_source.h"
#include "media/learning/mojo/public/cpp/mojo_learning_task_controller.h"
#include "media/media_buildflags.h"
#include "media/remoting/remoting_constants.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/data_url.h"
#include "third_party/blink/public/common/media/watch_time_reporter.h"
#include "third_party/blink/public/platform/web_encrypted_media_types.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_encrypted_media_client.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_media_source.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_surface_layer_bridge.h"
#include "third_party/blink/public/platform/web_texttrack_metadata.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/webaudiosourceprovider_impl.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/public/web/modules/media/webmediaplayer_util.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

using blink::WebMediaPlayer;
using blink::WebString;
using gpu::gles2::GLES2Interface;

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

namespace media {

namespace {

const char kWatchTimeHistogram[] = "Media.WebMediaPlayerImpl.WatchTime";

void RecordSimpleWatchTimeUMA(RendererFactoryType type) {
  UMA_HISTOGRAM_ENUMERATION(kWatchTimeHistogram, type);
}

void SetSinkIdOnMediaThread(
    scoped_refptr<blink::WebAudioSourceProviderImpl> sink,
    const std::string& device_id,
    OutputDeviceStatusCB callback) {
  sink->SwitchOutputDevice(device_id, std::move(callback));
}

bool IsBackgroundSuspendEnabled(const WebMediaPlayerImpl* wmpi) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableBackgroundMediaSuspend)) {
    return false;
  }
  return wmpi->IsBackgroundMediaSuspendEnabled();
}

bool IsResumeBackgroundVideosEnabled() {
  return base::FeatureList::IsEnabled(kResumeBackgroundVideo);
}

bool IsBackgroundVideoPauseOptimizationEnabled() {
  return base::FeatureList::IsEnabled(kBackgroundVideoPauseOptimization);
}

bool IsNetworkStateError(blink::WebMediaPlayer::NetworkState state) {
  bool result = state == blink::WebMediaPlayer::kNetworkStateFormatError ||
                state == blink::WebMediaPlayer::kNetworkStateNetworkError ||
                state == blink::WebMediaPlayer::kNetworkStateDecodeError;
  DCHECK_EQ(state > blink::WebMediaPlayer::kNetworkStateLoaded, result);
  return result;
}

gfx::Size GetRotatedVideoSize(VideoRotation rotation, gfx::Size natural_size) {
  if (rotation == VIDEO_ROTATION_90 || rotation == VIDEO_ROTATION_270)
    return gfx::Size(natural_size.height(), natural_size.width());
  return natural_size;
}

void RecordEncryptedEvent(bool encrypted_event_fired) {
  UMA_HISTOGRAM_BOOLEAN("Media.EME.EncryptedEvent", encrypted_event_fired);
}

// How much time must have elapsed since loading last progressed before we
// assume that the decoder will have had time to complete preroll.
constexpr base::TimeDelta kPrerollAttemptTimeout =
    base::TimeDelta::FromSeconds(3);

// Maximum number, per-WMPI, of media logs of playback rate changes.
constexpr int kMaxNumPlaybackRateLogs = 10;

int GetSwitchToLocalMessage(MediaObserverClient::ReasonToSwitchToLocal reason) {
  switch (reason) {
    case MediaObserverClient::ReasonToSwitchToLocal::NORMAL:
      return IDS_MEDIA_REMOTING_STOP_TEXT;
    case MediaObserverClient::ReasonToSwitchToLocal::POOR_PLAYBACK_QUALITY:
      return IDS_MEDIA_REMOTING_STOP_BY_PLAYBACK_QUALITY_TEXT;
    case MediaObserverClient::ReasonToSwitchToLocal::PIPELINE_ERROR:
      return IDS_MEDIA_REMOTING_STOP_BY_ERROR_TEXT;
    case MediaObserverClient::ReasonToSwitchToLocal::ROUTE_TERMINATED:
      return blink::WebMediaPlayerClient::kMediaRemotingStopNoText;
  }
  NOTREACHED();
  // To suppress compiler warning on Windows.
  return blink::WebMediaPlayerClient::kMediaRemotingStopNoText;
}

// These values are persisted to UMA. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(crbug.com/825041): This should use EncryptionScheme when kUnencrypted
// removed.
enum class EncryptionSchemeUMA { kCenc = 0, kCbcs = 1, kCount };

EncryptionSchemeUMA DetermineEncryptionSchemeUMAValue(
    EncryptionScheme encryption_scheme) {
  if (encryption_scheme == EncryptionScheme::kCbcs)
    return EncryptionSchemeUMA::kCbcs;

  DCHECK_EQ(encryption_scheme, EncryptionScheme::kCenc);
  return EncryptionSchemeUMA::kCenc;
}

#if BUILDFLAG(ENABLE_FFMPEG)
// Returns true if |url| represents (or is likely to) a local file.
bool IsLocalFile(const GURL& url) {
  return url.SchemeIsFile() || url.SchemeIsFileSystem() ||
         url.SchemeIs(url::kContentScheme) ||
         url.SchemeIs(url::kContentIDScheme) ||
         url.SchemeIs("chrome-extension");
}
#endif

// Handles destruction of media::Renderer dependent components after the
// renderer has been destructed on the media thread.
void DestructionHelper(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> vfc_task_runner,
    std::unique_ptr<Demuxer> demuxer,
    std::unique_ptr<DataSource> data_source,
    std::unique_ptr<VideoFrameCompositor> compositor,
    std::unique_ptr<CdmContextRef> cdm_context_1,
    std::unique_ptr<CdmContextRef> cdm_context_2,
    std::unique_ptr<MediaLog> media_log,
    std::unique_ptr<RendererFactorySelector> renderer_factory_selector,
    std::unique_ptr<blink::WebSurfaceLayerBridge> bridge,
    bool is_chunk_demuxer) {
  // We release |bridge| after pipeline stop to ensure layout tests receive
  // painted video frames before test harness exit.
  main_task_runner->DeleteSoon(FROM_HERE, std::move(bridge));

  // Since the media::Renderer is gone we can now destroy the compositor and
  // renderer factory selector.
  vfc_task_runner->DeleteSoon(FROM_HERE, std::move(compositor));
  main_task_runner->DeleteSoon(FROM_HERE, std::move(renderer_factory_selector));

  // ChunkDemuxer can be deleted on any thread, but other demuxers are bound to
  // the main thread and must be deleted there now that the renderer is gone.
  if (!is_chunk_demuxer) {
    main_task_runner->DeleteSoon(FROM_HERE, std::move(demuxer));
    main_task_runner->DeleteSoon(FROM_HERE, std::move(data_source));
    main_task_runner->DeleteSoon(FROM_HERE, std::move(cdm_context_1));
    main_task_runner->DeleteSoon(FROM_HERE, std::move(cdm_context_2));
    main_task_runner->DeleteSoon(FROM_HERE, std::move(media_log));
    return;
  }

  // ChunkDemuxer's streams may contain much buffered, compressed media that
  // may need to be paged back in during destruction.  Paging delay may exceed
  // the renderer hang monitor's threshold on at least Windows while also
  // blocking other work on the renderer main thread, so we do the actual
  // destruction in the background without blocking WMPI destruction or
  // |task_runner|.  On advice of task_scheduler OWNERS, MayBlock() is not
  // used because virtual memory overhead is not considered blocking I/O; and
  // CONTINUE_ON_SHUTDOWN is used to allow process termination to not block on
  // completing the task.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
             std::unique_ptr<Demuxer> demuxer_to_destroy,
             std::unique_ptr<CdmContextRef> cdm_context_1,
             std::unique_ptr<CdmContextRef> cdm_context_2,
             std::unique_ptr<MediaLog> media_log) {
            SCOPED_UMA_HISTOGRAM_TIMER("Media.MSE.DemuxerDestructionTime");
            demuxer_to_destroy.reset();
            main_task_runner->DeleteSoon(FROM_HERE, std::move(cdm_context_1));
            main_task_runner->DeleteSoon(FROM_HERE, std::move(cdm_context_2));
            main_task_runner->DeleteSoon(FROM_HERE, std::move(media_log));
          },
          std::move(main_task_runner), std::move(demuxer),
          std::move(cdm_context_1), std::move(cdm_context_2),
          std::move(media_log)));
}

std::string SanitizeUserStringProperty(blink::WebString value) {
  std::string converted = value.Utf8();
  return base::IsStringUTF8(converted) ? converted : "[invalid property]";
}

void CreateAllocation(base::trace_event::ProcessMemoryDump* pmd,
                      int32_t id,
                      const char* name,
                      int64_t bytes) {
  if (bytes <= 0)
    return;
  auto full_name =
      base::StringPrintf("media/webmediaplayer/%s/player_0x%x", name, id);
  auto* dump = pmd->CreateAllocatorDump(full_name);

  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes, bytes);

  auto* std_allocator = base::trace_event::MemoryDumpManager::GetInstance()
                            ->system_allocator_pool_name();
  if (std_allocator)
    pmd->AddSuballocation(dump->guid(), std_allocator);
}

// Determine whether we should update MediaPosition in |delegate_|.
bool MediaPositionNeedsUpdate(
    const media_session::MediaPosition& old_position,
    const media_session::MediaPosition& new_position) {
  if (old_position.playback_rate() != new_position.playback_rate())
    return true;

  if (old_position.duration() != new_position.duration())
    return true;

  // Special handling for "infinite" position required to avoid calculations
  // involving infinities.
  if (new_position.GetPosition().is_max())
    return !old_position.GetPosition().is_max();

  // MediaPosition is potentially changed upon each OnTimeUpdate() call. In
  // practice most of these calls happen periodically during normal playback,
  // with unchanged rate and duration. If we want to avoid updating
  // MediaPosition unnecessarily, we need to compare the current time
  // calculated from the old and new MediaPositions with some tolerance. That's
  // because we don't know the exact time when GetMediaTime() calculated the
  // media position. We choose an arbitrary tolerance that is high enough to
  // eliminate a lot of MediaPosition updates and low enough not to make a
  // perceptible difference.
  const auto drift =
      (old_position.GetPosition() - new_position.GetPosition()).magnitude();
  return drift > base::TimeDelta::FromMilliseconds(100);
}

}  // namespace

class BufferedDataSourceHostImpl;

STATIC_ASSERT_ENUM(WebMediaPlayer::kCorsModeUnspecified,
                   UrlData::CORS_UNSPECIFIED);
STATIC_ASSERT_ENUM(WebMediaPlayer::kCorsModeAnonymous, UrlData::CORS_ANONYMOUS);
STATIC_ASSERT_ENUM(WebMediaPlayer::kCorsModeUseCredentials,
                   UrlData::CORS_USE_CREDENTIALS);

WebMediaPlayerImpl::WebMediaPlayerImpl(
    blink::WebLocalFrame* frame,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    blink::WebMediaPlayerDelegate* delegate,
    std::unique_ptr<RendererFactorySelector> renderer_factory_selector,
    UrlIndex* url_index,
    std::unique_ptr<VideoFrameCompositor> compositor,
    std::unique_ptr<WebMediaPlayerParams> params)
    : frame_(frame),
      main_task_runner_(
          frame->GetTaskRunner(blink::TaskType::kMediaElementEvent)),
      media_task_runner_(params->media_task_runner()),
      worker_task_runner_(params->worker_task_runner()),
      media_log_(params->take_media_log()),
      client_(client),
      encrypted_client_(encrypted_client),
      delegate_(delegate),
      delegate_has_audio_(HasUnmutedAudio()),
      defer_load_cb_(params->defer_load_cb()),
      adjust_allocated_memory_cb_(params->adjust_allocated_memory_cb()),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      url_index_(url_index),
      raster_context_provider_(params->raster_context_provider()),
      vfc_task_runner_(params->video_frame_compositor_task_runner()),
      compositor_(std::move(compositor)),
      renderer_factory_selector_(std::move(renderer_factory_selector)),
      observer_(params->media_observer()),
      enable_instant_source_buffer_gc_(
          params->enable_instant_source_buffer_gc()),
      embedded_media_experience_enabled_(
          params->embedded_media_experience_enabled()),
      surface_layer_mode_(params->use_surface_layer_for_video()),
      create_bridge_callback_(params->create_bridge_callback()),
      request_routing_token_cb_(params->request_routing_token_cb()),
      overlay_routing_token_(OverlayInfo::RoutingToken()),
      media_metrics_provider_(params->take_metrics_provider()),
      is_background_suspend_enabled_(params->IsBackgroundSuspendEnabled()),
      is_background_video_playback_enabled_(
          params->IsBackgroundVideoPlaybackEnabled()),
      is_background_video_track_optimization_supported_(
          params->IsBackgroundVideoTrackOptimizationSupported()),
      simple_watch_timer_(
          base::BindRepeating(&WebMediaPlayerImpl::OnSimpleWatchTimerTick,
                              base::Unretained(this)),
          base::BindRepeating(&WebMediaPlayerImpl::GetCurrentTimeInternal,
                              base::Unretained(this))),
      will_play_helper_(nullptr),
      demuxer_override_(params->TakeDemuxerOverride()),
      power_status_helper_(params->TakePowerStatusHelper()) {
  DVLOG(1) << __func__;
  DCHECK(adjust_allocated_memory_cb_);
  DCHECK(renderer_factory_selector_);
  DCHECK(client_);
  DCHECK(delegate_);

  weak_this_ = weak_factory_.GetWeakPtr();

  // Using base::Unretained(this) is safe because the |pipeline| is owned by
  // |this| and the callback will always be made on the main task runner.
  // Not using BindToCurrentLoop() because CreateRenderer() is a sync call.
  auto pipeline = std::make_unique<PipelineImpl>(
      media_task_runner_, main_task_runner_,
      base::BindRepeating(&WebMediaPlayerImpl::CreateRenderer,
                          base::Unretained(this)),
      media_log_.get());

  pipeline_controller_ = std::make_unique<PipelineController>(
      std::move(pipeline),
      base::BindRepeating(&WebMediaPlayerImpl::OnPipelineSeeked, weak_this_),
      base::BindRepeating(&WebMediaPlayerImpl::OnPipelineSuspended, weak_this_),
      base::BindRepeating(&WebMediaPlayerImpl::OnBeforePipelineResume,
                          weak_this_),
      base::BindRepeating(&WebMediaPlayerImpl::OnPipelineResumed, weak_this_),
      base::BindRepeating(&WebMediaPlayerImpl::OnError, weak_this_));

  buffered_data_source_host_ = std::make_unique<BufferedDataSourceHostImpl>(
      base::BindRepeating(&WebMediaPlayerImpl::OnProgress, weak_this_),
      tick_clock_);

  // If we're supposed to force video overlays, then make sure that they're
  // enabled all the time.
  always_enable_overlays_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceVideoOverlays);

  if (base::FeatureList::IsEnabled(kOverlayFullscreenVideo))
    overlay_mode_ = OverlayMode::kUseAndroidOverlay;
  else
    overlay_mode_ = OverlayMode::kNoOverlays;

  delegate_id_ = delegate_->AddObserver(this);
  delegate_->SetIdle(delegate_id_, true);

  media_log_->AddEvent<MediaLogEvent::kWebMediaPlayerCreated>(
      url::Origin(frame_->GetSecurityOrigin()).GetURL().spec());

  media_log_->SetProperty<MediaLogProperty::kFrameUrl>(
      SanitizeUserStringProperty(frame_->GetDocument().Url().GetString()));
  media_log_->SetProperty<MediaLogProperty::kFrameTitle>(
      SanitizeUserStringProperty(frame_->GetDocument().Title()));

  if (params->initial_cdm())
    SetCdmInternal(params->initial_cdm());

  // Report a false "EncrytpedEvent" here as a baseline.
  RecordEncryptedEvent(false);

  auto on_audio_source_provider_set_client_callback = base::BindOnce(
      [](base::WeakPtr<WebMediaPlayerImpl> self,
         blink::WebMediaPlayerClient* const client) {
        if (!self)
          return;
        client->DidDisableAudioOutputSinkChanges();
      },
      weak_this_, client_);

  // TODO(xhwang): When we use an external Renderer, many methods won't work,
  // e.g. GetCurrentFrameFromCompositor(). See http://crbug.com/434861
  audio_source_provider_ = new blink::WebAudioSourceProviderImpl(
      params->audio_renderer_sink(), media_log_.get(),
      std::move(on_audio_source_provider_set_client_callback));

  if (observer_)
    observer_->SetClient(this);

  memory_usage_reporting_timer_.SetTaskRunner(
      frame_->GetTaskRunner(blink::TaskType::kInternalMedia));

  main_thread_mem_dumper_ = std::make_unique<MemoryDumpProviderProxy>(
      "WebMediaPlayer_MainThread", main_task_runner_,
      base::BindRepeating(&WebMediaPlayerImpl::OnMainThreadMemoryDump,
                          weak_this_, media_log_->id()));

  media_metrics_provider_->AcquirePlaybackEventsRecorder(
      playback_events_recorder_.BindNewPipeAndPassReceiver());

  // MediaMetricsProvider may drop the request for PlaybackEventsRecorder if
  // it's not interested in recording these events.
  playback_events_recorder_.reset_on_disconnect();

#if defined(OS_ANDROID)
  renderer_factory_selector_->SetRemotePlayStateChangeCB(
      BindToCurrentLoop(base::BindRepeating(
          &WebMediaPlayerImpl::OnRemotePlayStateChange, weak_this_)));
#endif  // defined (OS_ANDROID)
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (set_cdm_result_) {
    DVLOG(2)
        << "Resolve pending SetCdmInternal() when media player is destroyed.";
    set_cdm_result_->Complete();
    set_cdm_result_.reset();
  }

  suppress_destruction_errors_ = true;

  delegate_->PlayerGone(delegate_id_);
  delegate_->RemoveObserver(delegate_id_);
  delegate_ = nullptr;

  // Finalize any watch time metrics before destroying the pipeline.
  watch_time_reporter_.reset();

  // Unregister dump providers on their corresponding threads.
  media_task_runner_->DeleteSoon(FROM_HERE,
                                 std::move(media_thread_mem_dumper_));
  main_thread_mem_dumper_.reset();

  // The underlying Pipeline must be stopped before it is destroyed.
  //
  // Note: This destruction happens synchronously on the media thread and
  // |demuxer_|, |data_source_|, |compositor_|, and |media_log_| must outlive
  // this process. They will be destructed by the DestructionHelper below
  // after trampolining through the media thread.
  pipeline_controller_->Stop();

  if (last_reported_memory_usage_)
    adjust_allocated_memory_cb_.Run(-last_reported_memory_usage_);

  // Destruct compositor resources in the proper order.
  client_->SetCcLayer(nullptr);

  client_->MediaRemotingStopped(
      blink::WebMediaPlayerClient::kMediaRemotingStopNoText);

  if (!surface_layer_for_video_enabled_ && video_layer_)
    video_layer_->StopUsingProvider();

  simple_watch_timer_.Stop();
  media_log_->OnWebMediaPlayerDestroyed();

  if (data_source_)
    data_source_->Stop();

  // Disconnect from the surface layer. We still preserve the |bridge_| until
  // after pipeline shutdown to ensure any pending frames are painted for tests.
  if (bridge_)
    bridge_->ClearObserver();

  // Disconnect from the MediaObserver implementation since it's lifetime is
  // tied to the RendererFactorySelector which can't be destroyed until after
  // the Pipeline stops.
  //
  // Note: We can't use a WeakPtr with the RendererFactory because its methods
  // are called on the media thread and this destruction takes place on the
  // renderer thread.
  if (observer_)
    observer_->SetClient(nullptr);

  // If we're in the middle of an observation, then finish it.
  will_play_helper_.CompleteObservationIfNeeded(learning::TargetValue(false));

  // Handle destruction of things that need to be destructed after the pipeline
  // completes stopping on the media thread.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DestructionHelper, std::move(main_task_runner_),
                     std::move(vfc_task_runner_), std::move(demuxer_),
                     std::move(data_source_), std::move(compositor_),
                     std::move(cdm_context_ref_),
                     std::move(pending_cdm_context_ref_), std::move(media_log_),
                     std::move(renderer_factory_selector_), std::move(bridge_),
                     !!chunk_demuxer_));
}

WebMediaPlayer::LoadTiming WebMediaPlayerImpl::Load(
    LoadType load_type,
    const blink::WebMediaPlayerSource& source,
    CorsMode cors_mode,
    bool is_cache_disabled) {
  // Only URL or MSE blob URL is supported.
  DCHECK(source.IsURL());
  blink::WebURL url = source.GetAsURL();
  DVLOG(1) << __func__ << "(" << load_type << ", " << GURL(url) << ", "
           << cors_mode << ")";

  bool is_deferred = false;

  if (defer_load_cb_) {
    is_deferred = defer_load_cb_.Run(
        base::BindOnce(&WebMediaPlayerImpl::DoLoad, weak_this_, load_type, url,
                       cors_mode, is_cache_disabled));
  } else {
    DoLoad(load_type, url, cors_mode, is_cache_disabled);
  }

  return is_deferred ? LoadTiming::kDeferred : LoadTiming::kImmediate;
}

void WebMediaPlayerImpl::OnWebLayerUpdated() {}

void WebMediaPlayerImpl::RegisterContentsLayer(cc::Layer* layer) {
  DCHECK(bridge_);
  bridge_->SetContentsOpaque(opaque_);
  client_->SetCcLayer(layer);
}

void WebMediaPlayerImpl::UnregisterContentsLayer(cc::Layer* layer) {
  // |client_| will unregister its cc::Layer if given a nullptr.
  client_->SetCcLayer(nullptr);
}

void WebMediaPlayerImpl::OnSurfaceIdUpdated(viz::SurfaceId surface_id) {
  // TODO(726619): Handle the behavior when Picture-in-Picture mode is
  // disabled.
  // The viz::SurfaceId may be updated when the video begins playback or when
  // the size of the video changes.
  if (client_)
    client_->OnPictureInPictureStateChange();
}

void WebMediaPlayerImpl::EnableOverlay() {
  overlay_enabled_ = true;
  if (request_routing_token_cb_ &&
      overlay_mode_ == OverlayMode::kUseAndroidOverlay) {
    overlay_routing_token_is_pending_ = true;
    token_available_cb_.Reset(
        base::BindOnce(&WebMediaPlayerImpl::OnOverlayRoutingToken, weak_this_));
    request_routing_token_cb_.Run(token_available_cb_.callback());
  }

  // We have requested (and maybe already have) overlay information.  If the
  // restarted decoder requests overlay information, then we'll defer providing
  // it if it hasn't arrived yet.  Otherwise, this would be a race, since we
  // don't know if the request for overlay info or restart will complete first.
  if (decoder_requires_restart_for_overlay_)
    ScheduleRestart();
}

void WebMediaPlayerImpl::DisableOverlay() {
  overlay_enabled_ = false;
  if (overlay_mode_ == OverlayMode::kUseAndroidOverlay) {
    token_available_cb_.Cancel();
    overlay_routing_token_is_pending_ = false;
    overlay_routing_token_ = OverlayInfo::RoutingToken();
  }

  if (decoder_requires_restart_for_overlay_)
    ScheduleRestart();
  else
    MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::EnteredFullscreen() {
  overlay_info_.is_fullscreen = true;

  // |always_enable_overlays_| implies that we're already in overlay mode, so
  // take no action here.  Otherwise, switch to an overlay if it's allowed and
  // if it will display properly.
  if (!always_enable_overlays_ && overlay_mode_ != OverlayMode::kNoOverlays &&
      DoesOverlaySupportMetadata()) {
    EnableOverlay();
  }

  // We send this only if we can send multiple calls.  Otherwise, either (a)
  // we already sent it and we don't have a callback anyway (we reset it when
  // it's called in restart mode), or (b) we'll send this later when the surface
  // actually arrives.  GVD assumes that the first overlay info will have the
  // routing information.  Note that we set |is_fullscreen_| earlier, so that
  // if EnableOverlay() can include fullscreen info in case it sends the overlay
  // info before returning.
  if (!decoder_requires_restart_for_overlay_)
    MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::ExitedFullscreen() {
  overlay_info_.is_fullscreen = false;

  // If we're in overlay mode, then exit it unless we're supposed to allow
  // overlays all the time.
  if (!always_enable_overlays_ && overlay_enabled_)
    DisableOverlay();

  // See EnteredFullscreen for why we do this.
  if (!decoder_requires_restart_for_overlay_)
    MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::BecameDominantVisibleContent(bool is_dominant) {
  if (observer_)
    observer_->OnBecameDominantVisibleContent(is_dominant);
}

void WebMediaPlayerImpl::SetIsEffectivelyFullscreen(
    blink::WebFullscreenVideoStatus fullscreen_video_status) {
  if (power_status_helper_) {
    // We don't care about pip, so anything that's "not fullscreen" is good
    // enough for us.
    power_status_helper_->SetIsFullscreen(
        fullscreen_video_status !=
        blink::WebFullscreenVideoStatus::kNotEffectivelyFullscreen);
  }
}

void WebMediaPlayerImpl::OnHasNativeControlsChanged(bool has_native_controls) {
  if (!watch_time_reporter_)
    return;

  if (has_native_controls)
    watch_time_reporter_->OnNativeControlsEnabled();
  else
    watch_time_reporter_->OnNativeControlsDisabled();
}

void WebMediaPlayerImpl::OnDisplayTypeChanged(blink::DisplayType display_type) {
  if (surface_layer_for_video_enabled_) {
    vfc_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoFrameCompositor::SetForceSubmit,
                       base::Unretained(compositor_.get()),
                       display_type == blink::DisplayType::kPictureInPicture));
  }

  if (!watch_time_reporter_)
    return;

  switch (display_type) {
    case blink::DisplayType::kInline:
      watch_time_reporter_->OnDisplayTypeInline();
      break;
    case blink::DisplayType::kFullscreen:
      watch_time_reporter_->OnDisplayTypeFullscreen();
      break;
    case blink::DisplayType::kPictureInPicture:
      watch_time_reporter_->OnDisplayTypePictureInPicture();

      // Resumes playback if it was paused when hidden.
      if (paused_when_hidden_) {
        paused_when_hidden_ = false;
        client_->ResumePlayback();
      }
      break;
  }
}

void WebMediaPlayerImpl::DoLoad(LoadType load_type,
                                const blink::WebURL& url,
                                CorsMode cors_mode,
                                bool is_cache_disabled) {
  TRACE_EVENT1("media", "WebMediaPlayerImpl::DoLoad", "id", media_log_->id());
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Start a new observation.  If there was one before, then we didn't play it.
  will_play_helper_.CompleteObservationIfNeeded(learning::TargetValue(false));
  // For now, send in an empty set of features.  We should fill some in here,
  // and / or ask blink (via |client_|) for features from the DOM.
  learning::FeatureDictionary dict;
  will_play_helper_.BeginObservation(dict);

#if defined(OS_ANDROID)
  // Only allow credentials if the crossorigin attribute is unspecified
  // (kCorsModeUnspecified) or "use-credentials" (kCorsModeUseCredentials).
  // This value is only used by the MediaPlayerRenderer.
  // See https://crbug.com/936566.
  //
  // The credentials mode also has repercussions in WouldTaintOrigin(), but we
  // access what we need from |mb_data_source_|->cors_mode() directly, instead
  // of storing it here.
  allow_media_player_renderer_credentials_ = cors_mode != kCorsModeAnonymous;
#endif  // defined(OS_ANDROID)

  // Note: |url| may be very large, take care when making copies.
  loaded_url_ = GURL(url);
  load_type_ = load_type;

  ReportMetrics(load_type, loaded_url_, *frame_, media_log_.get());

  // Set subresource URL for crash reporting; will be truncated to 256 bytes.
  static base::debug::CrashKeyString* subresource_url =
      base::debug::AllocateCrashKeyString("subresource_url",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(subresource_url, loaded_url_.spec());

  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);

  // Do a truncation to kMaxUrlLength+1 at most; we can add ellipsis later.
  media_log_->AddEvent<MediaLogEvent::kLoad>(
      url.GetString().Substring(0, kMaxUrlLength + 1).Utf8());
  load_start_time_ = base::TimeTicks::Now();

  std::vector<TextTrackConfig> text_configs;
  for (const auto& metadata : client_->GetTextTrackMetadata()) {
    text_configs.emplace_back(TextTrackConfig::ConvertKind(metadata.kind()),
                              metadata.label(), metadata.language(),
                              metadata.id());
  }
  media_log_->SetProperty<MediaLogProperty::kTextTracks>(text_configs);

  // If we're adapting, then restart the smoothness experiment.
  if (smoothness_helper_)
    smoothness_helper_.reset();

  media_metrics_provider_->Initialize(
      load_type == kLoadTypeMediaSource,
      load_type == kLoadTypeURL ? blink::GetMediaURLScheme(loaded_url_)
                                : mojom::MediaURLScheme::kUnknown,
      mojom::MediaStreamType::kNone);

  if (demuxer_override_ || load_type == kLoadTypeMediaSource) {
    // If a demuxer override was specified or a Media Source pipeline will be
    // used, the pipeline can start immediately.
    StartPipeline();
  } else {
    // If |loaded_url_| is remoting media, starting the pipeline.
    if (loaded_url_.SchemeIs(remoting::kRemotingScheme)) {
      StartPipeline();
      return;
    }

    // Short circuit the more complex loading path for data:// URLs. Sending
    // them through the network based loading path just wastes memory and causes
    // worse performance since reads become asynchronous.
    if (loaded_url_.SchemeIs(url::kDataScheme)) {
      std::string mime_type, charset, data;
      if (!net::DataURL::Parse(loaded_url_, &mime_type, &charset, &data) ||
          data.empty()) {
        DataSourceInitialized(false);
        return;
      }

      // Replace |loaded_url_| with an empty data:// URL since it may be large.
      loaded_url_ = GURL("data:,");

      // Mark all the data as buffered.
      buffered_data_source_host_->SetTotalBytes(data.size());
      buffered_data_source_host_->AddBufferedByteRange(0, data.size());

      DCHECK(!mb_data_source_);
      data_source_ = std::make_unique<MemoryDataSource>(std::move(data));
      DataSourceInitialized(true);
      return;
    }

    auto url_data = url_index_->GetByUrl(
        url, static_cast<UrlData::CorsMode>(cors_mode),
        is_cache_disabled ? UrlIndex::kCacheDisabled : UrlIndex::kNormal);
    mb_data_source_ = new MultibufferDataSource(
        main_task_runner_, std::move(url_data), media_log_.get(),
        buffered_data_source_host_.get(),
        base::BindRepeating(&WebMediaPlayerImpl::NotifyDownloading,
                            weak_this_));
    data_source_.reset(mb_data_source_);
    mb_data_source_->OnRedirect(base::BindRepeating(
        &WebMediaPlayerImpl::OnDataSourceRedirected, weak_this_));
    mb_data_source_->SetPreload(preload_);
    mb_data_source_->SetIsClientAudioElement(client_->IsAudioElement());
    mb_data_source_->Initialize(
        base::BindOnce(&WebMediaPlayerImpl::DataSourceInitialized, weak_this_));
  }
}

void WebMediaPlayerImpl::Play() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // User initiated play unlocks background video playback.
  if (frame_->HasTransientUserActivation())
    video_locked_when_paused_when_hidden_ = false;

  // TODO(sandersd): Do we want to reset the idle timer here?
  delegate_->SetIdle(delegate_id_, false);
  paused_ = false;
  pipeline_controller_->SetPlaybackRate(playback_rate_);
  background_pause_timer_.Stop();

  if (observer_)
    observer_->OnPlaying();

  // Try to create the smoothness helper, in case we were paused before.
  UpdateSmoothnessHelper();

  if (playback_events_recorder_)
    playback_events_recorder_->OnPlaying();

  watch_time_reporter_->SetAutoplayInitiated(client_->WasAutoplayInitiated());

  // If we're seeking we'll trigger the watch time reporter upon seek completed;
  // we don't want to start it here since the seek time is unstable. E.g., when
  // playing content with a positive start time we would have a zero seek time.
  if (!Seeking()) {
    DCHECK(watch_time_reporter_);
    watch_time_reporter_->OnPlaying();
  }

  if (video_decode_stats_reporter_)
    video_decode_stats_reporter_->OnPlaying();

  simple_watch_timer_.Start();
  media_metrics_provider_->SetHasPlayed();
  media_log_->AddEvent<MediaLogEvent::kPlay>();

  MaybeUpdateBufferSizesForPlayback();
  UpdatePlayState();

  // Notify the learning task, if needed.
  will_play_helper_.CompleteObservationIfNeeded(learning::TargetValue(true));
}

void WebMediaPlayerImpl::Pause() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // We update the paused state even when casting, since we expect pause() to be
  // called when casting begins, and when we exit casting we should end up in a
  // paused state.
  paused_ = true;

  // No longer paused because it was hidden.
  paused_when_hidden_ = false;

  UpdateSmoothnessHelper();

  // User initiated pause locks background videos.
  if (frame_->HasTransientUserActivation())
    video_locked_when_paused_when_hidden_ = true;

  pipeline_controller_->SetPlaybackRate(0.0);

  // For states <= kReadyStateHaveMetadata, we may not have a renderer yet.
  if (highest_ready_state_ > WebMediaPlayer::kReadyStateHaveMetadata)
    paused_time_ = pipeline_controller_->GetMediaTime();

  if (observer_)
    observer_->OnPaused();

  if (playback_events_recorder_)
    playback_events_recorder_->OnPaused();

  DCHECK(watch_time_reporter_);
  watch_time_reporter_->OnPaused();

  if (video_decode_stats_reporter_)
    video_decode_stats_reporter_->OnPaused();

  simple_watch_timer_.Stop();
  media_log_->AddEvent<MediaLogEvent::kPause>();

  UpdatePlayState();
}

void WebMediaPlayerImpl::Seek(double seconds) {
  DVLOG(1) << __func__ << "(" << seconds << "s)";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent<MediaLogEvent::kSeek>(seconds);
  DoSeek(base::TimeDelta::FromSecondsD(seconds), true);
}

void WebMediaPlayerImpl::DoSeek(base::TimeDelta time, bool time_updated) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT2("media", "WebMediaPlayerImpl::DoSeek", "target",
               time.InSecondsF(), "id", media_log_->id());

  ReadyState old_state = ready_state_;
  if (ready_state_ > WebMediaPlayer::kReadyStateHaveMetadata)
    SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);

  // When paused or ended, we know exactly what the current time is and can
  // elide seeks to it. However, there are two cases that are not elided:
  //   1) When the pipeline state is not stable.
  //      In this case we just let |pipeline_controller_| decide what to do, as
  //      it has complete information.
  //   2) For MSE.
  //      Because the buffers may have changed between seeks, MSE seeks are
  //      never elided.
  if (paused_ && pipeline_controller_->IsStable() &&
      (paused_time_ == time ||
       (ended_ && time == base::TimeDelta::FromSecondsD(Duration()))) &&
      !chunk_demuxer_) {
    // If the ready state was high enough before, we can indicate that the seek
    // completed just by restoring it. Otherwise we will just wait for the real
    // ready state change to eventually happen.
    if (old_state == kReadyStateHaveEnoughData) {
      main_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&WebMediaPlayerImpl::OnBufferingStateChange,
                                    weak_this_, BUFFERING_HAVE_ENOUGH,
                                    BUFFERING_CHANGE_REASON_UNKNOWN));
    }
    return;
  }

  if (playback_events_recorder_)
    playback_events_recorder_->OnSeeking();

  // Call this before setting |seeking_| so that the current media time can be
  // recorded by the reporter.
  if (watch_time_reporter_)
    watch_time_reporter_->OnSeeking();

  // Send the seek updates only when the seek pipeline hasn't started,
  // OnPipelineSeeked is not called yet.
  if (!seeking_)
    client_->DidSeek();

  // TODO(sandersd): Move |seeking_| to PipelineController.
  // TODO(sandersd): Do we want to reset the idle timer here?
  delegate_->SetIdle(delegate_id_, false);
  ended_ = false;
  seeking_ = true;
  seek_time_ = time;
  if (paused_)
    paused_time_ = time;
  pipeline_controller_->Seek(time, time_updated);

  // This needs to be called after Seek() so that if a resume is triggered, it
  // is to the correct time.
  UpdatePlayState();
}

void WebMediaPlayerImpl::SetRate(double rate) {
  DVLOG(1) << __func__ << "(" << rate << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (rate != playback_rate_) {
    LIMITED_MEDIA_LOG(INFO, media_log_.get(), num_playback_rate_logs_,
                      kMaxNumPlaybackRateLogs)
        << "Effective playback rate changed from " << playback_rate_ << " to "
        << rate;
  }

  playback_rate_ = rate;
  if (!paused_)
    pipeline_controller_->SetPlaybackRate(rate);

  MaybeUpdateBufferSizesForPlayback();
}

void WebMediaPlayerImpl::SetVolume(double volume) {
  DVLOG(1) << __func__ << "(" << volume << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  volume_ = volume;
  pipeline_controller_->SetVolume(volume_ * volume_multiplier_);
  if (watch_time_reporter_)
    watch_time_reporter_->OnVolumeChange(volume);
  client_->DidPlayerMutedStatusChange(volume == 0.0);

  if (delegate_has_audio_ != HasUnmutedAudio()) {
    delegate_has_audio_ = HasUnmutedAudio();
    MediaContentType content_type =
        DurationToMediaContentType(GetPipelineMediaDuration());
    client_->DidMediaMetadataChange(delegate_has_audio_, HasVideo(),
                                    content_type);
    delegate_->DidMediaMetadataChange(delegate_id_, delegate_has_audio_,
                                      HasVideo(), content_type);
  }

  // The play state is updated because the player might have left the autoplay
  // muted state.
  UpdatePlayState();
}

void WebMediaPlayerImpl::SetLatencyHint(double seconds) {
  DVLOG(1) << __func__ << "(" << seconds << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::Optional<base::TimeDelta> latency_hint;
  if (std::isfinite(seconds)) {
    DCHECK_GE(seconds, 0);
    latency_hint = base::TimeDelta::FromSecondsD(seconds);
  }
  pipeline_controller_->SetLatencyHint(latency_hint);
}

void WebMediaPlayerImpl::SetPreservesPitch(bool preserves_pitch) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  pipeline_controller_->SetPreservesPitch(preserves_pitch);
}

void WebMediaPlayerImpl::SetAutoplayInitiated(bool autoplay_initiated) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  pipeline_controller_->SetAutoplayInitiated(autoplay_initiated);
}

void WebMediaPlayerImpl::OnRequestPictureInPicture() {
  if (!surface_layer_for_video_enabled_)
    ActivateSurfaceLayerForVideo();

  DCHECK(bridge_);
  DCHECK(bridge_->GetSurfaceId().is_valid());
}

bool WebMediaPlayerImpl::SetSinkId(
    const blink::WebString& sink_id,
    blink::WebSetSinkIdCompleteCallback completion_callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __func__;

  OutputDeviceStatusCB callback =
      ConvertToOutputDeviceStatusCB(std::move(completion_callback));
  auto sink_id_utf8 = sink_id.Utf8();
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SetSinkIdOnMediaThread, audio_source_provider_,
                                sink_id_utf8, std::move(callback)));
  return true;
}

STATIC_ASSERT_ENUM(WebMediaPlayer::kPreloadNone, MultibufferDataSource::NONE);
STATIC_ASSERT_ENUM(WebMediaPlayer::kPreloadMetaData,
                   MultibufferDataSource::METADATA);
STATIC_ASSERT_ENUM(WebMediaPlayer::kPreloadAuto, MultibufferDataSource::AUTO);

void WebMediaPlayerImpl::SetPreload(WebMediaPlayer::Preload preload) {
  DVLOG(1) << __func__ << "(" << preload << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  preload_ = static_cast<MultibufferDataSource::Preload>(preload);
  if (mb_data_source_)
    mb_data_source_->SetPreload(preload_);
}

bool WebMediaPlayerImpl::HasVideo() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_metadata_.has_video;
}

bool WebMediaPlayerImpl::HasAudio() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_metadata_.has_audio;
}

void WebMediaPlayerImpl::EnabledAudioTracksChanged(
    const blink::WebVector<blink::WebMediaPlayer::TrackId>& enabledTrackIds) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  std::ostringstream logstr;
  std::vector<MediaTrack::Id> enabledMediaTrackIds;
  for (const auto& blinkTrackId : enabledTrackIds) {
    const auto track_id = MediaTrack::Id(blinkTrackId.Utf8().data());
    logstr << track_id << " ";
    enabledMediaTrackIds.push_back(track_id);
  }
  MEDIA_LOG(INFO, media_log_.get())
      << "Enabled audio tracks: [" << logstr.str() << "]";
  pipeline_controller_->OnEnabledAudioTracksChanged(enabledMediaTrackIds);
}

void WebMediaPlayerImpl::SelectedVideoTrackChanged(
    blink::WebMediaPlayer::TrackId* selectedTrackId) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::Optional<MediaTrack::Id> selected_video_track_id;
  if (selectedTrackId && !video_track_disabled_)
    selected_video_track_id = MediaTrack::Id(selectedTrackId->Utf8().data());
  MEDIA_LOG(INFO, media_log_.get())
      << "Selected video track: ["
      << selected_video_track_id.value_or(MediaTrack::Id()) << "]";
  pipeline_controller_->OnSelectedVideoTrackChanged(selected_video_track_id);
}

gfx::Size WebMediaPlayerImpl::NaturalSize() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_metadata_.natural_size;
}

gfx::Size WebMediaPlayerImpl::VisibleSize() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  scoped_refptr<VideoFrame> video_frame = GetCurrentFrameFromCompositor();
  if (!video_frame)
    return gfx::Size();

  return video_frame->visible_rect().size();
}

bool WebMediaPlayerImpl::Paused() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return pipeline_controller_->GetPlaybackRate() == 0.0f;
}

bool WebMediaPlayerImpl::PausedWhenHidden() const {
  return paused_when_hidden_;
}

bool WebMediaPlayerImpl::Seeking() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return false;

  return seeking_;
}

double WebMediaPlayerImpl::Duration() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return std::numeric_limits<double>::quiet_NaN();

  // Use duration from ChunkDemuxer when present. MSE allows users to specify
  // duration as a double. This propagates to the rest of the pipeline as a
  // TimeDelta with potentially reduced precision (limited to Microseconds).
  // ChunkDemuxer returns the full-precision user-specified double. This ensures
  // users can "get" the exact duration they "set".
  if (chunk_demuxer_)
    return chunk_demuxer_->GetDuration();

  base::TimeDelta pipeline_duration = GetPipelineMediaDuration();
  return pipeline_duration == kInfiniteDuration
             ? std::numeric_limits<double>::infinity()
             : pipeline_duration.InSecondsF();
}

double WebMediaPlayerImpl::timelineOffset() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (pipeline_metadata_.timeline_offset.is_null())
    return std::numeric_limits<double>::quiet_NaN();

  return pipeline_metadata_.timeline_offset.ToJsTime();
}

base::TimeDelta WebMediaPlayerImpl::GetCurrentTimeInternal() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::TimeDelta current_time;
  if (Seeking())
    current_time = seek_time_;
  else if (paused_)
    current_time = paused_time_;
  else
    current_time = pipeline_controller_->GetMediaTime();

  // It's possible for |current_time| to be kInfiniteDuration here if the page
  // seeks to kInfiniteDuration (2**64 - 1) when Duration() is infinite.
  DCHECK_GE(current_time, base::TimeDelta());
  return current_time;
}

double WebMediaPlayerImpl::CurrentTime() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::kReadyStateHaveNothing);

  // Even though we have an explicit ended signal, a lot of content doesn't have
  // an accurate duration -- with some formats (e.g., VBR MP3, OGG) it can't be
  // known without a complete play-through from beginning to end.
  //
  // The HTML5 spec says that upon ended, current time must equal duration. Due
  // to the aforementioned issue, if we rely exclusively on current time, we can
  // be a few milliseconds off of the duration.
  const auto duration = Duration();
  return (ended_ && !std::isinf(duration))
             ? duration
             : GetCurrentTimeInternal().InSecondsF();
}

bool WebMediaPlayerImpl::IsEnded() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return ended_;
}

WebMediaPlayer::NetworkState WebMediaPlayerImpl::GetNetworkState() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerImpl::GetReadyState() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return ready_state_;
}

blink::WebMediaPlayer::SurfaceLayerMode
WebMediaPlayerImpl::GetVideoSurfaceLayerMode() const {
  return surface_layer_mode_;
}

blink::WebString WebMediaPlayerImpl::GetErrorMessage() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return blink::WebString::FromUTF8(media_log_->GetErrorMessage());
}

blink::WebTimeRanges WebMediaPlayerImpl::Buffered() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  Ranges<base::TimeDelta> buffered_time_ranges =
      pipeline_controller_->GetBufferedTimeRanges();

  const base::TimeDelta duration = GetPipelineMediaDuration();
  if (duration != kInfiniteDuration) {
    buffered_data_source_host_->AddBufferedTimeRanges(&buffered_time_ranges,
                                                      duration);
  }
  return blink::ConvertToWebTimeRanges(buffered_time_ranges);
}

blink::WebTimeRanges WebMediaPlayerImpl::Seekable() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (ready_state_ < WebMediaPlayer::kReadyStateHaveMetadata)
    return blink::WebTimeRanges();

  const double seekable_end = Duration();

  // Allow a special exception for seeks to zero for streaming sources with a
  // finite duration; this allows looping to work.
  const bool is_finite_stream = mb_data_source_ &&
                                mb_data_source_->IsStreaming() &&
                                std::isfinite(seekable_end);

  // Do not change the seekable range when using the MediaPlayerRenderer. It
  // will take care of dropping invalid seeks.
  const bool force_seeks_to_zero =
      !using_media_player_renderer_ && is_finite_stream;

  // TODO(dalecurtis): Technically this allows seeking on media which return an
  // infinite duration so long as DataSource::IsStreaming() is false. While not
  // expected, disabling this breaks semi-live players, http://crbug.com/427412.
  const blink::WebTimeRange seekable_range(
      0.0, force_seeks_to_zero ? 0.0 : seekable_end);
  return blink::WebTimeRanges(&seekable_range, 1);
}

bool WebMediaPlayerImpl::IsPrerollAttemptNeeded() {
  // TODO(sandersd): Replace with |highest_ready_state_since_seek_| if we need
  // to ensure that preroll always gets a chance to complete.
  // See http://crbug.com/671525.
  //
  // Note: Even though we get play/pause signals at kReadyStateHaveMetadata, we
  // must attempt to preroll until kReadyStateHaveFutureData so that the
  // canplaythrough event will be fired to the page (which may be waiting).
  //
  // TODO(dalecurtis): We should try signaling kReadyStateHaveFutureData upon
  // automatic-suspend of a non-playing element to avoid wasting resources.
  if (highest_ready_state_ >= ReadyState::kReadyStateHaveFutureData)
    return false;

  // To suspend before we reach kReadyStateHaveCurrentData is only ok
  // if we know we're going to get woken up when we get more data, which
  // will only happen if the network is in the "Loading" state.
  // This happens when the network is fast, but multiple videos are loading
  // and multiplexing gets held up waiting for available threads.
  if (highest_ready_state_ <= ReadyState::kReadyStateHaveMetadata &&
      network_state_ != WebMediaPlayer::kNetworkStateLoading) {
    return true;
  }

  if (preroll_attempt_pending_)
    return true;

  // Freshly initialized; there has never been any loading progress. (Otherwise
  // |preroll_attempt_pending_| would be true when the start time is null.)
  if (preroll_attempt_start_time_.is_null())
    return false;

  base::TimeDelta preroll_attempt_duration =
      tick_clock_->NowTicks() - preroll_attempt_start_time_;
  return preroll_attempt_duration < kPrerollAttemptTimeout;
}

bool WebMediaPlayerImpl::DidLoadingProgress() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Note: Separate variables used to ensure both methods are called every time.
  const bool pipeline_progress = pipeline_controller_->DidLoadingProgress();
  const bool data_progress = buffered_data_source_host_->DidLoadingProgress();
  return pipeline_progress || data_progress;
}

void WebMediaPlayerImpl::Paint(cc::PaintCanvas* canvas,
                               const gfx::Rect& rect,
                               cc::PaintFlags& flags) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl:paint");

  scoped_refptr<VideoFrame> video_frame = GetCurrentFrameFromCompositor();

  gfx::Rect gfx_rect(rect);
  if (video_frame && video_frame->HasTextures()) {
    if (!raster_context_provider_)
      return;  // Unable to get/create a shared main thread context.
    if (!raster_context_provider_->GrContext() &&
        !raster_context_provider_->ContextCapabilities().supports_oop_raster) {
      return;  // The context has been lost.
    }
  }
  video_renderer_.Paint(
      video_frame, canvas, gfx::RectF(gfx_rect), flags,
      pipeline_metadata_.video_decoder_config.video_transformation(),
      raster_context_provider_.get());
}

scoped_refptr<VideoFrame> WebMediaPlayerImpl::GetCurrentFrame() {
  return GetCurrentFrameFromCompositor();
}

media::PaintCanvasVideoRenderer*
WebMediaPlayerImpl::GetPaintCanvasVideoRenderer() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return &video_renderer_;
}

bool WebMediaPlayerImpl::WouldTaintOrigin() const {
  if (demuxer_found_hls_) {
    // HLS manifests might pull segments from a different origin. We can't know
    // for sure, so we conservatively say no here.
    return true;
  }

  if (!mb_data_source_)
    return false;

  // When the resource is redirected to another origin we think it as
  // tainted. This is actually not specified, and is under discussion.
  // See https://github.com/whatwg/fetch/issues/737.
  if (!mb_data_source_->HasSingleOrigin() &&
      mb_data_source_->cors_mode() == UrlData::CORS_UNSPECIFIED) {
    return true;
  }

  return mb_data_source_->IsCorsCrossOrigin();
}

double WebMediaPlayerImpl::MediaTimeForTimeValue(double timeValue) const {
  return base::TimeDelta::FromSecondsD(timeValue).InSecondsF();
}

unsigned WebMediaPlayerImpl::DecodedFrameCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return GetPipelineStatistics().video_frames_decoded;
}

unsigned WebMediaPlayerImpl::DroppedFrameCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return GetPipelineStatistics().video_frames_dropped;
}

uint64_t WebMediaPlayerImpl::AudioDecodedByteCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return GetPipelineStatistics().audio_bytes_decoded;
}

uint64_t WebMediaPlayerImpl::VideoDecodedByteCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return GetPipelineStatistics().video_bytes_decoded;
}

bool WebMediaPlayerImpl::HasAvailableVideoFrame() const {
  return has_first_frame_;
}

void WebMediaPlayerImpl::SetContentDecryptionModule(
    blink::WebContentDecryptionModule* cdm,
    blink::WebContentDecryptionModuleResult result) {
  DVLOG(1) << __func__ << ": cdm = " << cdm;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Once the CDM is set it can't be cleared as there may be frames being
  // decrypted on other threads. So fail this request.
  // http://crbug.com/462365#c7.
  if (!cdm) {
    result.CompleteWithError(
        blink::kWebContentDecryptionModuleExceptionInvalidStateError, 0,
        "The existing ContentDecryptionModule object cannot be removed at this "
        "time.");
    return;
  }

  // Create a local copy of |result| to avoid problems with the callback
  // getting passed to the media thread and causing |result| to be destructed
  // on the wrong thread in some failure conditions. Blink should prevent
  // multiple simultaneous calls.
  DCHECK(!set_cdm_result_);
  set_cdm_result_ =
      std::make_unique<blink::WebContentDecryptionModuleResult>(result);

  SetCdmInternal(cdm);
}

void WebMediaPlayerImpl::OnEncryptedMediaInitData(
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data) {
  DCHECK(init_data_type != EmeInitDataType::UNKNOWN);

  RecordEncryptedEvent(true);

  // Recreate the watch time reporter if necessary.
  const bool was_encrypted = is_encrypted_;
  is_encrypted_ = true;
  if (!was_encrypted) {
    media_metrics_provider_->SetIsEME();
    if (watch_time_reporter_)
      CreateWatchTimeReporter();

    // |was_encrypted| = false means we didn't have a CDM prior to observing
    // encrypted media init data. Reset the reporter until the CDM arrives. See
    // SetCdmInternal().
    DCHECK(!cdm_config_);
    video_decode_stats_reporter_.reset();
  }

  encrypted_client_->Encrypted(
      init_data_type, init_data.data(),
      base::saturated_cast<unsigned int>(init_data.size()));
}

void WebMediaPlayerImpl::OnFFmpegMediaTracksUpdated(
    std::unique_ptr<MediaTracks> tracks) {
  // For MSE/chunk_demuxer case the media track updates are handled by
  // WebSourceBufferImpl.
  DCHECK(demuxer_.get());
  DCHECK(!chunk_demuxer_);

  // Report the media track information to blink. Only the first audio track and
  // the first video track are enabled by default to match blink logic.
  bool is_first_audio_track = true;
  bool is_first_video_track = true;
  for (const auto& track : tracks->tracks()) {
    if (track->type() == MediaTrack::Audio) {
      client_->AddAudioTrack(
          blink::WebString::FromUTF8(track->id().value()),
          blink::WebMediaPlayerClient::kAudioTrackKindMain,
          blink::WebString::FromUTF8(track->label().value()),
          blink::WebString::FromUTF8(track->language().value()),
          is_first_audio_track);
      is_first_audio_track = false;
    } else if (track->type() == MediaTrack::Video) {
      client_->AddVideoTrack(
          blink::WebString::FromUTF8(track->id().value()),
          blink::WebMediaPlayerClient::kVideoTrackKindMain,
          blink::WebString::FromUTF8(track->label().value()),
          blink::WebString::FromUTF8(track->language().value()),
          is_first_video_track);
      is_first_video_track = false;
    } else {
      // Text tracks are not supported through this code path yet.
      NOTREACHED();
    }
  }
}

void WebMediaPlayerImpl::SetCdmInternal(
    blink::WebContentDecryptionModule* cdm) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(cdm);

  const bool was_encrypted = is_encrypted_;
  is_encrypted_ = true;

  // Recreate the watch time reporter if necessary.
  if (!was_encrypted) {
    media_metrics_provider_->SetIsEME();
    if (watch_time_reporter_)
      CreateWatchTimeReporter();
  }

  WebContentDecryptionModuleImpl* web_cdm =
      ToWebContentDecryptionModuleImpl(cdm);
  auto cdm_context_ref = web_cdm->GetCdmContextRef();
  if (!cdm_context_ref) {
    NOTREACHED();
    OnCdmAttached(false);
    return;
  }

  // Arrival of |cdm_config_| and |key_system_| unblocks recording of encrypted
  // stats. Attempt to create the stats reporter. Note, we do NOT guard this
  // within !was_encypted above because often the CDM arrives after the call to
  // OnEncryptedMediaInitData().
  cdm_config_ = web_cdm->GetCdmConfig();
  key_system_ = web_cdm->GetKeySystem();
  DCHECK(!key_system_.empty());
  CreateVideoDecodeStatsReporter();

  CdmContext* cdm_context = cdm_context_ref->GetCdmContext();
  DCHECK(cdm_context);

  // Keep the reference to the CDM, as it shouldn't be destroyed until
  // after the pipeline is done with the |cdm_context|.
  pending_cdm_context_ref_ = std::move(cdm_context_ref);
  pipeline_controller_->SetCdm(
      cdm_context,
      base::BindOnce(&WebMediaPlayerImpl::OnCdmAttached, weak_this_));
}

void WebMediaPlayerImpl::OnCdmAttached(bool success) {
  DVLOG(1) << __func__ << ": success = " << success;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(pending_cdm_context_ref_);

  // If the CDM is set from the constructor there is no promise
  // (|set_cdm_result_|) to fulfill.
  if (success) {
    media_log_->SetProperty<MediaLogProperty::kIsVideoEncrypted>(true);

    // This will release the previously attached CDM (if any).
    cdm_context_ref_ = std::move(pending_cdm_context_ref_);
    if (set_cdm_result_) {
      set_cdm_result_->Complete();
      set_cdm_result_.reset();
    }

    return;
  }

  pending_cdm_context_ref_.reset();
  if (set_cdm_result_) {
    set_cdm_result_->CompleteWithError(
        blink::kWebContentDecryptionModuleExceptionNotSupportedError, 0,
        "Unable to set ContentDecryptionModule object");
    set_cdm_result_.reset();
  }
}

void WebMediaPlayerImpl::OnPipelineSeeked(bool time_updated) {
  TRACE_EVENT2("media", "WebMediaPlayerImpl::OnPipelineSeeked", "target",
               seek_time_.InSecondsF(), "id", media_log_->id());
  seeking_ = false;
  seek_time_ = base::TimeDelta();

  if (paused_) {
    paused_time_ = pipeline_controller_->GetMediaTime();
  } else {
    DCHECK(watch_time_reporter_);
    watch_time_reporter_->OnPlaying();
    if (playback_events_recorder_)
      playback_events_recorder_->OnPlaying();
  }
  if (time_updated)
    should_notify_time_changed_ = true;

  // Reset underflow duration upon seek; this prevents looping videos and user
  // actions from artificially inflating the duration.
  underflow_timer_.reset();

  // Background video optimizations are delayed when shown/hidden if pipeline
  // is seeking.
  UpdateBackgroundVideoOptimizationState();

  // If we successfully completed a suspended startup, we need to make a call to
  // UpdatePlayState() in case any events which should trigger a resume have
  // occurred during startup.
  if (attempting_suspended_start_ &&
      pipeline_controller_->IsPipelineSuspended()) {
    skip_metrics_due_to_startup_suspend_ = true;

    // If we successfully completed a suspended startup, signal that we have
    // reached BUFFERING_HAVE_ENOUGH so that canplay and canplaythrough fire
    // correctly. We must unfortunately always do this because it's valid for
    // elements to play while not visible nor even in the DOM.
    //
    // Note: This call is dual purpose, it is also responsible for triggering an
    // UpdatePlayState() call which may need to resume the pipeline once Blink
    // has been told about the ReadyState change.
    OnBufferingStateChangeInternal(BUFFERING_HAVE_ENOUGH,
                                   BUFFERING_CHANGE_REASON_UNKNOWN, true);
  }

  attempting_suspended_start_ = false;
}

void WebMediaPlayerImpl::OnPipelineSuspended() {
  // Add a log event so the player shows up as "SUSPENDED" in media-internals.
  media_log_->AddEvent<MediaLogEvent::kSuspended>();

  if (attempting_suspended_start_) {
    DCHECK(pipeline_controller_->IsSuspended());
    did_lazy_load_ = !has_poster_ && HasVideo();
  }

  // Tell the data source we have enough data so that it may release the
  // connection (unless blink is waiting on us to signal play()).
  if (mb_data_source_ && !client_->CouldPlayIfEnoughData()) {
    // |attempting_suspended_start_| will be cleared by OnPipelineSeeked() which
    // will occur after this method during a suspended startup.
    if (attempting_suspended_start_ && did_lazy_load_) {
      DCHECK(!has_first_frame_);
      DCHECK(have_enough_after_lazy_load_cb_.IsCancelled());

      // For lazy load, we won't know if the element is non-visible until a
      // layout completes, so to avoid unnecessarily tearing down the network
      // connection, briefly (250ms chosen arbitrarily) delay signaling "have
      // enough" to the MultiBufferDataSource.
      //
      // base::Unretained() is safe here since the base::CancelableOnceClosure
      // will cancel upon destruction of this class and |mb_data_source_| is
      // gauranteeed to outlive us.
      have_enough_after_lazy_load_cb_.Reset(
          base::BindOnce(&MultibufferDataSource::OnBufferingHaveEnough,
                         base::Unretained(mb_data_source_), true));
      main_task_runner_->PostDelayedTask(
          FROM_HERE, have_enough_after_lazy_load_cb_.callback(),
          base::TimeDelta::FromMilliseconds(250));
    } else {
      have_enough_after_lazy_load_cb_.Cancel();
      mb_data_source_->OnBufferingHaveEnough(true);
    }
  }

  ReportMemoryUsage();

  if (pending_suspend_resume_cycle_) {
    pending_suspend_resume_cycle_ = false;
    UpdatePlayState();
  }
}

void WebMediaPlayerImpl::OnBeforePipelineResume() {
  // Since we're resuming, cancel closing of the network connection.
  have_enough_after_lazy_load_cb_.Cancel();

  // We went through suspended startup, so the player is only just now spooling
  // up for playback. As such adjust |load_start_time_| so it reports the same
  // metric as what would be reported if we had not suspended at startup.
  if (skip_metrics_due_to_startup_suspend_) {
    // In the event that the call to SetReadyState() initiated after pipeline
    // startup immediately tries to start playback, we should not update
    // |load_start_time_| to avoid losing visibility into the impact of a
    // suspended startup on the time until first frame / play ready for cases
    // where suspended startup was applied incorrectly.
    if (!attempting_suspended_start_)
      load_start_time_ = base::TimeTicks::Now() - time_to_metadata_;
    skip_metrics_due_to_startup_suspend_ = false;
  }

  // Enable video track if we disabled it in the background - this way the new
  // renderer will attach its callbacks to the video stream properly.
  // TODO(avayvod): Remove this when disabling and enabling video tracks in
  // non-playing state works correctly. See https://crbug.com/678374.
  EnableVideoTrackIfNeeded();
  is_pipeline_resuming_ = true;
}

void WebMediaPlayerImpl::OnPipelineResumed() {
  is_pipeline_resuming_ = false;

  UpdateBackgroundVideoOptimizationState();
}

void WebMediaPlayerImpl::OnDemuxerOpened() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  client_->MediaSourceOpened(new WebMediaSourceImpl(chunk_demuxer_));
}

void WebMediaPlayerImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  DVLOG(2) << __func__ << " memory_pressure_level=" << memory_pressure_level;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(base::FeatureList::IsEnabled(kMemoryPressureBasedSourceBufferGC));
  DCHECK(chunk_demuxer_);

  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  // The new value of |memory_pressure_level| will take effect on the next
  // garbage collection. Typically this means the next SourceBuffer append()
  // operation, since per MSE spec, the garbage collection must only occur
  // during SourceBuffer append(). But if memory pressure is critical it might
  // be better to perform GC immediately rather than wait for the next append
  // and potentially get killed due to out-of-memory.
  // So if this experiment is enabled and pressure level is critical, we'll pass
  // down force_instant_gc==true, which will force immediate GC on
  // SourceBufferStreams.
  bool force_instant_gc =
      (enable_instant_source_buffer_gc_ &&
       memory_pressure_level ==
           base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // base::Unretained is safe, since |chunk_demuxer_| is actually owned by
  // |this| via this->demuxer_. Note the destruction of |chunk_demuxer_| is done
  // from ~WMPI by first hopping to |media_task_runner_| to prevent race with
  // this task.
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ChunkDemuxer::OnMemoryPressure,
                                base::Unretained(chunk_demuxer_),
                                base::TimeDelta::FromSecondsD(CurrentTime()),
                                memory_pressure_level, force_instant_gc));
}

void WebMediaPlayerImpl::OnError(PipelineStatus status) {
  DVLOG(1) << __func__ << ": status=" << status;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(status, PIPELINE_OK);

  if (suppress_destruction_errors_)
    return;

#if defined(OS_ANDROID)
  // |mb_data_source_| may be nullptr if someone passes in a m3u8 as a data://
  // URL, since MediaPlayer doesn't support data:// URLs, fail playback now.
  const bool found_hls = base::FeatureList::IsEnabled(kHlsPlayer) &&
                         status == PipelineStatus::DEMUXER_ERROR_DETECTED_HLS;
  if (found_hls && mb_data_source_) {
    demuxer_found_hls_ = true;

    if (observer_)
      observer_->OnHlsManifestDetected();

    UMA_HISTOGRAM_BOOLEAN("Media.WebMediaPlayerImpl.HLS.IsCorsCrossOrigin",
                          mb_data_source_->IsCorsCrossOrigin());
    if (mb_data_source_->IsCorsCrossOrigin()) {
      UMA_HISTOGRAM_BOOLEAN("Media.WebMediaPlayerImpl.HLS.HasAccessControl",
                            mb_data_source_->HasAccessControl());
    }

    // Note: Does not consider the full redirect chain, which could contain
    // undetected mixed content.
    bool frame_url_is_cryptographic = url::Origin(frame_->GetSecurityOrigin())
                                          .GetURL()
                                          .SchemeIsCryptographic();
    bool manifest_url_is_cryptographic =
        loaded_url_.SchemeIsCryptographic() &&
        mb_data_source_->GetUrlAfterRedirects().SchemeIsCryptographic();
    UMA_HISTOGRAM_BOOLEAN(
        "Media.WebMediaPlayerImpl.HLS.IsMixedContent",
        frame_url_is_cryptographic && !manifest_url_is_cryptographic);

    renderer_factory_selector_->SetBaseFactoryType(
        RendererFactoryType::kMediaPlayer);

    loaded_url_ = mb_data_source_->GetUrlAfterRedirects();
    DCHECK(data_source_);
    data_source_->Stop();
    mb_data_source_ = nullptr;

    pipeline_controller_->Stop();
    SetMemoryReportingState(false);
    media_task_runner_->DeleteSoon(FROM_HERE,
                                   std::move(media_thread_mem_dumper_));

    // Trampoline through the media task runner to destruct the demuxer and
    // data source now that we're switching to HLS playback.
    media_task_runner_->PostTask(
        FROM_HERE,
        BindToCurrentLoop(base::BindOnce(
            [](std::unique_ptr<Demuxer> demuxer,
               std::unique_ptr<DataSource> data_source,
               base::OnceClosure start_pipeline_cb) {
              // Release resources before starting HLS.
              demuxer.reset();
              data_source.reset();

              std::move(start_pipeline_cb).Run();
            },
            std::move(demuxer_), std::move(data_source_),
            base::BindOnce(&WebMediaPlayerImpl::StartPipeline, weak_this_))));

    return;
  }

  // We found hls in a data:// URL, fail immediately.
  if (found_hls)
    status = PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED;
#endif

  MaybeSetContainerNameForMetrics();
  simple_watch_timer_.Stop();
  media_log_->NotifyError(status);
  media_metrics_provider_->OnError(status);
  if (playback_events_recorder_)
    playback_events_recorder_->OnError(status);
  if (watch_time_reporter_)
    watch_time_reporter_->OnError(status);

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    SetNetworkState(WebMediaPlayer::kNetworkStateFormatError);
  } else {
    SetNetworkState(blink::PipelineErrorToNetworkState(status));
  }

  // PipelineController::Stop() is idempotent.
  pipeline_controller_->Stop();

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnEnded() {
  TRACE_EVENT2("media", "WebMediaPlayerImpl::OnEnded", "duration", Duration(),
               "id", media_log_->id());
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Ignore state changes until we've completed all outstanding operations.
  if (!pipeline_controller_->IsStable())
    return;

  ended_ = true;
  client_->TimeChanged();

  if (playback_events_recorder_)
    playback_events_recorder_->OnEnded();

  // We don't actually want this to run until |client_| calls seek() or pause(),
  // but that should have already happened in timeChanged() and so this is
  // expected to be a no-op.
  UpdatePlayState();
}

void WebMediaPlayerImpl::OnMetadata(const PipelineMetadata& metadata) {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Cache the |time_to_metadata_| to use for adjusting the TimeToFirstFrame and
  // TimeToPlayReady metrics later if we end up doing a suspended startup.
  time_to_metadata_ = base::TimeTicks::Now() - load_start_time_;
  media_metrics_provider_->SetTimeToMetadata(time_to_metadata_);
  RecordTimingUMA("Media.TimeToMetadata", time_to_metadata_);

  MaybeSetContainerNameForMetrics();

  pipeline_metadata_ = metadata;
  if (power_status_helper_)
    power_status_helper_->SetMetadata(metadata);

  UMA_HISTOGRAM_ENUMERATION(
      "Media.VideoRotation",
      metadata.video_decoder_config.video_transformation().rotation,
      VIDEO_ROTATION_MAX + 1);

  if (HasAudio()) {
    media_metrics_provider_->SetHasAudio(metadata.audio_decoder_config.codec());
    RecordEncryptionScheme("Audio",
                           metadata.audio_decoder_config.encryption_scheme());
  }

  if (HasVideo()) {
    media_metrics_provider_->SetHasVideo(metadata.video_decoder_config.codec());
    RecordEncryptionScheme("Video",
                           metadata.video_decoder_config.encryption_scheme());

    if (overlay_enabled_) {
      // SurfaceView doesn't support rotated video, so transition back if
      // the video is now rotated.  If |always_enable_overlays_|, we keep the
      // overlay anyway so that the state machine keeps working.
      // TODO(liberato): verify if compositor feedback catches this.  If so,
      // then we don't need this check.
      if (!always_enable_overlays_ && !DoesOverlaySupportMetadata())
        DisableOverlay();
    }

    if (surface_layer_mode_ ==
        blink::WebMediaPlayer::SurfaceLayerMode::kAlways) {
      ActivateSurfaceLayerForVideo();
    } else {
      DCHECK(!video_layer_);
      // TODO(tmathmeyer) does this need support for reflections as well?
      video_layer_ = cc::VideoLayer::Create(
          compositor_.get(),
          pipeline_metadata_.video_decoder_config.video_transformation()
              .rotation);
      video_layer_->SetContentsOpaque(opaque_);
      client_->SetCcLayer(video_layer_.get());
    }
  }

  if (observer_)
    observer_->OnMetadataChanged(pipeline_metadata_);

  delegate_has_audio_ = HasUnmutedAudio();
  MediaContentType content_type =
      DurationToMediaContentType(GetPipelineMediaDuration());
  client_->DidMediaMetadataChange(delegate_has_audio_, HasVideo(),
                                  content_type);
  delegate_->DidMediaMetadataChange(delegate_id_, delegate_has_audio_,
                                    HasVideo(), content_type);

  // It could happen that the demuxer successfully completed initialization
  // (implying it had determined media metadata), but then removed all audio and
  // video streams and the ability to demux any A/V before |metadata| was
  // constructed and passed to us. One example is, with MSE-in-Workers, the
  // worker owning the MediaSource could have been terminated, or the app could
  // have explicitly removed all A/V SourceBuffers. That termination/removal
  // could race the construction of |metadata|. Regardless of load-type, we
  // shouldn't allow playback of a resource that has neither audio nor video.
  // We treat lack of A/V as if there were an error in the demuxer before
  // reaching HAVE_METADATA.
  if (!HasVideo() && !HasAudio()) {
    DVLOG(1) << __func__ << ": no audio and no video -> error";
    OnError(PipelineStatus::DEMUXER_ERROR_COULD_NOT_OPEN);
    return;  // Do not transition to HAVE_METADATA.
  }

  // TODO(dalecurtis): Don't create these until kReadyStateHaveFutureData; when
  // we create them early we just increase the chances of needing to throw them
  // away unnecessarily.
  CreateWatchTimeReporter();
  CreateVideoDecodeStatsReporter();

  // SetReadyState() may trigger all sorts of calls into this class (e.g.,
  // Play(), Pause(), etc) so do it last to avoid unexpected states during the
  // calls. An exception to this is UpdatePlayState(), which is safe to call and
  // needs to use the new ReadyState in its calculations.
  SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);
  UpdatePlayState();
}

void WebMediaPlayerImpl::ActivateSurfaceLayerForVideo() {
  // Note that we might or might not already be in VideoLayer mode.
  DCHECK(!bridge_);

  surface_layer_for_video_enabled_ = true;

  // If we're in VideoLayer mode, then get rid of the layer.
  if (video_layer_) {
    client_->SetCcLayer(nullptr);
    video_layer_ = nullptr;
  }

  bridge_ = std::move(create_bridge_callback_)
                .Run(this, compositor_->GetUpdateSubmissionStateCallback());
  bridge_->CreateSurfaceLayer();

  // TODO(tmathmeyer) does this need support for the reflection transformation
  // as well?
  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoFrameCompositor::EnableSubmission,
          base::Unretained(compositor_.get()), bridge_->GetSurfaceId(),
          pipeline_metadata_.video_decoder_config.video_transformation()
              .rotation,
          IsInPictureInPicture()));
  bridge_->SetContentsOpaque(opaque_);

  // If the element is already in Picture-in-Picture mode, it means that it
  // was set in this mode prior to this load, with a different
  // WebMediaPlayerImpl. The new player needs to send its id, size and
  // surface id to the browser process to make sure the states are properly
  // updated.
  // TODO(872056): the surface should be activated but for some reasons, it
  // does not. It is possible that this will no longer be needed after 872056
  // is fixed.
  if (IsInPictureInPicture())
    OnSurfaceIdUpdated(bridge_->GetSurfaceId());
}

void WebMediaPlayerImpl::OnBufferingStateChange(
    BufferingState state,
    BufferingStateChangeReason reason) {
  OnBufferingStateChangeInternal(state, reason, false);
}

void WebMediaPlayerImpl::CreateVideoDecodeStatsReporter() {
  // TODO(chcunningham): destroy reporter if we initially have video but the
  // track gets disabled. Currently not possible in default desktop Chrome.
  if (!HasVideo())
    return;

  // Only record stats from the local pipeline.
  if (is_flinging_ || is_remote_rendering_ || using_media_player_renderer_)
    return;

  // Stats reporter requires a valid config. We may not have one for HLS cases
  // where URL demuxer doesn't know details of the stream.
  if (!pipeline_metadata_.video_decoder_config.IsValidConfig())
    return;

  // Profile must be known for use as index to save the reported stats.
  if (pipeline_metadata_.video_decoder_config.profile() ==
      VIDEO_CODEC_PROFILE_UNKNOWN) {
    return;
  }

  // CdmConfig must be provided for use as index to save encrypted stats.
  if (is_encrypted_ && !cdm_config_) {
    return;
  } else if (cdm_config_) {
    DCHECK(!key_system_.empty());
  }

  mojo::PendingRemote<mojom::VideoDecodeStatsRecorder> recorder;
  media_metrics_provider_->AcquireVideoDecodeStatsRecorder(
      recorder.InitWithNewPipeAndPassReceiver());

  // Create capabilities reporter and synchronize its initial state.
  video_decode_stats_reporter_ = std::make_unique<VideoDecodeStatsReporter>(
      std::move(recorder),
      base::BindRepeating(&WebMediaPlayerImpl::GetPipelineStatistics,
                          base::Unretained(this)),
      pipeline_metadata_.video_decoder_config.profile(),
      pipeline_metadata_.natural_size, key_system_, cdm_config_,
      frame_->GetTaskRunner(blink::TaskType::kInternalMedia));

  if (delegate_->IsFrameHidden())
    video_decode_stats_reporter_->OnHidden();
  else
    video_decode_stats_reporter_->OnShown();

  if (paused_)
    video_decode_stats_reporter_->OnPaused();
  else
    video_decode_stats_reporter_->OnPlaying();
}

void WebMediaPlayerImpl::OnProgress() {
  DVLOG(4) << __func__;

  // See IsPrerollAttemptNeeded() for more details. We can't use that method
  // here since it considers |preroll_attempt_start_time_| and for OnProgress()
  // events we must make the attempt -- since there may not be another event.
  if (highest_ready_state_ < ReadyState::kReadyStateHaveFutureData) {
    // Reset the preroll attempt clock.
    preroll_attempt_pending_ = true;
    preroll_attempt_start_time_ = base::TimeTicks();

    // Clear any 'stale' flag and give the pipeline a chance to resume. If we
    // are already resumed, this will cause |preroll_attempt_start_time_| to
    // be set.
    delegate_->ClearStaleFlag(delegate_id_);
    UpdatePlayState();
  } else if (ready_state_ == ReadyState::kReadyStateHaveFutureData &&
             CanPlayThrough()) {
    SetReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);
  }
}

bool WebMediaPlayerImpl::CanPlayThrough() {
  if (!base::FeatureList::IsEnabled(kSpecCompliantCanPlayThrough))
    return true;
  if (chunk_demuxer_)
    return true;
  if (data_source_ && data_source_->AssumeFullyBuffered())
    return true;
  // If we're not currently downloading, we have as much buffer as
  // we're ever going to get, which means we say we can play through.
  if (network_state_ == WebMediaPlayer::kNetworkStateIdle)
    return true;
  return buffered_data_source_host_->CanPlayThrough(
      base::TimeDelta::FromSecondsD(CurrentTime()),
      base::TimeDelta::FromSecondsD(Duration()),
      playback_rate_ == 0.0 ? 1.0 : playback_rate_);
}

void WebMediaPlayerImpl::OnBufferingStateChangeInternal(
    BufferingState state,
    BufferingStateChangeReason reason,
    bool for_suspended_start) {
  DVLOG(1) << __func__ << "(" << state << ", " << reason << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Ignore buffering state changes caused by back-to-back seeking, so as not
  // to assume the second seek has finished when it was only the first seek.
  if (pipeline_controller_->IsPendingSeek())
    return;

  media_log_->AddEvent<MediaLogEvent::kBufferingStateChanged>(
      SerializableBufferingState<SerializableBufferingStateType::kPipeline>{
          state, reason, for_suspended_start});

  if (state == BUFFERING_HAVE_ENOUGH && !for_suspended_start)
    media_metrics_provider_->SetHaveEnough();

  if (state == BUFFERING_HAVE_ENOUGH) {
    TRACE_EVENT1("media", "WebMediaPlayerImpl::BufferingHaveEnough", "id",
                 media_log_->id());
    // The SetReadyState() call below may clear
    // |skip_metrics_due_to_startup_suspend_| so report this first.
    if (!have_reported_time_to_play_ready_ &&
        !skip_metrics_due_to_startup_suspend_) {
      DCHECK(!for_suspended_start);
      have_reported_time_to_play_ready_ = true;
      const base::TimeDelta elapsed = base::TimeTicks::Now() - load_start_time_;
      media_metrics_provider_->SetTimeToPlayReady(elapsed);
      RecordTimingUMA("Media.TimeToPlayReady", elapsed);
    }

    // Warning: This call may be re-entrant.
    SetReadyState(CanPlayThrough() ? WebMediaPlayer::kReadyStateHaveEnoughData
                                   : WebMediaPlayer::kReadyStateHaveFutureData);

    // Let the DataSource know we have enough data -- this is the only function
    // during which we advance to (or past) the kReadyStateHaveEnoughData state.
    // It may use this information to update buffer sizes or release unused
    // network connections.
    MaybeUpdateBufferSizesForPlayback();
    if (mb_data_source_ && !client_->CouldPlayIfEnoughData()) {
      // For LazyLoad this will be handled during OnPipelineSuspended().
      if (for_suspended_start && did_lazy_load_)
        DCHECK(!have_enough_after_lazy_load_cb_.IsCancelled());
      else
        mb_data_source_->OnBufferingHaveEnough(false);
    }

    // Blink expects a timeChanged() in response to a seek().
    if (should_notify_time_changed_) {
      should_notify_time_changed_ = false;
      client_->TimeChanged();
    }

    // Once we have enough, start reporting the total memory usage. We'll also
    // report once playback starts.
    ReportMemoryUsage();

    // Report the amount of time it took to leave the underflow state.
    if (underflow_timer_) {
      auto elapsed = underflow_timer_->Elapsed();
      RecordUnderflowDuration(elapsed);
      watch_time_reporter_->OnUnderflowComplete(elapsed);
      underflow_timer_.reset();
    }

    if (playback_events_recorder_)
      playback_events_recorder_->OnBufferingComplete();
  } else {
    // Buffering has underflowed.
    DCHECK_EQ(state, BUFFERING_HAVE_NOTHING);

    // Report the number of times we've entered the underflow state. Ensure we
    // only report the value when transitioning from HAVE_ENOUGH to
    // HAVE_NOTHING.
    if (ready_state_ == WebMediaPlayer::kReadyStateHaveEnoughData &&
        !seeking_) {
      underflow_timer_ = std::make_unique<base::ElapsedTimer>();
      watch_time_reporter_->OnUnderflow();
      client_->DidBufferUnderflow();

      if (playback_events_recorder_)
        playback_events_recorder_->OnBuffering();
    }

    // It shouldn't be possible to underflow if we've not advanced past
    // HAVE_CURRENT_DATA.
    DCHECK_GT(highest_ready_state_, WebMediaPlayer::kReadyStateHaveCurrentData);
    SetReadyState(WebMediaPlayer::kReadyStateHaveCurrentData);
  }

  // If this is an NNR, then notify the smoothness helper about it.  Note that
  // it's unclear what we should do if there is no smoothness helper yet.  As it
  // is, we just discard the NNR.
  if (state == BUFFERING_HAVE_NOTHING && reason == DECODER_UNDERFLOW &&
      smoothness_helper_) {
    smoothness_helper_->NotifyNNR();
  }

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnDurationChange() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (frame_->IsAdSubframe()) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Ads.Media.Duration", GetPipelineMediaDuration(),
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromDays(1),
                               50 /* bucket_count */);
  }

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return;

  client_->DurationChanged();

  MediaContentType content_type =
      DurationToMediaContentType(GetPipelineMediaDuration());
  client_->DidMediaMetadataChange(delegate_has_audio_, HasVideo(),
                                  content_type);
  delegate_->DidMediaMetadataChange(delegate_id_, delegate_has_audio_,
                                    HasVideo(), content_type);

  if (watch_time_reporter_)
    watch_time_reporter_->OnDurationChanged(GetPipelineMediaDuration());
}

void WebMediaPlayerImpl::OnAddTextTrack(const TextTrackConfig& config,
                                        AddTextTrackDoneCB done_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  const WebInbandTextTrackImpl::Kind web_kind =
      static_cast<WebInbandTextTrackImpl::Kind>(config.kind());
  const blink::WebString web_label = blink::WebString::FromUTF8(config.label());
  const blink::WebString web_language =
      blink::WebString::FromUTF8(config.language());
  const blink::WebString web_id = blink::WebString::FromUTF8(config.id());

  std::unique_ptr<WebInbandTextTrackImpl> web_inband_text_track(
      new WebInbandTextTrackImpl(web_kind, web_label, web_language, web_id));

  std::unique_ptr<TextTrack> text_track(new TextTrackImpl(
      main_task_runner_, client_, std::move(web_inband_text_track)));

  std::move(done_cb).Run(std::move(text_track));
}

void WebMediaPlayerImpl::OnWaiting(WaitingReason reason) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  switch (reason) {
    case WaitingReason::kNoCdm:
    case WaitingReason::kNoDecryptionKey:
      encrypted_client_->DidBlockPlaybackWaitingForKey();
      // TODO(jrummell): didResumePlaybackBlockedForKey() should only be called
      // when a key has been successfully added (e.g. OnSessionKeysChange() with
      // |has_additional_usable_key| = true). http://crbug.com/461903
      encrypted_client_->DidResumePlaybackBlockedForKey();
      return;

    // Ideally this should be handled by PipelineController directly without
    // being proxied here. But currently Pipeline::Client (|this|) is passed to
    // PipelineImpl directly without going through |pipeline_controller_|,
    // making it difficult to do.
    // TODO(xhwang): Handle this in PipelineController when we have a clearer
    // picture on how to refactor WebMediaPlayerImpl, PipelineController and
    // PipelineImpl.
    case WaitingReason::kDecoderStateLost:
      pipeline_controller_->OnDecoderStateLost();
      return;
  }
}

void WebMediaPlayerImpl::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::kReadyStateHaveNothing);

  TRACE_EVENT0("media", "WebMediaPlayerImpl::OnVideoNaturalSizeChange");

  // The input |size| is from the decoded video frame, which is the original
  // natural size and need to be rotated accordingly.
  gfx::Size rotated_size = GetRotatedVideoSize(
      pipeline_metadata_.video_decoder_config.video_transformation().rotation,
      size);

  RecordVideoNaturalSize(rotated_size);

  gfx::Size old_size = pipeline_metadata_.natural_size;
  if (rotated_size == old_size)
    return;

  pipeline_metadata_.natural_size = rotated_size;

  if (using_media_player_renderer_ && old_size.IsEmpty()) {
    // If we are using MediaPlayerRenderer and this is the first size change, we
    // now know that there is a video track. This condition is paired with code
    // in CreateWatchTimeReporter() that guesses the existence of a video track.
    CreateWatchTimeReporter();
  } else {
    UpdateSecondaryProperties();
  }

  if (video_decode_stats_reporter_ &&
      !video_decode_stats_reporter_->MatchesBucketedNaturalSize(
          pipeline_metadata_.natural_size)) {
    CreateVideoDecodeStatsReporter();
  }

  // Create or replace the smoothness helper now that we have a size.
  UpdateSmoothnessHelper();

  client_->SizeChanged();

  if (observer_)
    observer_->OnMetadataChanged(pipeline_metadata_);

  client_->DidPlayerSizeChange(NaturalSize());
}

void WebMediaPlayerImpl::OnVideoOpacityChange(bool opaque) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::kReadyStateHaveNothing);

  opaque_ = opaque;
  if (!surface_layer_for_video_enabled_ && video_layer_)
    video_layer_->SetContentsOpaque(opaque_);
  else if (bridge_->GetCcLayer())
    bridge_->SetContentsOpaque(opaque_);
}

void WebMediaPlayerImpl::OnVideoFrameRateChange(base::Optional<int> fps) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (power_status_helper_)
    power_status_helper_->SetAverageFrameRate(fps);

  last_reported_fps_ = fps;
  UpdateSmoothnessHelper();
}

void WebMediaPlayerImpl::OnAudioConfigChange(const AudioDecoderConfig& config) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::kReadyStateHaveNothing);

  const bool codec_change =
      pipeline_metadata_.audio_decoder_config.codec() != config.codec();
  const bool codec_profile_change =
      pipeline_metadata_.audio_decoder_config.profile() != config.profile();

  pipeline_metadata_.audio_decoder_config = config;

  if (observer_)
    observer_->OnMetadataChanged(pipeline_metadata_);

  if (codec_change) {
    media_metrics_provider_->SetHasAudio(
        pipeline_metadata_.audio_decoder_config.codec());
  }

  if (codec_change || codec_profile_change)
    UpdateSecondaryProperties();
}

void WebMediaPlayerImpl::OnVideoConfigChange(const VideoDecoderConfig& config) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::kReadyStateHaveNothing);

  const bool codec_change =
      pipeline_metadata_.video_decoder_config.codec() != config.codec();
  const bool codec_profile_change =
      pipeline_metadata_.video_decoder_config.profile() != config.profile();

  pipeline_metadata_.video_decoder_config = config;

  if (observer_)
    observer_->OnMetadataChanged(pipeline_metadata_);

  if (codec_change) {
    media_metrics_provider_->SetHasVideo(
        pipeline_metadata_.video_decoder_config.codec());
  }

  if (codec_change || codec_profile_change)
    UpdateSecondaryProperties();

  if (video_decode_stats_reporter_ && codec_profile_change)
    CreateVideoDecodeStatsReporter();
}

void WebMediaPlayerImpl::OnVideoAverageKeyframeDistanceUpdate() {
  UpdateBackgroundVideoOptimizationState();
}

void WebMediaPlayerImpl::OnAudioDecoderChange(const AudioDecoderInfo& info) {
  media_metrics_provider_->SetAudioPipelineInfo(info);
  if (info.decoder_type == audio_decoder_type_)
    return;

  audio_decoder_type_ = info.decoder_type;

  // If there's no current reporter, there's nothing to be done.
  if (!watch_time_reporter_)
    return;

  UpdateSecondaryProperties();
}

void WebMediaPlayerImpl::OnVideoDecoderChange(const VideoDecoderInfo& info) {
  media_metrics_provider_->SetVideoPipelineInfo(info);
  if (info.decoder_type == video_decoder_type_)
    return;

  video_decoder_type_ = info.decoder_type;

  // If there's no current reporter, there's nothing to be done.
  if (!watch_time_reporter_)
    return;

  UpdateSecondaryProperties();
}

void WebMediaPlayerImpl::OnFrameHidden() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Backgrounding a video requires a user gesture to resume playback.
  if (IsHidden())
    video_locked_when_paused_when_hidden_ = true;

  if (watch_time_reporter_)
    watch_time_reporter_->OnHidden();

  if (video_decode_stats_reporter_)
    video_decode_stats_reporter_->OnHidden();

  UpdateBackgroundVideoOptimizationState();
  UpdatePlayState();

  // Schedule suspended playing media to be paused if the user doesn't come back
  // to it within some timeout period to avoid any autoplay surprises.
  ScheduleIdlePauseTimer();

  // Notify the compositor of our page visibility status.
  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameCompositor::SetIsPageVisible,
                     base::Unretained(compositor_.get()), !IsHidden()));
}

void WebMediaPlayerImpl::OnFrameClosed() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnFrameShown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  background_pause_timer_.Stop();

  // Foreground videos don't require user gesture to continue playback.
  video_locked_when_paused_when_hidden_ = false;

  if (watch_time_reporter_)
    watch_time_reporter_->OnShown();

  if (video_decode_stats_reporter_)
    video_decode_stats_reporter_->OnShown();

  // Notify the compositor of our page visibility status.
  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameCompositor::SetIsPageVisible,
                     base::Unretained(compositor_.get()), !IsHidden()));

  UpdateBackgroundVideoOptimizationState();

  if (paused_when_hidden_) {
    paused_when_hidden_ = false;
    client_->ResumePlayback();  // Calls UpdatePlayState() so return afterwards.
    return;
  }

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnIdleTimeout() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // This should never be called when stale state testing overrides are used.
  DCHECK(!stale_state_override_for_testing_.has_value());

  // If we are attempting preroll, clear the stale flag.
  if (IsPrerollAttemptNeeded()) {
    delegate_->ClearStaleFlag(delegate_id_);
    return;
  }

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnVolumeMultiplierUpdate(double multiplier) {
  volume_multiplier_ = multiplier;
  SetVolume(volume_);
}

void WebMediaPlayerImpl::OnBecamePersistentVideo(bool value) {
  client_->OnBecamePersistentVideo(value);
  overlay_info_.is_persistent_video = value;
  MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::OnPowerExperimentState(bool state) {
  if (power_status_helper_)
    power_status_helper_->UpdatePowerExperimentState(state);
}

void WebMediaPlayerImpl::ScheduleRestart() {
  // TODO(watk): All restart logic should be moved into PipelineController.
  if (pipeline_controller_->IsPipelineRunning() &&
      !pipeline_controller_->IsPipelineSuspended()) {
    pending_suspend_resume_cycle_ = true;
    UpdatePlayState();
  }
}

void WebMediaPlayerImpl::RequestRemotePlaybackDisabled(bool disabled) {
  if (observer_)
    observer_->OnRemotePlaybackDisabled(disabled);
}

#if defined(OS_ANDROID)
void WebMediaPlayerImpl::FlingingStarted() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!disable_pipeline_auto_suspend_);
  disable_pipeline_auto_suspend_ = true;

  is_flinging_ = true;

  // Capabilities reporting should only be performed for local playbacks.
  video_decode_stats_reporter_.reset();

  // Requests to restart media pipeline. A flinging renderer will be created via
  // the |renderer_factory_selector_|.
  ScheduleRestart();
}

void WebMediaPlayerImpl::FlingingStopped() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(disable_pipeline_auto_suspend_);
  disable_pipeline_auto_suspend_ = false;

  is_flinging_ = false;

  CreateVideoDecodeStatsReporter();

  ScheduleRestart();
}

void WebMediaPlayerImpl::OnRemotePlayStateChange(MediaStatus::State state) {
  DCHECK(is_flinging_);
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (state == MediaStatus::State::PLAYING && Paused()) {
    DVLOG(1) << __func__ << " requesting PLAY.";
    client_->ResumePlayback();
  } else if (state == MediaStatus::State::PAUSED && !Paused()) {
    DVLOG(1) << __func__ << " requesting PAUSE.";
    client_->PausePlayback();
  }
}
#endif  // defined(OS_ANDROID)

void WebMediaPlayerImpl::SetPoster(const blink::WebURL& poster) {
  has_poster_ = !poster.IsEmpty();
}

void WebMediaPlayerImpl::DataSourceInitialized(bool success) {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (observer_ && mb_data_source_)
    observer_->OnDataSourceInitialized(mb_data_source_->GetUrlAfterRedirects());

  if (!success) {
    SetNetworkState(WebMediaPlayer::kNetworkStateFormatError);
    media_metrics_provider_->OnError(PIPELINE_ERROR_NETWORK);

    // Not really necessary, since the pipeline was never started, but it at
    // least this makes sure that the error handling code is in sync.
    UpdatePlayState();

    return;
  }

  // No point in preloading data as we'll probably just throw it away anyways.
  if (IsStreaming() && preload_ > MultibufferDataSource::METADATA &&
      mb_data_source_) {
    mb_data_source_->SetPreload(MultibufferDataSource::METADATA);
  }

  StartPipeline();
}

void WebMediaPlayerImpl::OnDataSourceRedirected() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(mb_data_source_);

  if (WouldTaintOrigin()) {
    audio_source_provider_->TaintOrigin();
  }
}

void WebMediaPlayerImpl::NotifyDownloading(bool is_downloading) {
  DVLOG(1) << __func__ << "(" << is_downloading << ")";
  if (!is_downloading && network_state_ == WebMediaPlayer::kNetworkStateLoading)
    SetNetworkState(WebMediaPlayer::kNetworkStateIdle);
  else if (is_downloading &&
           network_state_ == WebMediaPlayer::kNetworkStateIdle)
    SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  if (ready_state_ == ReadyState::kReadyStateHaveFutureData && !is_downloading)
    SetReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);
}

void WebMediaPlayerImpl::OnOverlayRoutingToken(
    const base::UnguessableToken& token) {
  DCHECK(overlay_mode_ == OverlayMode::kUseAndroidOverlay);
  // TODO(liberato): |token| should already be a RoutingToken.
  overlay_routing_token_is_pending_ = false;
  overlay_routing_token_ = OverlayInfo::RoutingToken(token);
  MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::OnOverlayInfoRequested(
    bool decoder_requires_restart_for_overlay,
    ProvideOverlayInfoCB provide_overlay_info_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // If we get a non-null cb, a decoder is initializing and requires overlay
  // info. If we get a null cb, a previously initialized decoder is
  // unregistering for overlay info updates.
  if (!provide_overlay_info_cb) {
    decoder_requires_restart_for_overlay_ = false;
    provide_overlay_info_cb_.Reset();
    return;
  }

  // If |decoder_requires_restart_for_overlay| is true, we must restart the
  // pipeline for fullscreen transitions. The decoder is unable to switch
  // surfaces otherwise. If false, we simply need to tell the decoder about the
  // new surface and it will handle things seamlessly.
  // For encrypted video we pretend that the decoder doesn't require a restart
  // because it needs an overlay all the time anyway. We'll switch into
  // |always_enable_overlays_| mode below.
  decoder_requires_restart_for_overlay_ =
      (overlay_mode_ == OverlayMode::kUseAndroidOverlay && is_encrypted_)
          ? false
          : decoder_requires_restart_for_overlay;
  provide_overlay_info_cb_ = std::move(provide_overlay_info_cb);

  // If the decoder doesn't require restarts for surface transitions, and we're
  // using AndroidOverlay mode, we can always enable the overlay and the decoder
  // can choose whether or not to use it. Otherwise, we'll restart the decoder
  // and enable the overlay on fullscreen transitions.
  if (overlay_mode_ == OverlayMode::kUseAndroidOverlay &&
      !decoder_requires_restart_for_overlay_) {
    always_enable_overlays_ = true;
    if (!overlay_enabled_)
      EnableOverlay();
  }

  // Send the overlay info if we already have it. If not, it will be sent later.
  MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::MaybeSendOverlayInfoToDecoder() {
  // If the decoder didn't request overlay info, then don't send it.
  if (!provide_overlay_info_cb_)
    return;

  // We should send the overlay info as long as we know it.  This includes the
  // case where |!overlay_enabled_|, since we want to tell the decoder to avoid
  // using overlays.  Assuming that the decoder has requested info, the only
  // case in which we don't want to send something is if we've requested the
  // info but not received it yet.  Then, we should wait until we do.
  //
  // Initialization requires this; AVDA should start with enough info to make an
  // overlay, so that (pre-M) the initial codec is created with the right output
  // surface; it can't switch later.
  if (overlay_mode_ == OverlayMode::kUseAndroidOverlay) {
    if (overlay_routing_token_is_pending_)
      return;

    overlay_info_.routing_token = overlay_routing_token_;
  }

  // If restart is required, the callback is one-shot only.
  if (decoder_requires_restart_for_overlay_) {
    std::move(provide_overlay_info_cb_).Run(overlay_info_);
  } else {
    provide_overlay_info_cb_.Run(overlay_info_);
  }
}

std::unique_ptr<Renderer> WebMediaPlayerImpl::CreateRenderer(
    base::Optional<RendererFactoryType> factory_type) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Make sure that overlays are enabled if they're always allowed.
  if (always_enable_overlays_)
    EnableOverlay();

  RequestOverlayInfoCB request_overlay_info_cb;
#if defined(OS_ANDROID)
  request_overlay_info_cb = BindToCurrentLoop(base::BindRepeating(
      &WebMediaPlayerImpl::OnOverlayInfoRequested, weak_this_));
#endif

  if (factory_type) {
    DVLOG(1) << __func__
             << ": factory_type=" << static_cast<int>(factory_type.value());
    renderer_factory_selector_->SetBaseFactoryType(factory_type.value());
  }

  reported_renderer_type_ = renderer_factory_selector_->GetCurrentFactoryType();

  return renderer_factory_selector_->GetCurrentFactory()->CreateRenderer(
      media_task_runner_, worker_task_runner_, audio_source_provider_.get(),
      compositor_.get(), std::move(request_overlay_info_cb),
      client_->TargetColorSpace());
}

void WebMediaPlayerImpl::StartPipeline() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb =
      BindToCurrentLoop(base::BindRepeating(
          &WebMediaPlayerImpl::OnEncryptedMediaInitData, weak_this_));

  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameCompositor::SetOnNewProcessedFrameCallback,
                     base::Unretained(compositor_.get()),
                     BindToCurrentLoop(base::BindOnce(
                         &WebMediaPlayerImpl::OnFirstFrame, weak_this_))));

#if defined(OS_ANDROID)
  if (demuxer_found_hls_ ||
      renderer_factory_selector_->GetCurrentFactory()
              ->GetRequiredMediaResourceType() == MediaResource::Type::URL) {
    // MediaPlayerRendererClientFactory is the only factory that a uses
    // MediaResource::Type::URL for the moment.
    using_media_player_renderer_ = true;

    // MediaPlayerRenderer does not provide pipeline stats, so nuke capabilities
    // reporter.
    video_decode_stats_reporter_.reset();

    SetDemuxer(std::make_unique<MediaUrlDemuxer>(
        media_task_runner_, loaded_url_,
        frame_->GetDocument().SiteForCookies().RepresentativeUrl(),
        frame_->GetDocument().TopFrameOrigin(),
        allow_media_player_renderer_credentials_, demuxer_found_hls_));
    pipeline_controller_->Start(Pipeline::StartType::kNormal, demuxer_.get(),
                                this, false, false);
    return;
  }
#endif  // defined(OS_ANDROID)

  // Figure out which demuxer to use.
  if (demuxer_override_) {
    DCHECK(!chunk_demuxer_);

    SetDemuxer(std::move(demuxer_override_));
    // TODO(https://crbug.com/1076267): Should everything else after this block
    // run in the demuxer override case?
  } else if (load_type_ != kLoadTypeMediaSource) {
    DCHECK(!chunk_demuxer_);
    DCHECK(data_source_);

#if BUILDFLAG(ENABLE_FFMPEG)
    Demuxer::MediaTracksUpdatedCB media_tracks_updated_cb =
        BindToCurrentLoop(base::BindRepeating(
            &WebMediaPlayerImpl::OnFFmpegMediaTracksUpdated, weak_this_));

    SetDemuxer(std::make_unique<FFmpegDemuxer>(
        media_task_runner_, data_source_.get(), encrypted_media_init_data_cb,
        media_tracks_updated_cb, media_log_.get(), IsLocalFile(loaded_url_)));
#else
    OnError(PipelineStatus::DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
#endif
  } else {
    DCHECK(!chunk_demuxer_);
    DCHECK(!data_source_);

    chunk_demuxer_ =
        new ChunkDemuxer(BindToCurrentLoop(base::BindOnce(
                             &WebMediaPlayerImpl::OnDemuxerOpened, weak_this_)),
                         BindToCurrentLoop(base::BindRepeating(
                             &WebMediaPlayerImpl::OnProgress, weak_this_)),
                         encrypted_media_init_data_cb, media_log_.get());
    SetDemuxer(std::unique_ptr<Demuxer>(chunk_demuxer_));

    if (base::FeatureList::IsEnabled(kMemoryPressureBasedSourceBufferGC)) {
      // base::Unretained is safe because |this| owns memory_pressure_listener_.
      memory_pressure_listener_ =
          std::make_unique<base::MemoryPressureListener>(
              FROM_HERE,
              base::BindRepeating(&WebMediaPlayerImpl::OnMemoryPressure,
                                  base::Unretained(this)));
    }
  }

  // If possible attempt to avoid decoder spool up until playback starts.
  Pipeline::StartType start_type = Pipeline::StartType::kNormal;
  if (!chunk_demuxer_ && preload_ == MultibufferDataSource::METADATA &&
      !client_->CouldPlayIfEnoughData() && !IsStreaming()) {
    start_type =
        (has_poster_ || base::FeatureList::IsEnabled(kPreloadMetadataLazyLoad))
            ? Pipeline::StartType::kSuspendAfterMetadata
            : Pipeline::StartType::kSuspendAfterMetadataForAudioOnly;
    attempting_suspended_start_ = true;
  }

  // TODO(sandersd): FileSystem objects may also be non-static, but due to our
  // caching layer such situations are broken already. http://crbug.com/593159
  const bool is_static = !chunk_demuxer_;

  // ... and we're ready to go!
  // TODO(sandersd): On Android, defer Start() if the tab is not visible.
  seeking_ = true;
  pipeline_controller_->Start(start_type, demuxer_.get(), this, IsStreaming(),
                              is_static);
}

void WebMediaPlayerImpl::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DVLOG(1) << __func__ << "(" << state << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  client_->NetworkStateChanged();
}

void WebMediaPlayerImpl::SetReadyState(WebMediaPlayer::ReadyState state) {
  DVLOG(1) << __func__ << "(" << state << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (state == WebMediaPlayer::kReadyStateHaveEnoughData && data_source_ &&
      data_source_->AssumeFullyBuffered() &&
      network_state_ == WebMediaPlayer::kNetworkStateLoading) {
    SetNetworkState(WebMediaPlayer::kNetworkStateLoaded);
  }

  ready_state_ = state;
  highest_ready_state_ = std::max(highest_ready_state_, ready_state_);

  // Always notify to ensure client has the latest value.
  client_->ReadyStateChanged();
}

scoped_refptr<blink::WebAudioSourceProviderImpl>
WebMediaPlayerImpl::GetAudioSourceProvider() {
  return audio_source_provider_;
}

scoped_refptr<VideoFrame> WebMediaPlayerImpl::GetCurrentFrameFromCompositor()
    const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl::GetCurrentFrameFromCompositor");

  // We can't copy from protected frames.
  if (cdm_context_ref_)
    return nullptr;

  // Can be null.
  scoped_refptr<VideoFrame> video_frame =
      compositor_->GetCurrentFrameOnAnyThread();

  // base::Unretained is safe here because |compositor_| is destroyed on
  // |vfc_task_runner_|. The destruction is queued from |this|' destructor,
  // which also runs on |main_task_runner_|, which makes it impossible for
  // UpdateCurrentFrameIfStale() to be queued after |compositor_|'s dtor.
  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameCompositor::UpdateCurrentFrameIfStale,
                     base::Unretained(compositor_.get()),
                     VideoFrameCompositor::UpdateType::kNormal));

  return video_frame;
}

void WebMediaPlayerImpl::UpdatePlayState() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  bool can_auto_suspend = !disable_pipeline_auto_suspend_;
  // For streaming videos, we only allow suspending at the very beginning of the
  // video, and only if we know the length of the video. (If we don't know
  // the length, it might be a dynamically generated video, and suspending
  // will not work at all.)
  if (IsStreaming()) {
    bool at_beginning =
        ready_state_ == WebMediaPlayer::kReadyStateHaveNothing ||
        CurrentTime() == 0.0;
    if (!at_beginning || GetPipelineMediaDuration() == kInfiniteDuration)
      can_auto_suspend = false;
  }

  bool is_suspended = pipeline_controller_->IsSuspended();
  bool is_backgrounded = IsBackgroundSuspendEnabled(this) && IsHidden();
  PlayState state = UpdatePlayState_ComputePlayState(
      is_flinging_, can_auto_suspend, is_suspended, is_backgrounded);
  SetDelegateState(state.delegate_state, state.is_idle);
  SetMemoryReportingState(state.is_memory_reporting_enabled);
  SetSuspendState(state.is_suspended || pending_suspend_resume_cycle_);
  if (power_status_helper_) {
    // Make sure that we're in something like steady-state before recording.
    power_status_helper_->SetIsPlaying(
        !paused_ && !seeking_ && !IsHidden() && !state.is_suspended &&
        ready_state_ == kReadyStateHaveEnoughData);
  }
  UpdateSmoothnessHelper();
}

void WebMediaPlayerImpl::OnTimeUpdate() {
  // When seeking the current time can go beyond the duration so we should
  // cap the current time at the duration.
  base::TimeDelta duration = GetPipelineMediaDuration();
  base::TimeDelta current_time = GetCurrentTimeInternal();
  if (current_time > duration)
    current_time = duration;

  const double effective_playback_rate =
      paused_ || ready_state_ < kReadyStateHaveFutureData ? 0.0
                                                          : playback_rate_;

  media_session::MediaPosition new_position(effective_playback_rate, duration,
                                            current_time);

  if (!MediaPositionNeedsUpdate(media_position_state_, new_position))
    return;

  DVLOG(2) << __func__ << "(" << new_position.ToString() << ")";
  media_position_state_ = new_position;
  client_->DidPlayerMediaPositionStateChange(effective_playback_rate, duration,
                                             current_time);
}

void WebMediaPlayerImpl::SetDelegateState(DelegateState new_state,
                                          bool is_idle) {
  DCHECK(delegate_);
  DVLOG(2) << __func__ << "(" << static_cast<int>(new_state) << ", " << is_idle
           << ")";

  // Prevent duplicate delegate calls.
  // TODO(sandersd): Move this deduplication into the delegate itself.
  if (delegate_state_ == new_state)
    return;
  delegate_state_ = new_state;

  switch (new_state) {
    case DelegateState::GONE:
      delegate_->PlayerGone(delegate_id_);
      break;
    case DelegateState::PLAYING: {
      if (HasVideo())
        client_->DidPlayerSizeChange(NaturalSize());
      client_->DidPlayerStartPlaying();
      delegate_->DidPlay(delegate_id_);
      break;
    }
    case DelegateState::PAUSED:
      client_->DidPlayerPaused(ended_);
      delegate_->DidPause(delegate_id_, ended_);
      break;
  }

  delegate_->SetIdle(delegate_id_, is_idle);
}

void WebMediaPlayerImpl::SetMemoryReportingState(
    bool is_memory_reporting_enabled) {
  if (memory_usage_reporting_timer_.IsRunning() ==
      is_memory_reporting_enabled) {
    return;
  }

  if (is_memory_reporting_enabled) {
    memory_usage_reporting_timer_.Start(FROM_HERE,
                                        base::TimeDelta::FromSeconds(2), this,
                                        &WebMediaPlayerImpl::ReportMemoryUsage);
  } else {
    memory_usage_reporting_timer_.Stop();
    ReportMemoryUsage();
  }
}

void WebMediaPlayerImpl::SetSuspendState(bool is_suspended) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __func__ << "(" << is_suspended << ")";

  // Do not change the state after an error has occurred.
  // TODO(sandersd): Update PipelineController to remove the need for this.
  if (IsNetworkStateError(network_state_))
    return;

  if (is_suspended) {
    // If we were not resumed for long enough to satisfy the preroll attempt,
    // reset the clock.
    if (!preroll_attempt_pending_ && IsPrerollAttemptNeeded()) {
      preroll_attempt_pending_ = true;
      preroll_attempt_start_time_ = base::TimeTicks();
    }
    pipeline_controller_->Suspend();
  } else {
    // When resuming, start the preroll attempt clock.
    if (preroll_attempt_pending_) {
      preroll_attempt_pending_ = false;
      preroll_attempt_start_time_ = tick_clock_->NowTicks();
    }
    pipeline_controller_->Resume();
  }
}

WebMediaPlayerImpl::PlayState
WebMediaPlayerImpl::UpdatePlayState_ComputePlayState(bool is_flinging,
                                                     bool can_auto_suspend,
                                                     bool is_suspended,
                                                     bool is_backgrounded) {
  PlayState result;

  bool must_suspend = delegate_->IsFrameClosed();
  bool is_stale = delegate_->IsStale(delegate_id_);

  if (stale_state_override_for_testing_.has_value() &&
      ready_state_ >= stale_state_override_for_testing_.value()) {
    is_stale = true;
  }

  // This includes both data source (before pipeline startup) and pipeline
  // errors.
  bool has_error = IsNetworkStateError(network_state_);

  // Note: Even though we get play/pause signals at kReadyStateHaveMetadata, we
  // must attempt to preroll until kReadyStateHaveFutureData so that the
  // canplaythrough event will be fired to the page (which may be waiting).
  bool have_future_data =
      highest_ready_state_ >= WebMediaPlayer::kReadyStateHaveFutureData;

  // Background suspend is only enabled for paused players.
  // In the case of players with audio the session should be kept.
  bool background_suspended =
      can_auto_suspend && is_backgrounded && paused_ && have_future_data;

  // Idle suspension is allowed prior to kReadyStateHaveMetadata since there
  // exist mechanisms to exit the idle state when the player is capable of
  // reaching the kReadyStateHaveMetadata state; see didLoadingProgress().
  //
  // TODO(sandersd): Make the delegate suspend idle players immediately when
  // hidden.
  bool idle_suspended = can_auto_suspend && is_stale && paused_ && !seeking_ &&
                        !overlay_enabled_ && !needs_first_frame_;

  // If we're already suspended, see if we can wait for user interaction. Prior
  // to kReadyStateHaveMetadata, we require |is_stale| to remain suspended.
  // |is_stale| will be cleared when we receive data which may take us to
  // kReadyStateHaveMetadata.
  bool can_stay_suspended = (is_stale || have_future_data) && is_suspended &&
                            paused_ && !seeking_ && !needs_first_frame_;

  // Combined suspend state.
  result.is_suspended = must_suspend || idle_suspended ||
                        background_suspended || can_stay_suspended;

  DVLOG(3) << __func__ << ": must_suspend=" << must_suspend
           << ", idle_suspended=" << idle_suspended
           << ", background_suspended=" << background_suspended
           << ", can_stay_suspended=" << can_stay_suspended
           << ", is_stale=" << is_stale
           << ", have_future_data=" << have_future_data
           << ", paused_=" << paused_ << ", seeking_=" << seeking_;

  // We do not treat |playback_rate_| == 0 as paused. For the media session,
  // being paused implies displaying a play button, which is incorrect in this
  // case. For memory usage reporting, we just use the same definition (but we
  // don't have to).
  //
  // Similarly, we don't consider |ended_| to be paused. Blink will immediately
  // call pause() or seek(), so |ended_| should not affect the computation.
  // Despite that, |ended_| does result in a separate paused state, to simplfy
  // the contract for SetDelegateState().
  //
  // |has_remote_controls| indicates if the player can be controlled outside the
  // page (e.g. via the notification controls or by audio focus events). Idle
  // suspension does not destroy the media session, because we expect that the
  // notification controls (and audio focus) remain. With some exceptions for
  // background videos, the player only needs to have audio to have controls
  // (requires |have_current_data|).
  //
  // |alive| indicates if the player should be present (not |GONE|) to the
  // delegate, either paused or playing. The following must be true for the
  // player:
  //   - |have_current_data|, since playback can't begin before that point, we
  //     need to know whether we are paused to correctly configure the session,
  //     and also because the tracks and duration are passed to DidPlay(),
  //   - |is_flinging| is false (RemotePlayback is not handled by the delegate)
  //   - |has_error| is false as player should have no errors,
  //   - |background_suspended| is false, otherwise |has_remote_controls| must
  //     be true.
  //
  // TODO(sandersd): If Blink told us the paused state sooner, we could detect
  // if the remote controls are available sooner.

  // Background videos with audio don't have remote controls if background
  // suspend is enabled and resuming background videos is not (original Android
  // behavior).
  bool backgrounded_video_has_no_remote_controls =
      IsBackgroundSuspendEnabled(this) && !IsResumeBackgroundVideosEnabled() &&
      is_backgrounded && HasVideo();
  bool have_current_data = highest_ready_state_ >= kReadyStateHaveCurrentData;
  bool can_play = !has_error && have_current_data;
  bool has_remote_controls =
      HasAudio() && !backgrounded_video_has_no_remote_controls;
  bool alive = can_play && !is_flinging && !must_suspend &&
               (!background_suspended || has_remote_controls);
  if (!alive) {
    // Do not mark players as idle when flinging.
    result.delegate_state = DelegateState::GONE;
    result.is_idle = delegate_->IsIdle(delegate_id_) && !is_flinging;
  } else if (paused_) {
    // TODO(sandersd): Is it possible to have a suspended session, be ended,
    // and not be paused? If so we should be in a PLAYING state.
    result.delegate_state = DelegateState::PAUSED;
    result.is_idle = !seeking_;
  } else {
    result.delegate_state = DelegateState::PLAYING;
    result.is_idle = false;
  }

  // It's not critical if some cases where memory usage can change are missed,
  // since media memory changes are usually gradual.
  result.is_memory_reporting_enabled = !has_error && can_play && !is_flinging &&
                                       !result.is_suspended &&
                                       (!paused_ || seeking_);

  return result;
}

void WebMediaPlayerImpl::SetDemuxer(std::unique_ptr<Demuxer> demuxer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!demuxer_);
  DCHECK(!media_thread_mem_dumper_);
  DCHECK(demuxer);

  demuxer_ = std::move(demuxer);

  // base::Unretained() is safe here. |demuxer_| is destroyed on the main
  // thread, but before doing it ~WebMediaPlayerImpl() posts a media thread task
  // that deletes media_thread_mem_dumper_ and  waits for it to finish.
  media_thread_mem_dumper_ = std::make_unique<MemoryDumpProviderProxy>(
      "WebMediaPlayer_MediaThread", media_task_runner_,
      base::BindRepeating(&WebMediaPlayerImpl::OnMediaThreadMemoryDump,
                          media_log_->id(), base::Unretained(demuxer_.get())));
}

void WebMediaPlayerImpl::ReportMemoryUsage() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // About base::Unretained() usage below: We destroy |demuxer_| on the main
  // thread.  Before that, however, ~WebMediaPlayerImpl() posts a task to the
  // media thread and waits for it to finish.  Hence, the GetMemoryUsage() task
  // posted here must finish earlier.
  //
  // The exception to the above is when OnError() has been called. If we're in
  // the error state we've already shut down the pipeline and can't rely on it
  // to cycle the media thread before we destroy |demuxer_|. In this case skip
  // collection of the demuxer memory stats.
  if (demuxer_ && !IsNetworkStateError(network_state_)) {
    base::PostTaskAndReplyWithResult(
        media_task_runner_.get(), FROM_HERE,
        base::BindOnce(&Demuxer::GetMemoryUsage,
                       base::Unretained(demuxer_.get())),
        base::BindOnce(&WebMediaPlayerImpl::FinishMemoryUsageReport,
                       weak_this_));
  } else {
    FinishMemoryUsageReport(0);
  }
}

void WebMediaPlayerImpl::FinishMemoryUsageReport(int64_t demuxer_memory_usage) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  const PipelineStatistics stats = GetPipelineStatistics();
  const int64_t data_source_memory_usage =
      data_source_ ? data_source_->GetMemoryUsage() : 0;

  // If we have video and no video memory usage and we've rendered the first
  // frame, assume the VideoFrameCompositor is holding onto the last frame after
  // we've suspended the pipeline; which thus reports zero memory usage from the
  // video renderer.
  //
  // Technically this should use the coded size, but that requires us to hop to
  // the compositor to get and byte-perfect accuracy isn't important here.
  const int64_t video_memory_usage =
      stats.video_memory_usage +
      ((pipeline_metadata_.has_video && !stats.video_memory_usage &&
        has_first_frame_)
           ? VideoFrame::AllocationSize(PIXEL_FORMAT_I420,
                                        pipeline_metadata_.natural_size)
           : 0);

  const int64_t current_memory_usage =
      stats.audio_memory_usage + video_memory_usage + data_source_memory_usage +
      demuxer_memory_usage;

  DVLOG(3) << "Memory Usage -- Total: " << current_memory_usage
           << " Audio: " << stats.audio_memory_usage
           << ", Video: " << video_memory_usage
           << ", DataSource: " << data_source_memory_usage
           << ", Demuxer: " << demuxer_memory_usage;

  const int64_t delta = current_memory_usage - last_reported_memory_usage_;
  last_reported_memory_usage_ = current_memory_usage;
  adjust_allocated_memory_cb_.Run(delta);
}

void WebMediaPlayerImpl::OnMainThreadMemoryDump(
    int32_t id,
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  const PipelineStatistics stats = GetPipelineStatistics();
  auto player_node_name =
      base::StringPrintf("media/webmediaplayer/player_0x%x", id);
  auto* player_node = pmd->CreateAllocatorDump(player_node_name);
  player_node->AddScalar(
      base::trace_event::MemoryAllocatorDump::kNameObjectCount,
      base::trace_event::MemoryAllocatorDump::kUnitsObjects, 1);

  if (args.level_of_detail !=
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND) {
    bool suspended = pipeline_controller_->IsPipelineSuspended();
    auto player_state =
        base::StringPrintf("Paused: %d Ended: %d ReadyState: %d Suspended: %d",
                           paused_, ended_, GetReadyState(), suspended);
    player_node->AddString("player_state", "", player_state);
  }

  CreateAllocation(pmd, id, "audio", stats.audio_memory_usage);
  CreateAllocation(pmd, id, "video", stats.video_memory_usage);

  if (data_source_)
    CreateAllocation(pmd, id, "data_source", data_source_->GetMemoryUsage());
}

// static
void WebMediaPlayerImpl::OnMediaThreadMemoryDump(
    int32_t id,
    Demuxer* demuxer,
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (!demuxer)
    return;

  CreateAllocation(pmd, id, "demuxer", demuxer->GetMemoryUsage());
}

void WebMediaPlayerImpl::ScheduleIdlePauseTimer() {
  // Only schedule the pause timer if we're not paused or paused but going to
  // resume when foregrounded, and are suspended and have audio.
  if ((paused_ && !paused_when_hidden_) ||
      !pipeline_controller_->IsSuspended() || !HasAudio()) {
    return;
  }

#if defined(OS_ANDROID)
  // Don't pause videos casted as part of RemotePlayback.
  if (is_flinging_)
    return;
#endif

  // Idle timeout chosen arbitrarily.
  background_pause_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(5),
                                client_,
                                &blink::WebMediaPlayerClient::ResumePlayback);
}

void WebMediaPlayerImpl::CreateWatchTimeReporter() {
  if (!HasVideo() && !HasAudio())
    return;

  // MediaPlayerRenderer does not know about tracks until playback starts.
  // Assume audio-only unless the natural size has been detected.
  bool has_video = pipeline_metadata_.has_video;
  if (using_media_player_renderer_) {
    has_video = !pipeline_metadata_.natural_size.IsEmpty();
  }

  // Create the watch time reporter and synchronize its initial state.
  watch_time_reporter_ = std::make_unique<blink::WatchTimeReporter>(
      mojom::PlaybackProperties::New(
          pipeline_metadata_.has_audio, has_video, false, false,
          !!chunk_demuxer_, is_encrypted_, embedded_media_experience_enabled_,
          mojom::MediaStreamType::kNone),
      pipeline_metadata_.natural_size,
      base::BindRepeating(&WebMediaPlayerImpl::GetCurrentTimeInternal,
                          base::Unretained(this)),
      base::BindRepeating(&WebMediaPlayerImpl::GetPipelineStatistics,
                          base::Unretained(this)),
      media_metrics_provider_.get(),
      frame_->GetTaskRunner(blink::TaskType::kInternalMedia));
  watch_time_reporter_->OnVolumeChange(volume_);
  watch_time_reporter_->OnDurationChanged(GetPipelineMediaDuration());

  if (delegate_->IsFrameHidden())
    watch_time_reporter_->OnHidden();
  else
    watch_time_reporter_->OnShown();

  if (client_->HasNativeControls())
    watch_time_reporter_->OnNativeControlsEnabled();
  else
    watch_time_reporter_->OnNativeControlsDisabled();

  switch (client_->GetDisplayType()) {
    case blink::DisplayType::kInline:
      watch_time_reporter_->OnDisplayTypeInline();
      break;
    case blink::DisplayType::kFullscreen:
      watch_time_reporter_->OnDisplayTypeFullscreen();
      break;
    case blink::DisplayType::kPictureInPicture:
      watch_time_reporter_->OnDisplayTypePictureInPicture();
      break;
  }

  UpdateSecondaryProperties();

  // If the WatchTimeReporter was recreated in the middle of playback, we want
  // to resume playback here too since we won't get another play() call. When
  // seeking, the seek completion will restart it if necessary.
  if (!paused_ && !seeking_)
    watch_time_reporter_->OnPlaying();
}

void WebMediaPlayerImpl::UpdateSecondaryProperties() {
  watch_time_reporter_->UpdateSecondaryProperties(
      mojom::SecondaryPlaybackProperties::New(
          pipeline_metadata_.audio_decoder_config.codec(),
          pipeline_metadata_.video_decoder_config.codec(),
          pipeline_metadata_.audio_decoder_config.profile(),
          pipeline_metadata_.video_decoder_config.profile(),
          audio_decoder_type_, video_decoder_type_,
          pipeline_metadata_.audio_decoder_config.encryption_scheme(),
          pipeline_metadata_.video_decoder_config.encryption_scheme(),
          pipeline_metadata_.natural_size));
}

bool WebMediaPlayerImpl::IsHidden() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return delegate_->IsFrameHidden() && !delegate_->IsFrameClosed();
}

bool WebMediaPlayerImpl::IsStreaming() const {
  return data_source_ && data_source_->IsStreaming();
}

bool WebMediaPlayerImpl::DoesOverlaySupportMetadata() const {
  return pipeline_metadata_.video_decoder_config.video_transformation() ==
         kNoTransformation;
}

void WebMediaPlayerImpl::UpdateRemotePlaybackCompatibility(bool is_compatible) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  client_->RemotePlaybackCompatibilityChanged(loaded_url_, is_compatible);
}

void WebMediaPlayerImpl::ForceStaleStateForTesting(ReadyState target_state) {
  stale_state_override_for_testing_.emplace(target_state);
  UpdatePlayState();
}

bool WebMediaPlayerImpl::IsSuspendedForTesting() {
  // This intentionally uses IsPipelineSuspended since we need to know when the
  // pipeline has reached the suspended state, not when it's in suspending.
  return pipeline_controller_->IsPipelineSuspended();
}

bool WebMediaPlayerImpl::DidLazyLoad() const {
  return did_lazy_load_;
}

void WebMediaPlayerImpl::OnBecameVisible() {
  have_enough_after_lazy_load_cb_.Cancel();
  needs_first_frame_ = !has_first_frame_;
  UpdatePlayState();
}

bool WebMediaPlayerImpl::IsOpaque() const {
  return opaque_;
}

int WebMediaPlayerImpl::GetDelegateId() {
  return delegate_id_;
}

base::Optional<viz::SurfaceId> WebMediaPlayerImpl::GetSurfaceId() {
  if (!surface_layer_for_video_enabled_)
    return base::nullopt;
  return bridge_->GetSurfaceId();
}

void WebMediaPlayerImpl::RequestVideoFrameCallback() {
  compositor_->SetOnFramePresentedCallback(BindToCurrentLoop(base::BindOnce(
      &WebMediaPlayerImpl::OnNewFramePresentedCallback, weak_this_)));
}

void WebMediaPlayerImpl::OnNewFramePresentedCallback() {
  client_->OnRequestVideoFrameCallback();
}

std::unique_ptr<blink::WebMediaPlayer::VideoFramePresentationMetadata>
WebMediaPlayerImpl::GetVideoFramePresentationMetadata() {
  return compositor_->GetLastPresentedFrameMetadata();
}

void WebMediaPlayerImpl::UpdateFrameIfStale() {
  // base::Unretained is safe here because |compositor_| is destroyed on
  // |vfc_task_runner_|. The destruction is queued from |this|' destructor,
  // which also runs on |main_task_runner_|, which makes it impossible for
  // UpdateCurrentFrameIfStale() to be queued after |compositor_|'s dtor.
  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameCompositor::UpdateCurrentFrameIfStale,
                     base::Unretained(compositor_.get()),
                     VideoFrameCompositor::UpdateType::kBypassClient));
}

base::WeakPtr<blink::WebMediaPlayer> WebMediaPlayerImpl::AsWeakPtr() {
  return weak_this_;
}

bool WebMediaPlayerImpl::ShouldPausePlaybackWhenHidden() const {
  // Audio only stream is allowed to play when in background.
  // TODO: We should check IsBackgroundOptimizationCandidate here. But we need
  // to move the logic of checking video frames out of that function.
  if (!HasVideo())
    return false;

  if (using_media_player_renderer_ &&
      pipeline_metadata_.natural_size.IsEmpty()) {
    return false;
  }

  if (!is_background_video_playback_enabled_)
    return true;

  // If suspending background video, pause any video that's not remoted or
  // not unlocked to play in the background.
  if (IsBackgroundSuspendEnabled(this)) {
#if defined(OS_ANDROID)
    if (is_flinging_)
      return false;
#endif

    return !HasAudio() || (IsResumeBackgroundVideosEnabled() &&
                           video_locked_when_paused_when_hidden_);
  }

  // Otherwise only pause if the optimization is on and it's a video-only
  // optimization candidate.
  return IsBackgroundVideoPauseOptimizationEnabled() && !HasAudio() &&
         IsBackgroundOptimizationCandidate() && !is_flinging_;
}

bool WebMediaPlayerImpl::ShouldDisableVideoWhenHidden() const {
  // This optimization is behind the flag on all platforms, only for non-mse
  // video. MSE video track switching on hide has gone through a field test.
  // TODO(tmathmeyer): Passing load_type_ won't be needed after src= field
  // testing is finished. see: http://crbug.com/709302
  if (!is_background_video_track_optimization_supported_)
    return false;

  // Disable video track only for players with audio that match the criteria for
  // being optimized.
  return HasAudio() && IsBackgroundOptimizationCandidate();
}

bool WebMediaPlayerImpl::IsBackgroundOptimizationCandidate() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Don't optimize Picture-in-Picture players.
  if (IsInPictureInPicture())
    return false;

#if defined(OS_ANDROID)
  // Don't optimize videos casted as part of RemotePlayback.
  if (is_flinging_)
    return false;
#endif

  // Don't optimize audio-only or streaming players.
  if (!HasVideo() || IsStreaming())
    return false;

  // Video-only players are always optimized (paused).
  // Don't check the keyframe distance and duration.
  if (!HasAudio() && HasVideo())
    return true;

  // Videos shorter than the maximum allowed keyframe distance can be optimized.
  base::TimeDelta duration = GetPipelineMediaDuration();

  constexpr base::TimeDelta kMaxKeyframeDistanceToDisableBackgroundVideo =
      base::TimeDelta::FromMilliseconds(
          kMaxKeyframeDistanceToDisableBackgroundVideoMs);
  if (duration < kMaxKeyframeDistanceToDisableBackgroundVideo)
    return true;

  // Otherwise, only optimize videos with shorter average keyframe distance.
  PipelineStatistics stats = GetPipelineStatistics();
  return stats.video_keyframe_distance_average <
         kMaxKeyframeDistanceToDisableBackgroundVideo;
}

void WebMediaPlayerImpl::UpdateBackgroundVideoOptimizationState() {
  if (IsHidden()) {
    if (ShouldPausePlaybackWhenHidden()) {
      PauseVideoIfNeeded();
    } else if (is_background_status_change_cancelled_) {
      // Only trigger updates when we don't have one already scheduled.
      update_background_status_cb_.Reset(
          base::BindOnce(&WebMediaPlayerImpl::DisableVideoTrackIfNeeded,
                         base::Unretained(this)));
      is_background_status_change_cancelled_ = false;

      // Defer disable track until we're sure the clip will be backgrounded for
      // some time. Resuming may take half a second, so frequent tab switches
      // will yield a poor user experience otherwise. http://crbug.com/709302
      // may also cause AV sync issues if disable/enable happens too fast.
      main_task_runner_->PostDelayedTask(
          FROM_HERE, update_background_status_cb_.callback(),
          base::TimeDelta::FromSeconds(10));
    }
  } else {
    update_background_status_cb_.Cancel();
    is_background_status_change_cancelled_ = true;
    EnableVideoTrackIfNeeded();
  }
}

void WebMediaPlayerImpl::PauseVideoIfNeeded() {
  DCHECK(IsHidden());

  // Don't pause video while the pipeline is stopped, resuming or seeking.
  // Also if the video is paused already.
  if (!pipeline_controller_->IsPipelineRunning() || is_pipeline_resuming_ ||
      seeking_ || paused_)
    return;

  // client_->PausePlayback() will get |paused_when_hidden_| set to
  // false and UpdatePlayState() called, so set the flag to true after and then
  // return.
  client_->PausePlayback();
  paused_when_hidden_ = true;
}

void WebMediaPlayerImpl::EnableVideoTrackIfNeeded() {
  // Don't change video track while the pipeline is stopped, resuming or
  // seeking.
  if (!pipeline_controller_->IsPipelineRunning() || is_pipeline_resuming_ ||
      seeking_)
    return;

  if (video_track_disabled_) {
    video_track_disabled_ = false;
    if (client_->HasSelectedVideoTrack()) {
      WebMediaPlayer::TrackId trackId = client_->GetSelectedVideoTrackId();
      SelectedVideoTrackChanged(&trackId);
    }
  }
}

void WebMediaPlayerImpl::DisableVideoTrackIfNeeded() {
  DCHECK(IsHidden());

  // Don't change video track while the pipeline is resuming or seeking.
  if (is_pipeline_resuming_ || seeking_)
    return;

  if (!video_track_disabled_ && ShouldDisableVideoWhenHidden()) {
    video_track_disabled_ = true;
    SelectedVideoTrackChanged(nullptr);
  }
}

void WebMediaPlayerImpl::SetPipelineStatisticsForTest(
    const PipelineStatistics& stats) {
  pipeline_statistics_for_test_ = base::make_optional(stats);
}

PipelineStatistics WebMediaPlayerImpl::GetPipelineStatistics() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_statistics_for_test_.value_or(
      pipeline_controller_->GetStatistics());
}

void WebMediaPlayerImpl::SetPipelineMediaDurationForTest(
    base::TimeDelta duration) {
  pipeline_media_duration_for_test_ = base::make_optional(duration);
}

base::TimeDelta WebMediaPlayerImpl::GetPipelineMediaDuration() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_media_duration_for_test_.value_or(
      pipeline_controller_->GetMediaDuration());
}

void WebMediaPlayerImpl::SwitchToRemoteRenderer(
    const std::string& remote_device_friendly_name) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  DCHECK(!is_remote_rendering_);
  is_remote_rendering_ = true;

  DCHECK(!disable_pipeline_auto_suspend_);
  disable_pipeline_auto_suspend_ = true;

  // Capabilities reporting should only be performed for local playbacks.
  video_decode_stats_reporter_.reset();

  // Requests to restart media pipeline. A remote renderer will be created via
  // the |renderer_factory_selector_|.
  ScheduleRestart();
  if (client_) {
    client_->MediaRemotingStarted(
        WebString::FromUTF8(remote_device_friendly_name));
  }
}

void WebMediaPlayerImpl::SwitchToLocalRenderer(
    MediaObserverClient::ReasonToSwitchToLocal reason) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!is_remote_rendering_)
    return;  // Is currently with local renderer.
  is_remote_rendering_ = false;

  DCHECK(disable_pipeline_auto_suspend_);
  disable_pipeline_auto_suspend_ = false;

  // Capabilities reporting may resume now that playback is local.
  CreateVideoDecodeStatsReporter();

  // Requests to restart media pipeline. A local renderer will be created via
  // the |renderer_factory_selector_|.
  ScheduleRestart();
  if (client_)
    client_->MediaRemotingStopped(GetSwitchToLocalMessage(reason));
}

void WebMediaPlayerImpl::RecordUnderflowDuration(base::TimeDelta duration) {
  DCHECK(data_source_ || chunk_demuxer_);

  if (data_source_)
    UMA_HISTOGRAM_TIMES("Media.UnderflowDuration2.SRC", duration);
  else
    UMA_HISTOGRAM_TIMES("Media.UnderflowDuration2.MSE", duration);

  if (is_encrypted_)
    UMA_HISTOGRAM_TIMES("Media.UnderflowDuration2.EME", duration);
}

#define UMA_HISTOGRAM_VIDEO_HEIGHT(name, sample) \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, 100, 10000, 50)

void WebMediaPlayerImpl::RecordVideoNaturalSize(const gfx::Size& natural_size) {
  // Always report video natural size to MediaLog.
  media_log_->AddEvent<MediaLogEvent::kVideoSizeChanged>(natural_size);
  media_log_->SetProperty<MediaLogProperty::kResolution>(natural_size);

  if (initial_video_height_recorded_)
    return;

  initial_video_height_recorded_ = true;

  int height = natural_size.height();

  if (load_type_ == kLoadTypeURL)
    UMA_HISTOGRAM_VIDEO_HEIGHT("Media.VideoHeight.Initial.SRC", height);
  else if (load_type_ == kLoadTypeMediaSource)
    UMA_HISTOGRAM_VIDEO_HEIGHT("Media.VideoHeight.Initial.MSE", height);

  if (is_encrypted_)
    UMA_HISTOGRAM_VIDEO_HEIGHT("Media.VideoHeight.Initial.EME", height);

  UMA_HISTOGRAM_VIDEO_HEIGHT("Media.VideoHeight.Initial.All", height);

  if (playback_events_recorder_)
    playback_events_recorder_->OnNaturalSizeChanged(natural_size);
}

#undef UMA_HISTOGRAM_VIDEO_HEIGHT

void WebMediaPlayerImpl::SetTickClockForTest(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
  buffered_data_source_host_->SetTickClockForTest(tick_clock);
}

void WebMediaPlayerImpl::OnFirstFrame(base::TimeTicks frame_time) {
  DCHECK(!load_start_time_.is_null());
  DCHECK(!skip_metrics_due_to_startup_suspend_);
  has_first_frame_ = true;
  needs_first_frame_ = false;
  const base::TimeDelta elapsed = frame_time - load_start_time_;
  media_metrics_provider_->SetTimeToFirstFrame(elapsed);
  RecordTimingUMA("Media.TimeToFirstFrame", elapsed);

  // Needed to signal HTMLVideoElement that it should remove the poster image.
  if (client_ && has_poster_)
    client_->Repaint();
}

void WebMediaPlayerImpl::RecordTimingUMA(const std::string& key,
                                         base::TimeDelta elapsed) {
  if (chunk_demuxer_)
    base::UmaHistogramMediumTimes(key + ".MSE", elapsed);
  else
    base::UmaHistogramMediumTimes(key + ".SRC", elapsed);
  if (is_encrypted_)
    base::UmaHistogramMediumTimes(key + ".EME", elapsed);
}

void WebMediaPlayerImpl::RecordEncryptionScheme(
    const std::string& stream_name,
    EncryptionScheme encryption_scheme) {
  DCHECK(stream_name == "Audio" || stream_name == "Video");

  // If the stream is not encrypted, don't record it.
  if (encryption_scheme == EncryptionScheme::kUnencrypted)
    return;

  base::UmaHistogramEnumeration(
      "Media.EME.EncryptionScheme.Initial." + stream_name,
      DetermineEncryptionSchemeUMAValue(encryption_scheme),
      EncryptionSchemeUMA::kCount);
}

bool WebMediaPlayerImpl::IsInPictureInPicture() const {
  DCHECK(client_);
  return client_->GetDisplayType() == blink::DisplayType::kPictureInPicture;
}

void WebMediaPlayerImpl::MaybeSetContainerNameForMetrics() {
  // Pipeline startup failed before even getting a demuxer setup.
  if (!demuxer_)
    return;

  // Container has already been set.
  if (highest_ready_state_ >= WebMediaPlayer::kReadyStateHaveMetadata)
    return;

  // Only report metrics for demuxers that provide container information.
  auto container = demuxer_->GetContainerForMetrics();
  if (container.has_value())
    media_metrics_provider_->SetContainerName(container.value());
}

void WebMediaPlayerImpl::MaybeUpdateBufferSizesForPlayback() {
  // Don't increase the MultiBufferDataSource buffer size until we've reached
  // kReadyStateHaveEnoughData. Otherwise we will unnecessarily slow down
  // playback startup -- it can instead be done for free after playback starts.
  if (!mb_data_source_ || highest_ready_state_ < kReadyStateHaveEnoughData)
    return;

  mb_data_source_->MediaPlaybackRateChanged(playback_rate_);
  if (!paused_)
    mb_data_source_->MediaIsPlaying();
}

void WebMediaPlayerImpl::OnSimpleWatchTimerTick() {
  RecordSimpleWatchTimeUMA(reported_renderer_type_);

  if (playback_events_recorder_)
    playback_events_recorder_->OnPipelineStatistics(GetPipelineStatistics());
}

GURL WebMediaPlayerImpl::GetSrcAfterRedirects() {
  return mb_data_source_ ? mb_data_source_->GetUrlAfterRedirects() : GURL();
}

void WebMediaPlayerImpl::UpdateSmoothnessHelper() {
  // If the experiment flag is off, then do nothing.
  if (!base::FeatureList::IsEnabled(kMediaLearningSmoothnessExperiment))
    return;

  // If we're paused, or if we can't get all the features, then clear any
  // smoothness helper and stop.  We'll try to create it later when we're
  // playing and have all the features.
  if (paused_ || !HasVideo() || pipeline_metadata_.natural_size.IsEmpty() ||
      !last_reported_fps_) {
    smoothness_helper_.reset();
    return;
  }

  // Fill in features.
  // NOTE: this is a very bad way to do this, since it memorizes the order of
  // features in the task.  However, it'll do for now.
  learning::FeatureVector features;
  features.push_back(
      learning::FeatureValue(pipeline_metadata_.video_decoder_config.codec()));
  features.push_back(learning::FeatureValue(
      pipeline_metadata_.video_decoder_config.profile()));
  features.push_back(
      learning::FeatureValue(pipeline_metadata_.natural_size.width()));
  features.push_back(learning::FeatureValue(*last_reported_fps_));

  // If we have a smoothness helper, and we're not changing the features, then
  // do nothing.  This prevents restarting the helper for no reason.
  if (smoothness_helper_ && features == smoothness_helper_->features())
    return;

  // Create or restart the smoothness helper with |features|.
  smoothness_helper_ = SmoothnessHelper::Create(
      GetLearningTaskController(learning::tasknames::kConsecutiveBadWindows),
      GetLearningTaskController(learning::tasknames::kConsecutiveNNRs),
      features, this);
}

std::unique_ptr<learning::LearningTaskController>
WebMediaPlayerImpl::GetLearningTaskController(const char* task_name) {
  // Get the LearningTaskController for |task_id|.
  learning::LearningTask task = learning::MediaLearningTasks::Get(task_name);
  DCHECK_EQ(task.name, task_name);

  mojo::Remote<media::learning::mojom::LearningTaskController> remote_ltc;
  media_metrics_provider_->AcquireLearningTaskController(
      task.name, remote_ltc.BindNewPipeAndPassReceiver());
  return std::make_unique<learning::MojoLearningTaskController>(
      task, std::move(remote_ltc));
}

bool WebMediaPlayerImpl::HasUnmutedAudio() const {
  // Pretend that the media has no audio if it never played unmuted. This is to
  // avoid any action related to audible media such as taking audio focus or
  // showing a media notification. To preserve a consistent experience, it does
  // not apply if a media was audible so the system states do not flicker
  // depending on whether the user muted the player.
  return HasAudio() && !client_->WasAlwaysMuted();
}

}  // namespace media
