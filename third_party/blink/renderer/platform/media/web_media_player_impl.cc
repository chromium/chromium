// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/web_media_player_impl.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/layers/video_layer.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/cdm_context.h"
#include "media/base/demuxer.h"
#include "media/base/encryption_scheme.h"
#include "media/base/key_systems.h"
#include "media/base/limits.h"
#include "media/base/media_content_type.h"
#include "media/base/media_log.h"
#include "media/base/media_player_logging_id.h"
#include "media/base/media_switches.h"
#include "media/base/media_url_demuxer.h"
#include "media/base/memory_dump_provider_proxy.h"
#include "media/base/remoting_constants.h"
#include "media/base/renderer.h"
#include "media/base/routing_token_callback.h"
#include "media/base/supported_types.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/memory_data_source.h"
#include "media/filters/pipeline_controller.h"
#include "media/learning/common/learning_task_controller.h"
#include "media/learning/common/media_learning_tasks.h"
#include "media/learning/mojo/public/cpp/mojo_learning_task_controller.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/data_url.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_job.h"
#include "services/device/public/mojom/battery_monitor.mojom-blink.h"
#include "third_party/blink/public/common/media/display_type.h"
#include "third_party/blink/public/common/media/watch_time_reporter.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_audio_source_provider_impl.h"
#include "third_party/blink/public/platform/web_content_decryption_module.h"
#include "third_party/blink/public/platform/web_encrypted_media_types.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_encrypted_media_client.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_media_source.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_surface_layer_bridge.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/public/web/modules/media/web_media_player_util.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/platform/media/buffered_data_source_host_impl.h"
#include "third_party/blink/renderer/platform/media/power_status_helper.h"
#include "third_party/blink/renderer/platform/media/url_index.h"
#include "third_party/blink/renderer/platform/media/video_decode_stats_reporter.h"
#include "third_party/blink/renderer/platform/media/web_content_decryption_module_impl.h"
#include "third_party/blink/renderer/platform/media/web_media_source_impl.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
#include "media/filters/hls_data_source_provider_impl.h"
#include "third_party/blink/renderer/platform/media/multi_buffer_data_source_factory.h"
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)

#if BUILDFLAG(IS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

namespace blink {
namespace {

enum SplitHistogramTypes {
  kTotal = 0x1 << 0,
  kPlaybackType = 0x1 << 1,
  kEncrypted = 0x1 << 2,
};

constexpr const char* GetHistogramName(SplitHistogramName type) {
  switch (type) {
    case SplitHistogramName::kTimeToMetadata:
      return "Media.TimeToMetadata";
    case SplitHistogramName::kTimeToPlayReady:
      return "Media.TimeToPlayReady";
    case SplitHistogramName::kUnderflowDuration2:
      return "Media.UnderflowDuration2";
    case SplitHistogramName::kVideoHeightInitial:
      return "Media.VideoHeight.Initial";
    case SplitHistogramName::kTimeToFirstFrame:
      return "Media.TimeToFirstFrame";
  }
  NOTREACHED();
}

namespace learning = ::media::learning;
using ::media::Demuxer;
using ::media::MediaLogEvent;
using ::media::MediaLogProperty;
using ::media::MediaTrack;

void SetSinkIdOnMediaThread(scoped_refptr<WebAudioSourceProviderImpl> sink,
                            const std::string& device_id,
                            media::OutputDeviceStatusCB callback) {
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
  return base::FeatureList::IsEnabled(media::kResumeBackgroundVideo);
}

bool IsNetworkStateError(WebMediaPlayer::NetworkState state) {
  bool result = state == WebMediaPlayer::kNetworkStateFormatError ||
                state == WebMediaPlayer::kNetworkStateNetworkError ||
                state == WebMediaPlayer::kNetworkStateDecodeError;
  DCHECK_EQ(state > WebMediaPlayer::kNetworkStateLoaded, result);
  return result;
}

gfx::Size GetRotatedVideoSize(media::VideoRotation rotation,
                              gfx::Size natural_size) {
  if (rotation == media::VIDEO_ROTATION_90 ||
      rotation == media::VIDEO_ROTATION_270)
    return gfx::Size(natural_size.height(), natural_size.width());
  return natural_size;
}

void RecordEncryptedEvent(bool encrypted_event_fired) {
  UMA_HISTOGRAM_BOOLEAN("Media.EME.EncryptedEvent", encrypted_event_fired);
}

// How much time must have elapsed since loading last progressed before we
// assume that the decoder will have had time to complete preroll.
constexpr base::TimeDelta kPrerollAttemptTimeout = base::Seconds(3);

// Maximum number, per-WMPI, of media logs of playback rate changes.
constexpr int kMaxNumPlaybackRateLogs = 10;

int GetSwitchToLocalMessage(
    media::MediaObserverClient::ReasonToSwitchToLocal reason) {
  switch (reason) {
    case media::MediaObserverClient::ReasonToSwitchToLocal::NORMAL:
      return IDS_MEDIA_REMOTING_STOP_TEXT;
    case media::MediaObserverClient::ReasonToSwitchToLocal::
        POOR_PLAYBACK_QUALITY:
      return IDS_MEDIA_REMOTING_STOP_BY_PLAYBACK_QUALITY_TEXT;
    case media::MediaObserverClient::ReasonToSwitchToLocal::PIPELINE_ERROR:
      return IDS_MEDIA_REMOTING_STOP_BY_ERROR_TEXT;
    case media::MediaObserverClient::ReasonToSwitchToLocal::ROUTE_TERMINATED:
      return WebMediaPlayerClient::kMediaRemotingStopNoText;
  }
  NOTREACHED_IN_MIGRATION();
  // To suppress compiler warning on Windows.
  return WebMediaPlayerClient::kMediaRemotingStopNoText;
}

// These values are persisted to UMA. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(crbug.com/825041): This should use EncryptionScheme when kUnencrypted
// removed.
enum class EncryptionSchemeUMA { kCenc = 0, kCbcs = 1, kCount };

EncryptionSchemeUMA DetermineEncryptionSchemeUMAValue(
    media::EncryptionScheme encryption_scheme) {
  if (encryption_scheme == media::EncryptionScheme::kCbcs)
    return EncryptionSchemeUMA::kCbcs;

  DCHECK_EQ(encryption_scheme, media::EncryptionScheme::kCenc);
  return EncryptionSchemeUMA::kCenc;
}

// Handles destruction of media::Renderer dependent components after the
// renderer has been destructed on the media thread.
void DestructionHelper(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> vfc_task_runner,
    std::unique_ptr<media::DemuxerManager> demuxer_manager,
    std::unique_ptr<VideoFrameCompositor> compositor,
    std::unique_ptr<media::CdmContextRef> cdm_context_1,
    std::unique_ptr<media::CdmContextRef> cdm_context_2,
    std::unique_ptr<media::MediaLog> media_log,
    std::unique_ptr<media::RendererFactorySelector> renderer_factory_selector,
    std::unique_ptr<WebSurfaceLayerBridge> bridge) {
  // We release `bridge` after pipeline stop to ensure layout tests receive
  // painted video frames before test harness exit.
  main_task_runner->DeleteSoon(FROM_HERE, std::move(bridge));

  // Since the media::Renderer is gone we can now destroy the compositor and
  // renderer factory selector.
  vfc_task_runner->DeleteSoon(FROM_HERE, std::move(compositor));
  main_task_runner->DeleteSoon(FROM_HERE, std::move(renderer_factory_selector));

  // ChunkDemuxer can be deleted on any thread, but other demuxers are bound to
  // the main thread and must be deleted there now that the renderer is gone.
  if (demuxer_manager &&
      demuxer_manager->GetDemuxerType() != media::DemuxerType::kChunkDemuxer) {
    main_task_runner->DeleteSoon(FROM_HERE, std::move(demuxer_manager));
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
  // `task_runner`.  On advice of task_scheduler OWNERS, MayBlock() is not
  // used because virtual memory overhead is not considered blocking I/O; and
  // CONTINUE_ON_SHUTDOWN is used to allow process termination to not block on
  // completing the task.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
             std::unique_ptr<media::DemuxerManager> demuxer_manager,
             std::unique_ptr<media::CdmContextRef> cdm_context_1,
             std::unique_ptr<media::CdmContextRef> cdm_context_2,
             std::unique_ptr<media::MediaLog> media_log) {
            demuxer_manager.reset();
            main_task_runner->DeleteSoon(FROM_HERE, std::move(cdm_context_1));
            main_task_runner->DeleteSoon(FROM_HERE, std::move(cdm_context_2));
            main_task_runner->DeleteSoon(FROM_HERE, std::move(media_log));
          },
          std::move(main_task_runner), std::move(demuxer_manager),
          std::move(cdm_context_1), std::move(cdm_context_2),
          std::move(media_log)));
}

std::string SanitizeUserStringProperty(WebString value) {
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

// Determine whether we should update MediaPosition in `delegate_`.
bool MediaPositionNeedsUpdate(
    const media_session::MediaPosition& old_position,
    const media_session::MediaPosition& new_position) {
  if (old_position.playback_rate() != new_position.playback_rate() ||
      old_position.duration() != new_position.duration() ||
      old_position.end_of_media() != new_position.end_of_media()) {
    return true;
  }

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
  return drift > base::Milliseconds(100);
}

// Returns whether the player uses AudioService. This is needed to enable
// AudioStreamMonitor (for audio indicator) when not using AudioService.
// TODO(crbug.com/1017943): Support other RendererTypes.
bool UsesAudioService(media::RendererType renderer_type) {
  return renderer_type != media::RendererType::kMediaFoundation;
}

}  // namespace

STATIC_ASSERT_ENUM(WebMediaPlayer::kCorsModeUnspecified,
                   UrlData::CORS_UNSPECIFIED);
STATIC_ASSERT_ENUM(WebMediaPlayer::kCorsModeAnonymous, UrlData::CORS_ANONYMOUS);
STATIC_ASSERT_ENUM(WebMediaPlayer::kCorsModeUseCredentials,
                   UrlData::CORS_USE_CREDENTIALS);

WebMediaPlayerImpl::WebMediaPlayerImpl(
    WebLocalFrame* frame,
    WebMediaPlayerClient* client,
    WebMediaPlayerEncryptedMediaClient* encrypted_client,
    WebMediaPlayerDelegate* delegate,
    std::unique_ptr<media::RendererFactorySelector> renderer_factory_selector,
    UrlIndex* url_index,
    std::unique_ptr<VideoFrameCompositor> compositor,
    std::unique_ptr<media::MediaLog> media_log,
    media::MediaPlayerLoggingID player_id,
    WebMediaPlayerBuilder::DeferLoadCB defer_load_cb,
    scoped_refptr<media::SwitchableAudioRendererSink> audio_renderer_sink,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    scoped_refptr<base::TaskRunner> worker_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner>
        video_frame_compositor_task_runner,
    WebContentDecryptionModule* initial_cdm,
    media::RequestRoutingTokenCallback request_routing_token_cb,
    base::WeakPtr<media::MediaObserver> media_observer,
    bool enable_instant_source_buffer_gc,
    bool embedded_media_experience_enabled,
    mojo::PendingRemote<media::mojom::MediaMetricsProvider> metrics_provider,
    CreateSurfaceLayerBridgeCB create_bridge_callback,
    scoped_refptr<viz::RasterContextProvider> raster_context_provider,
    bool use_surface_layer,
    bool is_background_suspend_enabled,
    bool is_background_video_playback_enabled,
    bool is_background_video_track_optimization_supported,
    std::unique_ptr<media::Demuxer> demuxer_override,
    scoped_refptr<ThreadSafeBrowserInterfaceBrokerProxy> remote_interfaces)
    : frame_(frame),
      main_task_runner_(frame->GetTaskRunner(TaskType::kMediaElementEvent)),
      media_task_runner_(std::move(media_task_runner)),
      worker_task_runner_(std::move(worker_task_runner)),
      media_player_id_(player_id),
      media_log_(std::move(media_log)),
      client_(client),
      encrypted_client_(encrypted_client),
      delegate_(delegate),
      delegate_has_audio_(HasUnmutedAudio()),
      defer_load_cb_(std::move(defer_load_cb)),
      isolate_(frame_->GetAgentGroupScheduler()->Isolate()),
      demuxer_manager_(std::make_unique<media::DemuxerManager>(
          this,
          media_task_runner_,
          media_log_.get(),
          frame_->GetDocument().SiteForCookies(),
          frame_->GetDocument().TopFrameOrigin(),
          frame_->GetDocument().StorageAccessApiStatus(),
          enable_instant_source_buffer_gc,
          std::move(demuxer_override))),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      url_index_(url_index),
      raster_context_provider_(std::move(raster_context_provider)),
      vfc_task_runner_(std::move(video_frame_compositor_task_runner)),
      compositor_(std::move(compositor)),
      renderer_factory_selector_(std::move(renderer_factory_selector)),
      observer_(std::move(media_observer)),
      embedded_media_experience_enabled_(embedded_media_experience_enabled),
      use_surface_layer_(use_surface_layer),
      create_bridge_callback_(std::move(create_bridge_callback)),
      request_routing_token_cb_(std::move(request_routing_token_cb)),
      media_metrics_provider_(std::move(metrics_provider)),
      is_background_suspend_enabled_(is_background_suspend_enabled),
      is_background_video_playback_enabled_(
          is_background_video_playback_enabled),
      is_background_video_track_optimization_supported_(
          is_background_video_track_optimization_supported),
      should_pause_background_muted_audio_(
          base::FeatureList::IsEnabled(media::kPauseBackgroundMutedAudio)),
      simple_watch_timer_(
          base::BindRepeating(&WebMediaPlayerImpl::OnSimpleWatchTimerTick,
                              base::Unretained(this)),
          base::BindRepeating(&WebMediaPlayerImpl::GetCurrentTimeInternal,
                              base::Unretained(this))),
      will_play_helper_(nullptr) {
  DVLOG(1) << __func__;
  DCHECK(isolate_);
  DCHECK(renderer_factory_selector_);
  DCHECK(client_);
  DCHECK(delegate_);

  if (base::FeatureList::IsEnabled(media::kMediaPowerExperiment)) {
    // The battery monitor is only available through the blink provider.
    DCHECK(remote_interfaces);
    auto battery_monitor_cb = base::BindRepeating(
        [](scoped_refptr<ThreadSafeBrowserInterfaceBrokerProxy>
               remote_interfaces) {
          mojo::PendingRemote<device::mojom::blink::BatteryMonitor>
              battery_monitor;
          remote_interfaces->GetInterface(
              battery_monitor.InitWithNewPipeAndPassReceiver());
          return battery_monitor;
        },
        remote_interfaces);
    power_status_helper_ =
        std::make_unique<PowerStatusHelper>(std::move(battery_monitor_cb));
  }

  weak_this_ = weak_factory_.GetWeakPtr();

  // Using base::Unretained(this) is safe because the `pipeline` is owned by
  // `this` and the callback will always be made on the main task runner.
  // Not using base::BindPostTaskToCurrentDefault() because CreateRenderer() is
  // a sync call.
  auto pipeline = std::make_unique<media::PipelineImpl>(
      media_task_runner_, main_task_runner_,
      base::BindRepeating(&WebMediaPlayerImpl::CreateRenderer,
                          base::Unretained(this)),
      media_log_.get());

  // base::Unretained for |demuxer_manager_| is safe, because it outlives
  // |pipeline_controller_|.
  pipeline_controller_ = std::make_unique<media::PipelineController>(
      std::move(pipeline),
      base::BindRepeating(&WebMediaPlayerImpl::OnPipelineStarted, weak_this_),
      base::BindRepeating(&WebMediaPlayerImpl::OnPipelineSeeked, weak_this_),
      base::BindRepeating(&WebMediaPlayerImpl::OnPipelineSuspended, weak_this_),
      base::BindRepeating(&WebMediaPlayerImpl::OnBeforePipelineResume,
                          weak_this_),
      base::BindRepeating(&WebMediaPlayerImpl::OnPipelineResumed, weak_this_),
      base::BindRepeating(&media::DemuxerManager::OnPipelineError,
                          base::Unretained(demuxer_manager_.get())));

  buffered_data_source_host_ = std::make_unique<BufferedDataSourceHostImpl>(
      base::BindRepeating(&WebMediaPlayerImpl::OnProgress, weak_this_),
      tick_clock_);

  // If we're supposed to force video overlays, then make sure that they're
  // enabled all the time.
  always_enable_overlays_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceVideoOverlays);

  if (base::FeatureList::IsEnabled(media::kOverlayFullscreenVideo))
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

  if (initial_cdm)
    SetCdmInternal(initial_cdm);

  // Report a false "EncrytpedEvent" here as a baseline.
  RecordEncryptedEvent(false);

  auto on_audio_source_provider_set_client_callback = base::BindOnce(
      [](base::WeakPtr<WebMediaPlayerImpl> self,
         WebMediaPlayerClient* const client) {
        if (!self)
          return;
        client->DidDisableAudioOutputSinkChanges();
      },
      weak_this_, client_);

  // TODO(xhwang): When we use an external Renderer, many methods won't work,
  // e.g. GetCurrentFrameFromCompositor(). See http://crbug.com/434861
  audio_source_provider_ = base::MakeRefCounted<WebAudioSourceProviderImpl>(
      std::move(audio_renderer_sink), media_log_.get(),
      std::move(on_audio_source_provider_set_client_callback));

  if (observer_)
    observer_->SetClient(this);

  memory_usage_reporting_timer_.SetTaskRunner(
      frame_->GetTaskRunner(TaskType::kInternalMedia));

  main_thread_mem_dumper_ = std::make_unique<media::MemoryDumpProviderProxy>(
      "WebMediaPlayer_MainThread", main_task_runner_,
      base::BindRepeating(&WebMediaPlayerImpl::OnMainThreadMemoryDump,
                          weak_this_, media_player_id_));

  media_metrics_provider_->AcquirePlaybackEventsRecorder(
      playback_events_recorder_.BindNewPipeAndPassReceiver());

  // MediaMetricsProvider may drop the request for PlaybackEventsRecorder if
  // it's not interested in recording these events.
  playback_events_recorder_.reset_on_disconnect();

#if BUILDFLAG(IS_ANDROID)
  renderer_factory_selector_->SetRemotePlayStateChangeCB(
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &WebMediaPlayerImpl::OnRemotePlayStateChange, weak_this_)));
#endif  // defined (IS_ANDROID)
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  ReportSessionUMAs();

  if (set_cdm_result_) {
    DVLOG(2)
        << "Resolve pending SetCdmInternal() when media player is destroyed.";
    set_cdm_result_->Complete();
    set_cdm_result_.reset();
  }

  suppress_destruction_errors_ = true;
  demuxer_manager_->DisallowFallback();

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
  // `demuxer_manager_`, `compositor_`, and `media_log_` must outlive
  // this process. They will be destructed by the DestructionHelper below
  // after trampolining through the media thread.
  pipeline_controller_->Stop();

  if (last_reported_memory_usage_) {
    external_memory_accounter_.Decrease(isolate_.get(),
                                        last_reported_memory_usage_);
  }

  // Destruct compositor resources in the proper order.
  client_->SetCcLayer(nullptr);

  client_->MediaRemotingStopped(WebMediaPlayerClient::kMediaRemotingStopNoText);

  if (!surface_layer_for_video_enabled_ && video_layer_)
    video_layer_->StopUsingProvider();

  simple_watch_timer_.Stop();
  media_log_->OnWebMediaPlayerDestroyed();

  demuxer_manager_->StopAndResetClient();
  demuxer_manager_->InvalidateWeakPtrs();

  // Disconnect from the surface layer. We still preserve the `bridge_` until
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

  // Explicitly reset `pipeline_controller_` to guarantee its destruction
  // before DestructionHelper runs on `media_task_runner_`.
  // This prevents possible dangling ptr's if `compositor` is destroyed
  // before `pipeline_controller_`, which holds a VideoRendererSink
  // in MediaFoundationRendererClient.
  pipeline_controller_.reset();

  // Handle destruction of things that need to be destructed after the pipeline
  // completes stopping on the media thread.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DestructionHelper, std::move(main_task_runner_),
                     std::move(vfc_task_runner_), std::move(demuxer_manager_),
                     std::move(compositor_), std::move(cdm_context_ref_),
                     std::move(pending_cdm_context_ref_), std::move(media_log_),
                     std::move(renderer_factory_selector_),
                     std::move(bridge_)));
}

WebMediaPlayer::LoadTiming WebMediaPlayerImpl::Load(
    LoadType load_type,
    const WebMediaPlayerSource& source,
    CorsMode cors_mode,
    bool is_cache_disabled) {
  // Only URL or MSE blob URL is supported.
  DCHECK(source.IsURL());
  WebURL url = source.GetAsURL();
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
  // `client_` will unregister its cc::Layer if given a nullptr.
  client_->SetCcLayer(nullptr);
}

void WebMediaPlayerImpl::OnSurfaceIdUpdated(viz::SurfaceId surface_id) {
  // TODO(726619): Handle the behavior when Picture-in-Picture mode is
  // disabled.
  // The viz::SurfaceId may be updated when the video begins playback or when
  // the size of the video changes.
  if (client_ && !client_->IsAudioElement()) {
    client_->OnPictureInPictureStateChange();
  }
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
    overlay_routing_token_ = media::OverlayInfo::RoutingToken();
  }

  if (decoder_requires_restart_for_overlay_)
    ScheduleRestart();
  else
    MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::EnteredFullscreen() {
  overlay_info_.is_fullscreen = true;

  // `always_enable_overlays_` implies that we're already in overlay mode, so
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
  // routing information.  Note that we set `is_fullscreen_` earlier, so that
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
    WebFullscreenVideoStatus fullscreen_video_status) {
  if (power_status_helper_) {
    // We don't care about pip, so anything that's "not fullscreen" is good
    // enough for us.
    power_status_helper_->SetIsFullscreen(
        fullscreen_video_status !=
        WebFullscreenVideoStatus::kNotEffectivelyFullscreen);
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

void WebMediaPlayerImpl::OnDisplayTypeChanged(DisplayType display_type) {
  DVLOG(2) << __func__ << ": display_type=" << static_cast<int>(display_type);

  if (surface_layer_for_video_enabled_) {
    vfc_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoFrameCompositor::SetForceSubmit,
                       base::Unretained(compositor_.get()),
                       display_type == DisplayType::kPictureInPicture));

    if (display_type == DisplayType::kPictureInPicture) {
      // In picture in picture mode, since the video is compositing in the PIP
      // windows, stop composting it in the original window. One exception is
      // for persistent video, where can happen in auto-pip mode, where the
      // video is not playing in the regular Picture-in-Picture mode.
      if (!client_->IsInAutoPIP()) {
        client_->SetCcLayer(nullptr);
      }

      // Resumes playback if it was paused when hidden.
      if (IsPausedBecauseFrameHidden() || IsPausedBecausePageHidden()) {
        visibility_pause_reason_.reset();
        client_->ResumePlayback();
      }
    } else {
      // Resume compositing in the original window if not already doing so.
      client_->SetCcLayer(bridge_->GetCcLayer());
    }
  }

  if (watch_time_reporter_) {
    switch (display_type) {
      case DisplayType::kInline:
        watch_time_reporter_->OnDisplayTypeInline();
        break;
      case DisplayType::kFullscreen:
        watch_time_reporter_->OnDisplayTypeFullscreen();
        break;
      case DisplayType::kPictureInPicture:
        watch_time_reporter_->OnDisplayTypePictureInPicture();
        break;
    }
  }

  SetPersistentState(display_type == DisplayType::kPictureInPicture);
  UpdatePlayState();
}

void WebMediaPlayerImpl::DoLoad(LoadType load_type,
                                const WebURL& url,
                                CorsMode cors_mode,
                                bool is_cache_disabled) {
  TRACE_EVENT1("media", "WebMediaPlayerImpl::DoLoad", "id", media_player_id_);
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  is_cache_disabled_ = is_cache_disabled;
  cors_mode_ = cors_mode;

  // Start a new observation.  If there was one before, then we didn't play it.
  will_play_helper_.CompleteObservationIfNeeded(learning::TargetValue(false));
  // For now, send in an empty set of features.  We should fill some in here,
  // and / or ask blink (via `client_`) for features from the DOM.
  learning::FeatureDictionary dict;
  will_play_helper_.BeginObservation(dict);

#if BUILDFLAG(IS_ANDROID)
  // Only allow credentials if the crossorigin attribute is unspecified
  // (kCorsModeUnspecified) or "use-credentials" (kCorsModeUseCredentials).
  // This value is only used by the MediaPlayerRenderer.
  // See https://crbug.com/936566.
  demuxer_manager_->SetAllowMediaPlayerRendererCredentials(cors_mode !=
                                                           kCorsModeAnonymous);
#endif  // BUILDFLAG(IS_ANDROID)

  // Note: `url` may be very large, take care when making copies.
  demuxer_manager_->SetLoadedUrl(GURL(url));
  load_type_ = load_type;

  ReportMetrics(load_type, demuxer_manager_->LoadedUrl(), media_log_.get());

  // Set subresource URL for crash reporting; will be truncated to 256 bytes.
  static base::debug::CrashKeyString* subresource_url =
      base::debug::AllocateCrashKeyString("subresource_url",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(subresource_url,
                                 demuxer_manager_->LoadedUrl().spec());

  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);

  // Do a truncation to kMaxUrlLength+1 at most; we can add ellipsis later.
  media_log_->AddEvent<MediaLogEvent::kLoad>(
      url.GetString().Substring(0, media::kMaxUrlLength + 1).Utf8());
  load_start_time_ = base::TimeTicks::Now();

  // If we're adapting, then restart the smoothness experiment.
  if (smoothness_helper_)
    smoothness_helper_.reset();

  media_metrics_provider_->Initialize(
      load_type == kLoadTypeMediaSource,
      load_type == kLoadTypeURL
          ? GetMediaURLScheme(demuxer_manager_->LoadedUrl())
          : media::mojom::MediaURLScheme::kUnknown,
      media::mojom::MediaStreamType::kNone);

  // If a demuxer override was specified or a Media Source pipeline will be
  // used, the pipeline can start immediately.
  if (demuxer_manager_->HasDemuxerOverride() ||
      load_type == kLoadTypeMediaSource ||
      demuxer_manager_->LoadedUrl().SchemeIs(
          media::remoting::kRemotingScheme)) {
    StartPipeline();
    return;
  }

  // Short circuit the more complex loading path for data:// URLs. Sending
  // them through the network based loading path just wastes memory and causes
  // worse performance since reads become asynchronous.
  if (demuxer_manager_->LoadedUrl().SchemeIs(url::kDataScheme)) {
    std::string mime_type, charset, data;
    if (!net::DataURL::Parse(demuxer_manager_->LoadedUrl(), &mime_type,
                             &charset, &data) ||
        data.empty()) {
      return MemoryDataSourceInitialized(false, 0);
    }
    size_t data_size = data.size();
    demuxer_manager_->SetDataSource(
        std::make_unique<media::MemoryDataSource>(std::move(data)));
    MemoryDataSourceInitialized(true, data_size);
    return;
  }

  auto data_source = std::make_unique<MultiBufferDataSource>(
      main_task_runner_,
      url_index_->GetByUrl(
          url, static_cast<UrlData::CorsMode>(cors_mode),
          is_cache_disabled ? UrlData::kCacheDisabled : UrlData::kNormal),
      media_log_.get(), buffered_data_source_host_.get(),
      base::BindRepeating(&WebMediaPlayerImpl::NotifyDownloading, weak_this_));

  auto* mb_data_source = data_source.get();
  demuxer_manager_->SetDataSource(std::move(data_source));

  mb_data_source->OnRedirect(base::BindRepeating(
      &WebMediaPlayerImpl::OnDataSourceRedirected, weak_this_));
  mb_data_source->SetPreload(preload_);
  mb_data_source->SetIsClientAudioElement(client_->IsAudioElement());
  mb_data_source->Initialize(base::BindOnce(
      &WebMediaPlayerImpl::MultiBufferDataSourceInitialized, weak_this_));
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
  visibility_pause_reason_.reset();

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

void WebMediaPlayerImpl::OnFrozen() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // We should already be paused before we are frozen.
  DCHECK(paused_);

  if (observer_)
    observer_->OnFrozen();
}

void WebMediaPlayerImpl::Seek(double seconds) {
  DVLOG(1) << __func__ << "(" << seconds << "s)";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DoSeek(base::Seconds(seconds), true);
}

void WebMediaPlayerImpl::DoSeek(base::TimeDelta time, bool time_updated) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT2("media", "WebMediaPlayerImpl::DoSeek", "target",
               time.InSecondsF(), "id", media_player_id_);

  ReadyState old_state = ready_state_;
  if (ready_state_ > WebMediaPlayer::kReadyStateHaveMetadata)
    SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);

  // For zero duration video-only media, if we can elide the seek, use a large
  // delay to avoid an expensive spin loop. Per spec we must still deliver all
  // the requisite events, but we're not required to be timely about it.
  //
  // 250ms matches the max timeupdate interval used by the media element.
  auto delay = base::TimeDelta();
  bool is_at_eos = false;
  if (ended_) {
    if (time == base::Seconds(Duration())) {
      is_at_eos = true;
    } else if (!HasAudio()) {
      if (auto frame = compositor_->GetCurrentFrameOnAnyThread()) {
        if (frame->timestamp() == GetCurrentTimeInternal()) {
          is_at_eos = true;
          delay = base::Milliseconds(250);
        }
      }
    }
  }

  // When paused or ended, we know exactly what the current time is and can
  // elide seeks to it. However, there are three cases that are not elided:
  //   1) When the pipeline state is not stable.
  //      In this case we just let PipelineController decide what to do, as
  //      it has complete information.
  //   2) When the ready state was not kReadyStateHaveEnoughData.
  //      If playback has not started, it's possible to enter a state where
  //      OnBufferingStateChange() will not be called again to complete the
  //      seek.
  //   3) For MSE.
  //      Because the buffers may have changed between seeks, MSE seeks are
  //      never elided.
  if (((paused_ && paused_time_ == time) || (ended_ && is_at_eos)) &&
      pipeline_controller_->IsStable() &&
      GetDemuxerType() != media::DemuxerType::kChunkDemuxer) {
    if (old_state == kReadyStateHaveEnoughData) {
      // This will in turn SetReadyState() to signal the demuxer seek, followed
      // by timeChanged() to signal the renderer seek.
      should_notify_time_changed_ = true;

      if (has_first_frame_) {
        // Seek will always emit a new frame -- even if the it's the same frame
        // it will be decoded again with a new frame id, so simulate that here.
        main_task_runner_->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&WebMediaPlayerImpl::OnNewFramePresentedCallback,
                           weak_this_),
            delay);
      }

      main_task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&WebMediaPlayerImpl::OnBufferingStateChange,
                         weak_this_, media::BUFFERING_HAVE_ENOUGH,
                         media::BUFFERING_CHANGE_REASON_UNKNOWN),
          delay);
      return;
    }
  }

  media_log_->AddEvent<MediaLogEvent::kSeek>(time.InSecondsF());

  if (playback_events_recorder_)
    playback_events_recorder_->OnSeeking();

  // Call this before setting `seeking_` so that the current media time can be
  // recorded by the reporter.
  if (watch_time_reporter_)
    watch_time_reporter_->OnSeeking();

  // TODO(sandersd): Move `seeking_` to PipelineController.
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
    DidMediaMetadataChange();

    // If we paused a background video in a non-visible page since it was muted,
    // the volume change should resume the playback.
    if (IsPausedBecausePageHidden()) {
      visibility_pause_reason_.reset();
      // Calls UpdatePlayState() so return afterwards.
      client_->ResumePlayback();
      return;
    }
  }

  // The play state is updated because the player might have left the autoplay
  // muted state.
  UpdatePlayState();
}

void WebMediaPlayerImpl::SetLatencyHint(double seconds) {
  DVLOG(1) << __func__ << "(" << seconds << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  std::optional<base::TimeDelta> latency_hint;
  if (std::isfinite(seconds)) {
    DCHECK_GE(seconds, 0);
    latency_hint = base::Seconds(seconds);
  }
  pipeline_controller_->SetLatencyHint(latency_hint);
}

void WebMediaPlayerImpl::SetPreservesPitch(bool preserves_pitch) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  pipeline_controller_->SetPreservesPitch(preserves_pitch);
}

void WebMediaPlayerImpl::SetWasPlayedWithUserActivationAndHighMediaEngagement(
    bool was_played_with_user_activation_and_high_media_engagement) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  pipeline_controller_->SetWasPlayedWithUserActivationAndHighMediaEngagement(
      was_played_with_user_activation_and_high_media_engagement);
}

void WebMediaPlayerImpl::SetShouldPauseWhenFrameIsHidden(
    bool should_pause_when_frame_is_hidden) {
  should_pause_when_frame_is_hidden_ = should_pause_when_frame_is_hidden;
}

bool WebMediaPlayerImpl::GetShouldPauseWhenFrameIsHidden() {
  return should_pause_when_frame_is_hidden_;
}

void WebMediaPlayerImpl::OnRequestPictureInPicture() {
  ActivateSurfaceLayerForVideo();

  DCHECK(bridge_);
  DCHECK(bridge_->GetSurfaceId().is_valid());
}

bool WebMediaPlayerImpl::SetSinkId(
    const WebString& sink_id,
    WebSetSinkIdCompleteCallback completion_callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __func__;

  media::OutputDeviceStatusCB callback =
      ConvertToOutputDeviceStatusCB(std::move(completion_callback));
  auto sink_id_utf8 = sink_id.Utf8();
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SetSinkIdOnMediaThread, audio_source_provider_,
                                sink_id_utf8, std::move(callback)));
  return true;
}

STATIC_ASSERT_ENUM(WebMediaPlayer::kPreloadNone, media::DataSource::NONE);
STATIC_ASSERT_ENUM(WebMediaPlayer::kPreloadMetaData,
                   media::DataSource::METADATA);
STATIC_ASSERT_ENUM(WebMediaPlayer::kPreloadAuto, media::DataSource::AUTO);

void WebMediaPlayerImpl::SetPreload(WebMediaPlayer::Preload preload) {
  DVLOG(1) << __func__ << "(" << preload << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  preload_ = static_cast<media::DataSource::Preload>(preload);
  demuxer_manager_->SetPreload(preload_);
}

bool WebMediaPlayerImpl::HasVideo() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_metadata_.has_video;
}

bool WebMediaPlayerImpl::HasAudio() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_metadata_.has_audio;
}

void WebMediaPlayerImpl::OnEnabledAudioTracksChanged(
    std::vector<media::MediaTrack::Id> enabled) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent<MediaLogEvent::kAudioTrackChange>(enabled);
  pipeline_controller_->OnEnabledAudioTracksChanged(enabled);
}

void WebMediaPlayerImpl::OnSelectedVideoTrackChanged(
    std::optional<media::MediaTrack::Id> selected) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent<MediaLogEvent::kVideoTrackChange>(selected);
  pipeline_controller_->OnSelectedVideoTrackChanged(selected);
}

void WebMediaPlayerImpl::EnabledAudioTracksChanged(
    const WebVector<WebMediaPlayer::TrackId>& enabled_track_ids) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  std::vector<MediaTrack::Id> enabled_tracks;
  for (const auto& blinkTrackId : enabled_track_ids) {
    enabled_tracks.push_back(MediaTrack::Id(blinkTrackId.Utf8().data()));
  }
  OnEnabledAudioTracksChanged(std::move(enabled_tracks));
}

void WebMediaPlayerImpl::SelectedVideoTrackChanged(
    std::optional<WebMediaPlayer::TrackId> selected_track_id) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  std::optional<MediaTrack::Id> selected_track;
  if (selected_track_id.has_value()) {
    selected_track = MediaTrack::Id(selected_track_id->Utf8().data());
  }
  OnSelectedVideoTrackChanged(selected_track);
}

gfx::Size WebMediaPlayerImpl::NaturalSize() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_metadata_.natural_size;
}

gfx::Size WebMediaPlayerImpl::VisibleSize() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  scoped_refptr<media::VideoFrame> video_frame =
      GetCurrentFrameFromCompositor();
  if (!video_frame)
    return gfx::Size();

  return video_frame->visible_rect().size();
}

bool WebMediaPlayerImpl::Paused() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return paused_;
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

  // Some demuxer's might have more accurate duration information than the
  // pipeline, so check that first.
  std::optional<double> duration = demuxer_manager_->GetDemuxerDuration();
  if (duration.has_value()) {
    return *duration;
  }

  base::TimeDelta pipeline_duration = GetPipelineMediaDuration();
  return pipeline_duration == media::kInfiniteDuration
             ? std::numeric_limits<double>::infinity()
             : pipeline_duration.InSecondsF();
}

double WebMediaPlayerImpl::timelineOffset() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (pipeline_metadata_.timeline_offset.is_null())
    return std::numeric_limits<double>::quiet_NaN();

  return pipeline_metadata_.timeline_offset.InMillisecondsFSinceUnixEpoch();
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

  // It's possible for `current_time` to be kInfiniteDuration here if the page
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

WebString WebMediaPlayerImpl::GetErrorMessage() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return WebString::FromUTF8(media_log_->GetErrorMessage());
}

WebTimeRanges WebMediaPlayerImpl::Buffered() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  media::Ranges<base::TimeDelta> buffered_time_ranges =
      pipeline_controller_->GetBufferedTimeRanges();

  const base::TimeDelta duration = GetPipelineMediaDuration();
  if (duration != media::kInfiniteDuration) {
    buffered_data_source_host_->AddBufferedTimeRanges(&buffered_time_ranges,
                                                      duration);
  }
  return ConvertToWebTimeRanges(buffered_time_ranges);
}

WebTimeRanges WebMediaPlayerImpl::Seekable() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (ready_state_ < WebMediaPlayer::kReadyStateHaveMetadata) {
    return WebTimeRanges();
  }

  if (demuxer_manager_->IsLiveContent()) {
    return WebTimeRanges();
  }

  const double seekable_end = Duration();

  // Allow a special exception for seeks to zero for streaming sources with a
  // finite duration; this allows looping to work.
  const bool is_finite_stream = IsStreaming() && std::isfinite(seekable_end);

  // Do not change the seekable range when using the MediaPlayerRenderer. It
  // will take care of dropping invalid seeks.
  const bool force_seeks_to_zero =
      !using_media_player_renderer_ && is_finite_stream;

  // TODO(dalecurtis): Technically this allows seeking on media which return an
  // infinite duration so long as DataSource::IsStreaming() is false. While not
  // expected, disabling this breaks semi-live players, http://crbug.com/427412.
  const WebTimeRange seekable_range(0.0,
                                    force_seeks_to_zero ? 0.0 : seekable_end);
  return WebTimeRanges(&seekable_range, 1);
}

bool WebMediaPlayerImpl::IsPrerollAttemptNeeded() {
  // TODO(sandersd): Replace with `highest_ready_state_since_seek_` if we need
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
  // `preroll_attempt_pending_` would be true when the start time is null.)
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

  scoped_refptr<media::VideoFrame> video_frame =
      GetCurrentFrameFromCompositor();
  last_frame_request_time_ = tick_clock_->NowTicks();
  video_frame_readback_count_++;
  pipeline_controller_->OnExternalVideoFrameRequest();

  media::PaintCanvasVideoRenderer::PaintParams paint_params;
  paint_params.dest_rect = gfx::RectF(rect);
  paint_params.transformation =
      pipeline_metadata_.video_decoder_config.video_transformation();
  video_renderer_.Paint(video_frame, canvas, flags, paint_params,
                        raster_context_provider_.get());
}

scoped_refptr<media::VideoFrame>
WebMediaPlayerImpl::GetCurrentFrameThenUpdate() {
  last_frame_request_time_ = tick_clock_->NowTicks();
  video_frame_readback_count_++;
  pipeline_controller_->OnExternalVideoFrameRequest();
  return GetCurrentFrameFromCompositor();
}

std::optional<media::VideoFrame::ID> WebMediaPlayerImpl::CurrentFrameId()
    const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl::GetCurrentFrameID");

  // We can't copy from protected frames.
  if (cdm_context_ref_)
    return std::nullopt;

  if (auto frame = compositor_->GetCurrentFrameOnAnyThread())
    return frame->unique_id();
  return std::nullopt;
}

media::PaintCanvasVideoRenderer*
WebMediaPlayerImpl::GetPaintCanvasVideoRenderer() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return &video_renderer_;
}

bool WebMediaPlayerImpl::WouldTaintOrigin() const {
  return demuxer_manager_->WouldTaintOrigin();
}

double WebMediaPlayerImpl::MediaTimeForTimeValue(double timeValue) const {
  return base::Seconds(timeValue).InSecondsF();
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

bool WebMediaPlayerImpl::HasReadableVideoFrame() const {
  return has_first_frame_ && is_frame_readable_;
}

void WebMediaPlayerImpl::SetContentDecryptionModule(
    WebContentDecryptionModule* cdm,
    WebContentDecryptionModuleResult result) {
  DVLOG(1) << __func__ << ": cdm = " << cdm;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Once the CDM is set it can't be cleared as there may be frames being
  // decrypted on other threads. So fail this request.
  // http://crbug.com/462365#c7.
  if (!cdm) {
    result.CompleteWithError(
        kWebContentDecryptionModuleExceptionInvalidStateError, 0,
        "The existing ContentDecryptionModule object cannot be removed at this "
        "time.");
    return;
  }

  // Create a local copy of `result` to avoid problems with the callback
  // getting passed to the media thread and causing `result` to be destructed
  // on the wrong thread in some failure conditions. Blink should prevent
  // multiple simultaneous calls.
  DCHECK(!set_cdm_result_);
  set_cdm_result_ = std::make_unique<WebContentDecryptionModuleResult>(result);

  SetCdmInternal(cdm);
}

void WebMediaPlayerImpl::OnEncryptedMediaInitData(
    media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data) {
  DCHECK(init_data_type != media::EmeInitDataType::UNKNOWN);

  RecordEncryptedEvent(true);

  // Recreate the watch time reporter if necessary.
  const bool was_encrypted = is_encrypted_;
  is_encrypted_ = true;
  if (!was_encrypted) {
    media_metrics_provider_->SetIsEME();
    if (watch_time_reporter_)
      CreateWatchTimeReporter();

    // `was_encrypted` = false means we didn't have a CDM prior to observing
    // encrypted media init data. Reset the reporter until the CDM arrives. See
    // SetCdmInternal().
    DCHECK(!cdm_config_);
    video_decode_stats_reporter_.reset();
  }

  encrypted_client_->Encrypted(
      init_data_type, init_data.data(),
      base::saturated_cast<unsigned int>(init_data.size()));
}

#if BUILDFLAG(ENABLE_FFMPEG) || BUILDFLAG(ENABLE_HLS_DEMUXER)

void WebMediaPlayerImpl::AddMediaTrack(const media::MediaTrack& track) {
  client_->AddMediaTrack(track);
}

void WebMediaPlayerImpl::RemoveMediaTrack(const media::MediaTrack& track) {
  client_->RemoveMediaTrack(track);
}

#endif  // BUILDFLAG(ENABLE_FFMPEG) || BUILDFLAG(ENABLE_HLS_DEMUXER)

#if BUILDFLAG(ENABLE_HLS_DEMUXER)

void WebMediaPlayerImpl::GetUrlData(
    const GURL& gurl,
    bool ignore_cache,
    base::OnceCallback<void(scoped_refptr<UrlData>)> cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  auto url_data = url_index_->GetByUrl(
      gurl, static_cast<UrlData::CorsMode>(cors_mode_),
      (is_cache_disabled_ || ignore_cache) ? UrlData::kCacheDisabled
                                           : UrlData::kNormal);
  std::move(cb).Run(std::move(url_data));
}

base::SequenceBound<media::HlsDataSourceProvider>
WebMediaPlayerImpl::GetHlsDataSourceProvider() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return base::SequenceBound<media::HlsDataSourceProviderImpl>(
      main_task_runner_,
      std::make_unique<MultiBufferDataSourceFactory>(
          media_log_.get(),
          base::BindRepeating(&WebMediaPlayerImpl::GetUrlData,
                              weak_factory_.GetWeakPtr()),
          main_task_runner_, tick_clock_));
}
#endif

void WebMediaPlayerImpl::SetCdmInternal(WebContentDecryptionModule* cdm) {
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
    NOTREACHED_IN_MIGRATION();
    OnCdmAttached(false);
    return;
  }

  // Arrival of `cdm_config_` unblocks recording of encrypted stats. Attempt to
  // create the stats reporter. Note, we do NOT guard this within !was_encypted
  // above because often the CDM arrives after the call to
  // OnEncryptedMediaInitData().
  cdm_config_ = web_cdm->GetCdmConfig();
  DCHECK(!cdm_config_->key_system.empty());

  media_log_->SetProperty<MediaLogProperty::kSetCdm>(cdm_config_.value());

  media_metrics_provider_->SetKeySystem(cdm_config_->key_system);
  if (cdm_config_->use_hw_secure_codecs)
    media_metrics_provider_->SetIsHardwareSecure();
  CreateVideoDecodeStatsReporter();

  auto* cdm_context = cdm_context_ref->GetCdmContext();
  DCHECK(cdm_context);

  // Keep the reference to the CDM, as it shouldn't be destroyed until
  // after the pipeline is done with the `cdm_context`.
  pending_cdm_context_ref_ = std::move(cdm_context_ref);
  pipeline_controller_->SetCdm(
      cdm_context,
      base::BindOnce(&WebMediaPlayerImpl::OnCdmAttached, weak_this_));
}

void WebMediaPlayerImpl::OnCdmAttached(bool success) {
  DVLOG(1) << __func__ << ": success = " << success;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(pending_cdm_context_ref_);

  media_log_->SetProperty<MediaLogProperty::kIsCdmAttached>(success);

  // If the CDM is set from the constructor there is no promise
  // (`set_cdm_result_`) to fulfill.
  if (success) {
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
        kWebContentDecryptionModuleExceptionNotSupportedError, 0,
        "Unable to set ContentDecryptionModule object");
    set_cdm_result_.reset();
  }
}

void WebMediaPlayerImpl::OnPipelineSeeked(bool time_updated) {
  TRACE_EVENT2("media", "WebMediaPlayerImpl::OnPipelineSeeked", "target",
               seek_time_.InSecondsF(), "id", media_player_id_);
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
    OnBufferingStateChangeInternal(media::BUFFERING_HAVE_ENOUGH,
                                   media::BUFFERING_CHANGE_REASON_UNKNOWN,
                                   true);
  }

  attempting_suspended_start_ = false;
}

void WebMediaPlayerImpl::OnPipelineStarted(media::PipelineStatus status) {
  media_metrics_provider_->OnStarted(status);
}

void WebMediaPlayerImpl::OnPipelineSuspended() {
  // Add a log event so the player shows up as "SUSPENDED" in media-internals.
  media_log_->AddEvent<MediaLogEvent::kSuspended>();

  pending_oneshot_suspend_ = false;

  if (attempting_suspended_start_) {
    DCHECK(pipeline_controller_->IsSuspended());
    did_lazy_load_ = !has_poster_ && HasVideo();
  }

  // Tell the data source we have enough data so that it may release the
  // connection (unless blink is waiting on us to signal play()).
  if (demuxer_manager_->HasDataSource() && !CouldPlayIfEnoughData()) {
    // `attempting_suspended_start_` will be cleared by OnPipelineSeeked() which
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
      // will cancel upon destruction of this class and `demuxer_manager_` is
      // gauranteeed to outlive us as a result of the DestructionHelper.
      have_enough_after_lazy_load_cb_.Reset(
          base::BindOnce(&media::DemuxerManager::OnBufferingHaveEnough,
                         base::Unretained(demuxer_manager_.get()), true));
      main_task_runner_->PostDelayedTask(
          FROM_HERE, have_enough_after_lazy_load_cb_.callback(),
          base::Milliseconds(250));
    } else {
      have_enough_after_lazy_load_cb_.Cancel();
      demuxer_manager_->OnBufferingHaveEnough(true);
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
  // up for playback. As such adjust `load_start_time_` so it reports the same
  // metric as what would be reported if we had not suspended at startup.
  if (skip_metrics_due_to_startup_suspend_) {
    // In the event that the call to SetReadyState() initiated after pipeline
    // startup immediately tries to start playback, we should not update
    // `load_start_time_` to avoid losing visibility into the impact of a
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

void WebMediaPlayerImpl::OnChunkDemuxerOpened(media::ChunkDemuxer* demuxer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  client_->MediaSourceOpened(std::make_unique<WebMediaSourceImpl>(demuxer));
}

void WebMediaPlayerImpl::OnFallback(media::PipelineStatus status) {
  media_metrics_provider_->OnFallback(std::move(status).AddHere());
}

void WebMediaPlayerImpl::StopForDemuxerReset() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(pipeline_controller_);
  pipeline_controller_->Stop();

  // delete the thread dumper on the media thread.
  media_task_runner_->DeleteSoon(FROM_HERE,
                                 std::move(media_thread_mem_dumper_));
}

bool WebMediaPlayerImpl::IsSecurityOriginCryptographic() const {
  return url::Origin(frame_->GetSecurityOrigin())
      .GetURL()
      .SchemeIsCryptographic();
}

void WebMediaPlayerImpl::UpdateLoadedUrl(const GURL& url) {
  demuxer_manager_->SetLoadedUrl(url);
}

void WebMediaPlayerImpl::DemuxerRequestsSeek(base::TimeDelta seek_time) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DoSeek(seek_time, true);
}

void WebMediaPlayerImpl::RestartForHls() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  observer_->OnHlsManifestDetected();

  // Use the media player renderer if the native hls demuxer isn't compiled in
  // or if the feature is disabled.
#if BUILDFLAG(ENABLE_HLS_DEMUXER)
  if (!base::FeatureList::IsEnabled(media::kBuiltInHlsPlayer)) {
    renderer_factory_selector_->SetBaseRendererType(
        media::RendererType::kMediaPlayer);
  }
#elif BUILDFLAG(IS_ANDROID)
  renderer_factory_selector_->SetBaseRendererType(
      media::RendererType::kMediaPlayer);
#else
  // Shouldn't be reachable from desktop where hls is not enabled.
  NOTREACHED_IN_MIGRATION();
#endif
  SetMemoryReportingState(false);
  StartPipeline();
}

void WebMediaPlayerImpl::OnError(media::PipelineStatus status) {
  DVLOG(1) << __func__ << ": status=" << status;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(status != media::PIPELINE_OK);

  if (suppress_destruction_errors_)
    return;

#if BUILDFLAG(IS_WIN)
  // Hardware context reset is not an error. Restart to recover.
  // TODO(crbug.com/1208618): Find a way to break the potential infinite loop of
  // restart -> PIPELINE_ERROR_HARDWARE_CONTEXT_RESET -> restart.
  if (status == media::PIPELINE_ERROR_HARDWARE_CONTEXT_RESET) {
    ScheduleRestart();
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

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
    SetNetworkState(PipelineErrorToNetworkState(status.code()));
  }

  // PipelineController::Stop() is idempotent.
  pipeline_controller_->Stop();

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnEnded() {
  TRACE_EVENT2("media", "WebMediaPlayerImpl::OnEnded", "duration", Duration(),
               "id", media_player_id_);
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Ignore state changes until we've completed all outstanding operations.
  if (!pipeline_controller_->IsStable())
    return;

  ended_ = true;
  if (!paused_) {
    client_->TimeChanged();
  }

  if (playback_events_recorder_)
    playback_events_recorder_->OnEnded();

  // We don't actually want this to run until `client_` calls seek() or pause(),
  // but that should have already happened in timeChanged() and so this is
  // expected to be a no-op.
  UpdatePlayState();
}

void WebMediaPlayerImpl::OnMetadata(const media::PipelineMetadata& metadata) {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Cache the `time_to_metadata_` to use for adjusting the TimeToFirstFrame and
  // TimeToPlayReady metrics later if we end up doing a suspended startup.
  time_to_metadata_ = base::TimeTicks::Now() - load_start_time_;
  media_metrics_provider_->SetTimeToMetadata(time_to_metadata_);
  WriteSplitHistogram<kPlaybackType | kEncrypted>(
      &base::UmaHistogramMediumTimes, SplitHistogramName::kTimeToMetadata,
      time_to_metadata_);

  MaybeSetContainerNameForMetrics();

  pipeline_metadata_ = metadata;
  if (power_status_helper_)
    power_status_helper_->SetMetadata(metadata);

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
      // the video is now rotated.  If `always_enable_overlays_`, we keep the
      // overlay anyway so that the state machine keeps working.
      // TODO(liberato): verify if compositor feedback catches this.  If so,
      // then we don't need this check.
      if (!always_enable_overlays_ && !DoesOverlaySupportMetadata())
        DisableOverlay();
    }

    if (use_surface_layer_) {
      ActivateSurfaceLayerForVideo();
    } else {
      DCHECK(!video_layer_);
      video_layer_ = cc::VideoLayer::Create(
          compositor_.get(),
          pipeline_metadata_.video_decoder_config.video_transformation());
      video_layer_->SetContentsOpaque(opaque_);
      client_->SetCcLayer(video_layer_.get());
    }
  }

  if (observer_)
    observer_->OnMetadataChanged(pipeline_metadata_);

  delegate_has_audio_ = HasUnmutedAudio();
  DidMediaMetadataChange();

  // It could happen that the demuxer successfully completed initialization
  // (implying it had determined media metadata), but then removed all audio and
  // video streams and the ability to demux any A/V before `metadata` was
  // constructed and passed to us. One example is, with MSE-in-Workers, the
  // worker owning the MediaSource could have been terminated, or the app could
  // have explicitly removed all A/V SourceBuffers. That termination/removal
  // could race the construction of `metadata`. Regardless of load-type, we
  // shouldn't allow playback of a resource that has neither audio nor video.
  // We treat lack of A/V as if there were an error in the demuxer before
  // reaching HAVE_METADATA.
  if (!HasVideo() && !HasAudio()) {
    DVLOG(1) << __func__ << ": no audio and no video -> error";
    OnError(media::DEMUXER_ERROR_COULD_NOT_OPEN);
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
  if (surface_layer_for_video_enabled_) {
    // Surface layer has already been activated.
    return;
  }

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
          pipeline_metadata_.video_decoder_config.video_transformation(),
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
    media::BufferingState state,
    media::BufferingStateChangeReason reason) {
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
      media::VIDEO_CODEC_PROFILE_UNKNOWN) {
    return;
  }

  // CdmConfig must be provided for use as index to save encrypted stats.
  if (is_encrypted_ && !cdm_config_) {
    return;
  } else if (cdm_config_) {
    DCHECK(!cdm_config_->key_system.empty());
  }

  mojo::PendingRemote<media::mojom::VideoDecodeStatsRecorder> recorder;
  media_metrics_provider_->AcquireVideoDecodeStatsRecorder(
      recorder.InitWithNewPipeAndPassReceiver());

  // Create capabilities reporter and synchronize its initial state.
  video_decode_stats_reporter_ = std::make_unique<VideoDecodeStatsReporter>(
      std::move(recorder),
      base::BindRepeating(&WebMediaPlayerImpl::GetPipelineStatistics,
                          base::Unretained(this)),
      pipeline_metadata_.video_decoder_config.profile(),
      pipeline_metadata_.natural_size, cdm_config_,
      frame_->GetTaskRunner(TaskType::kInternalMedia));

  if (delegate_->IsPageHidden()) {
    video_decode_stats_reporter_->OnHidden();
  } else {
    video_decode_stats_reporter_->OnShown();
  }

  if (paused_)
    video_decode_stats_reporter_->OnPaused();
  else
    video_decode_stats_reporter_->OnPlaying();
}

void WebMediaPlayerImpl::OnProgress() {
  DVLOG(4) << __func__;

  // See IsPrerollAttemptNeeded() for more details. We can't use that method
  // here since it considers `preroll_attempt_start_time_` and for OnProgress()
  // events we must make the attempt -- since there may not be another event.
  if (highest_ready_state_ < ReadyState::kReadyStateHaveFutureData) {
    // Reset the preroll attempt clock.
    preroll_attempt_pending_ = true;
    preroll_attempt_start_time_ = base::TimeTicks();

    // Clear any 'stale' flag and give the pipeline a chance to resume. If we
    // are already resumed, this will cause `preroll_attempt_start_time_` to
    // be set.
    delegate_->ClearStaleFlag(delegate_id_);
    UpdatePlayState();
  } else if (ready_state_ == ReadyState::kReadyStateHaveFutureData &&
             CanPlayThrough()) {
    SetReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);
  }
}

bool WebMediaPlayerImpl::CanPlayThrough() {
  if (!base::FeatureList::IsEnabled(media::kSpecCompliantCanPlayThrough))
    return true;
  if (GetDemuxerType() == media::DemuxerType::kChunkDemuxer)
    return true;
  if (demuxer_manager_->DataSourceFullyBuffered()) {
    return true;
  }
  // If we're not currently downloading, we have as much buffer as
  // we're ever going to get, which means we say we can play through.
  if (network_state_ == WebMediaPlayer::kNetworkStateIdle)
    return true;
  return buffered_data_source_host_->CanPlayThrough(
      base::Seconds(CurrentTime()), base::Seconds(Duration()),
      playback_rate_ == 0.0 ? 1.0 : playback_rate_);
}

void WebMediaPlayerImpl::OnBufferingStateChangeInternal(
    media::BufferingState state,
    media::BufferingStateChangeReason reason,
    bool for_suspended_start) {
  DVLOG(1) << __func__ << "(" << state << ", " << reason << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Ignore buffering state changes caused by back-to-back seeking, so as not
  // to assume the second seek has finished when it was only the first seek.
  if (pipeline_controller_->IsPendingSeek())
    return;

  media_log_->AddEvent<MediaLogEvent::kBufferingStateChanged>(
      media::SerializableBufferingState<
          media::SerializableBufferingStateType::kPipeline>{
          state, reason, for_suspended_start});

  if (state == media::BUFFERING_HAVE_ENOUGH && !for_suspended_start)
    media_metrics_provider_->SetHaveEnough();

  if (state == media::BUFFERING_HAVE_ENOUGH) {
    TRACE_EVENT1("media", "WebMediaPlayerImpl::BufferingHaveEnough", "id",
                 media_player_id_);
    // The SetReadyState() call below may clear
    // `skip_metrics_due_to_startup_suspend_` so report this first.
    if (!have_reported_time_to_play_ready_ &&
        !skip_metrics_due_to_startup_suspend_) {
      DCHECK(!for_suspended_start);
      have_reported_time_to_play_ready_ = true;
      const base::TimeDelta elapsed = base::TimeTicks::Now() - load_start_time_;
      media_metrics_provider_->SetTimeToPlayReady(elapsed);
      WriteSplitHistogram<kPlaybackType | kEncrypted>(
          &base::UmaHistogramMediumTimes, SplitHistogramName::kTimeToPlayReady,
          elapsed);
    }

    // Warning: This call may be re-entrant.
    SetReadyState(CanPlayThrough() ? WebMediaPlayer::kReadyStateHaveEnoughData
                                   : WebMediaPlayer::kReadyStateHaveFutureData);

    // Let the DataSource know we have enough data -- this is the only function
    // during which we advance to (or past) the kReadyStateHaveEnoughData state.
    // It may use this information to update buffer sizes or release unused
    // network connections.
    MaybeUpdateBufferSizesForPlayback();
    if (demuxer_manager_->HasDataSource() && !CouldPlayIfEnoughData()) {
      // For LazyLoad this will be handled during OnPipelineSuspended().
      if (for_suspended_start && did_lazy_load_)
        DCHECK(!have_enough_after_lazy_load_cb_.IsCancelled());
      else
        demuxer_manager_->OnBufferingHaveEnough(false);
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
    DCHECK_EQ(state, media::BUFFERING_HAVE_NOTHING);

    // Report the number of times we've entered the underflow state. Ensure we
    // only report the value when transitioning from HAVE_ENOUGH to
    // HAVE_NOTHING.
    if (ready_state_ == WebMediaPlayer::kReadyStateHaveEnoughData &&
        !seeking_) {
      underflow_timer_ = std::make_unique<base::ElapsedTimer>();
      watch_time_reporter_->OnUnderflow();

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
  if (state == media::BUFFERING_HAVE_NOTHING &&
      reason == media::DECODER_UNDERFLOW && smoothness_helper_) {
    smoothness_helper_->NotifyNNR();
  }

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnDurationChange() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return;

  client_->DurationChanged();
  DidMediaMetadataChange();
  demuxer_manager_->DurationChanged();

  if (watch_time_reporter_)
    watch_time_reporter_->OnDurationChanged(GetPipelineMediaDuration());
}

void WebMediaPlayerImpl::OnWaiting(media::WaitingReason reason) {
  DVLOG(2) << __func__ << ": reason=" << static_cast<int>(reason);
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  switch (reason) {
    case media::WaitingReason::kNoCdm:
    case media::WaitingReason::kNoDecryptionKey:
      has_waiting_for_key_ = true;
      media_metrics_provider_->SetHasWaitingForKey();
      encrypted_client_->DidBlockPlaybackWaitingForKey();
      // TODO(jrummell): didResumePlaybackBlockedForKey() should only be called
      // when a key has been successfully added (e.g. OnSessionKeysChange() with
      // `has_additional_usable_key` = true). http://crbug.com/461903
      encrypted_client_->DidResumePlaybackBlockedForKey();
      return;

    // Ideally this should be handled by PipelineController directly without
    // being proxied here. But currently Pipeline::Client (`this`) is passed to
    // PipelineImpl directly without going through `pipeline_controller_`,
    // making it difficult to do.
    // TODO(xhwang): Handle this in PipelineController when we have a clearer
    // picture on how to refactor WebMediaPlayerImpl, PipelineController and
    // PipelineImpl.
    case media::WaitingReason::kDecoderStateLost:
      pipeline_controller_->OnDecoderStateLost();
      return;

    // On Android, it happens when the surface used by the decoder is destroyed,
    // e.g. background. We want to suspend the pipeline and hope the surface
    // will be available when resuming the pipeline by some other signals.
    case media::WaitingReason::kSecureSurfaceLost:
      if (!pipeline_controller_->IsSuspended() && !pending_oneshot_suspend_) {
        pending_oneshot_suspend_ = true;
        UpdatePlayState();
      }
      return;
  }
}

void WebMediaPlayerImpl::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::kReadyStateHaveNothing);

  TRACE_EVENT0("media", "WebMediaPlayerImpl::OnVideoNaturalSizeChange");

  // The input `size` is from the decoded video frame, which is the original
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

void WebMediaPlayerImpl::OnVideoFrameRateChange(std::optional<int> fps) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (power_status_helper_)
    power_status_helper_->SetAverageFrameRate(fps);

  last_reported_fps_ = fps;
  UpdateSmoothnessHelper();
}

void WebMediaPlayerImpl::OnAudioConfigChange(
    const media::AudioDecoderConfig& config) {
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

void WebMediaPlayerImpl::OnVideoConfigChange(
    const media::VideoDecoderConfig& config) {
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

void WebMediaPlayerImpl::OnAudioPipelineInfoChange(
    const media::AudioPipelineInfo& info) {
  media_metrics_provider_->SetAudioPipelineInfo(info);
  if (info.decoder_type == audio_decoder_type_)
    return;

  audio_decoder_type_ = info.decoder_type;

  // If there's no current reporter, there's nothing to be done.
  if (!watch_time_reporter_)
    return;

  UpdateSecondaryProperties();
}

void WebMediaPlayerImpl::OnVideoPipelineInfoChange(
    const media::VideoPipelineInfo& info) {
  media_metrics_provider_->SetVideoPipelineInfo(info);
  if (info.decoder_type == video_decoder_type_)
    return;

  video_decoder_type_ = info.decoder_type;

  // If there's no current reporter, there's nothing to be done.
  if (!watch_time_reporter_)
    return;

  UpdateSecondaryProperties();
}

void WebMediaPlayerImpl::OnPageHidden() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Backgrounding a video requires a user gesture to resume playback.
  if (IsPageHidden()) {
    video_locked_when_paused_when_hidden_ = true;
  }

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
                     base::Unretained(compositor_.get()), !IsPageHidden()));
}

void WebMediaPlayerImpl::SuspendForFrameClosed() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  was_suspended_for_frame_closed_ = true;
  UpdateBackgroundVideoOptimizationState();
  UpdatePlayState();
}

void WebMediaPlayerImpl::OnPageShown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  background_pause_timer_.Stop();

  // Foreground videos don't require user gesture to continue playback.
  video_locked_when_paused_when_hidden_ = false;

  was_suspended_for_frame_closed_ = false;

  if (watch_time_reporter_)
    watch_time_reporter_->OnShown();

  if (video_decode_stats_reporter_)
    video_decode_stats_reporter_->OnShown();

  // Notify the compositor of our page visibility status.
  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameCompositor::SetIsPageVisible,
                     base::Unretained(compositor_.get()), !IsPageHidden()));

  // UpdateBackgroundVideoOptimizationState will set `visibility_pause_reason_`
  // to the updated correct value. However, we need to know the previous one to
  // decide if we should resume playback.
  bool was_paused_because_page_hidden = IsPausedBecausePageHidden();
  UpdateBackgroundVideoOptimizationState();

  if (!visibility_pause_reason_ && was_paused_because_page_hidden) {
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

void WebMediaPlayerImpl::OnFrameShown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  background_pause_timer_.Stop();

  // Foreground videos don't require user gesture to continue playback.
  video_locked_when_paused_when_hidden_ = false;

  was_suspended_for_frame_closed_ = false;

  if (watch_time_reporter_) {
    watch_time_reporter_->OnShown();
  }

  if (video_decode_stats_reporter_) {
    video_decode_stats_reporter_->OnShown();
  }

  UpdateBackgroundVideoOptimizationState();

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnFrameHidden() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Backgrounding a video requires a user gesture to resume playback.
  if (IsFrameHidden()) {
    video_locked_when_paused_when_hidden_ = true;
  }

  if (watch_time_reporter_) {
    watch_time_reporter_->OnHidden();
  }

  if (video_decode_stats_reporter_) {
    video_decode_stats_reporter_->OnHidden();
  }

  UpdateBackgroundVideoOptimizationState();
  UpdatePlayState();

  // Schedule suspended playing media to be paused if the user doesn't come back
  // to it within some timeout period to avoid any autoplay surprises.
  ScheduleIdlePauseTimer();
}

void WebMediaPlayerImpl::SetVolumeMultiplier(double multiplier) {
  volume_multiplier_ = multiplier;
  SetVolume(volume_);
}

void WebMediaPlayerImpl::SetPersistentState(bool value) {
  DVLOG(2) << __func__ << ": value=" << value;
  overlay_info_.is_persistent_video = value;
  MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::SetPowerExperimentState(bool state) {
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
  if (client_) {
    client_->OnRemotePlaybackDisabled(disabled);
  }
}

void WebMediaPlayerImpl::RequestMediaRemoting() {
  if (observer_) {
    observer_->OnMediaRemotingRequested();
  }
}

#if BUILDFLAG(IS_ANDROID)
void WebMediaPlayerImpl::FlingingStarted() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!disable_pipeline_auto_suspend_);
  disable_pipeline_auto_suspend_ = true;

  is_flinging_ = true;

  // Capabilities reporting should only be performed for local playbacks.
  video_decode_stats_reporter_.reset();

  // Requests to restart media pipeline. A flinging renderer will be created via
  // the `renderer_factory_selector_`.
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

void WebMediaPlayerImpl::OnRemotePlayStateChange(
    media::MediaStatus::State state) {
  DCHECK(is_flinging_);
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (state == media::MediaStatus::State::kPlaying && Paused()) {
    DVLOG(1) << __func__ << " requesting PLAY.";
    client_->ResumePlayback();
  } else if (state == media::MediaStatus::State::kPaused && !Paused()) {
    DVLOG(1) << __func__ << " requesting PAUSE.";
    client_->PausePlayback(
        WebMediaPlayerClient::PauseReason::kRemotePlayStateChange);
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

void WebMediaPlayerImpl::SetPoster(const WebURL& poster) {
  has_poster_ = !poster.IsEmpty();
}

void WebMediaPlayerImpl::MemoryDataSourceInitialized(bool success,
                                                     size_t data_size) {
  if (success) {
    // Replace the loaded url with an empty data:// URL since it may be large.
    demuxer_manager_->SetLoadedUrl(GURL("data:,"));

    // Mark all the data as buffered.
    buffered_data_source_host_->SetTotalBytes(data_size);
    buffered_data_source_host_->AddBufferedByteRange(0, data_size);
  }
  DataSourceInitialized(success);
}

void WebMediaPlayerImpl::DataSourceInitialized(bool success) {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (!success) {
    SetNetworkState(WebMediaPlayer::kNetworkStateFormatError);
    media_metrics_provider_->OnError(media::PIPELINE_ERROR_NETWORK);

    // Not really necessary, since the pipeline was never started, but it at
    // least this makes sure that the error handling code is in sync.
    UpdatePlayState();

    return;
  }

  StartPipeline();
}

void WebMediaPlayerImpl::MultiBufferDataSourceInitialized(bool success) {
  DVLOG(1) << __func__;
  DCHECK(demuxer_manager_->HasDataSource());
  if (observer_) {
    observer_->OnDataSourceInitialized(
        demuxer_manager_->GetDataSourceUrlAfterRedirects().value());
  }

  // No point in preloading data as we'll probably just throw it away anyways.
  if (success && IsStreaming() && preload_ > media::DataSource::METADATA)
    demuxer_manager_->SetPreload(media::DataSource::METADATA);
  DataSourceInitialized(success);
}

void WebMediaPlayerImpl::OnDataSourceRedirected() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

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
  // TODO(liberato): `token` should already be a RoutingToken.
  overlay_routing_token_is_pending_ = false;
  overlay_routing_token_ = media::OverlayInfo::RoutingToken(token);
  MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::OnOverlayInfoRequested(
    bool decoder_requires_restart_for_overlay,
    media::ProvideOverlayInfoCB provide_overlay_info_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // If we get a non-null cb, a decoder is initializing and requires overlay
  // info. If we get a null cb, a previously initialized decoder is
  // unregistering for overlay info updates.
  if (!provide_overlay_info_cb) {
    decoder_requires_restart_for_overlay_ = false;
    provide_overlay_info_cb_.Reset();
    return;
  }

  // If `decoder_requires_restart_for_overlay` is true, we must restart the
  // pipeline for fullscreen transitions. The decoder is unable to switch
  // surfaces otherwise. If false, we simply need to tell the decoder about the
  // new surface and it will handle things seamlessly.
  // For encrypted video we pretend that the decoder doesn't require a restart
  // because it needs an overlay all the time anyway. We'll switch into
  // `always_enable_overlays_` mode below.
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
  // case where `!overlay_enabled_`, since we want to tell the decoder to avoid
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

std::unique_ptr<media::Renderer> WebMediaPlayerImpl::CreateRenderer(
    std::optional<media::RendererType> renderer_type) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Make sure that overlays are enabled if they're always allowed.
  if (always_enable_overlays_)
    EnableOverlay();

  media::RequestOverlayInfoCB request_overlay_info_cb;
#if BUILDFLAG(IS_ANDROID)
  request_overlay_info_cb =
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &WebMediaPlayerImpl::OnOverlayInfoRequested, weak_this_));
#endif

  if (renderer_type) {
    DVLOG(1) << __func__
             << ": renderer_type=" << static_cast<int>(renderer_type.value());
    renderer_factory_selector_->SetBaseRendererType(renderer_type.value());
  }

  bool old_uses_audio_service = UsesAudioService(renderer_type_);
  renderer_type_ = renderer_factory_selector_->GetCurrentRendererType();

  // TODO(crbug/1426179): Support codec changing for Media Foundation.
  if (renderer_type_ == media::RendererType::kMediaFoundation) {
    demuxer_manager_->DisableDemuxerCanChangeType();
  }

  bool new_uses_audio_service = UsesAudioService(renderer_type_);
  if (new_uses_audio_service != old_uses_audio_service)
    client_->DidUseAudioServiceChange(new_uses_audio_service);

  media_metrics_provider_->SetRendererType(renderer_type_);
  media_log_->SetProperty<MediaLogProperty::kRendererName>(renderer_type_);

  return renderer_factory_selector_->GetCurrentFactory()->CreateRenderer(
      media_task_runner_, worker_task_runner_, audio_source_provider_.get(),
      compositor_.get(), std::move(request_overlay_info_cb),
      client_->TargetColorSpace());
}

std::optional<media::DemuxerType> WebMediaPlayerImpl::GetDemuxerType() const {
  // Note: this can't be a ternary expression because the compiler throws a fit
  // over type conversions.
  if (demuxer_manager_) {
    return demuxer_manager_->GetDemuxerType();
  }
  return std::nullopt;
}

media::PipelineStatus WebMediaPlayerImpl::OnDemuxerCreated(
    Demuxer* demuxer,
    media::Pipeline::StartType start_type,
    bool is_streaming,
    bool is_static) {
  CHECK_NE(demuxer, nullptr);
  switch (demuxer->GetDemuxerType()) {
    case media::DemuxerType::kMediaUrlDemuxer: {
      using_media_player_renderer_ = true;
      video_decode_stats_reporter_.reset();
      break;
    }
    default: {
      seeking_ = true;
      break;
    }
  }

  if (start_type != media::Pipeline::StartType::kNormal) {
    attempting_suspended_start_ = true;
  }

  pipeline_controller_->Start(start_type, demuxer, this, is_streaming,
                              is_static);
  return media::OkStatus();
}

void WebMediaPlayerImpl::StartPipeline() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameCompositor::SetOnNewProcessedFrameCallback,
                     base::Unretained(compositor_.get()),
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &WebMediaPlayerImpl::OnFirstFrame, weak_this_))));
  base::flat_map<std::string, std::string> headers;
  // Referer is the right spelling of the HTTP header, not Referrer.
  headers[net::HttpRequestHeaders::kReferer] =
      net::URLRequestJob::ComputeReferrerForPolicy(
          frame_->GetDocument().GetReferrerPolicy(),
          GURL(frame_->GetDocument().OutgoingReferrer().Utf8()),
          demuxer_manager_->LoadedUrl())
          .spec();

  // base::Unretained(this) is safe here, since |CreateDemuxer| calls the bound
  // method directly and immediately.
  auto create_demuxer_error = demuxer_manager_->CreateDemuxer(
      load_type_ == kLoadTypeMediaSource, preload_, needs_first_frame_,
      base::BindOnce(&WebMediaPlayerImpl::OnDemuxerCreated,
                     base::Unretained(this)),
      std::move(headers));

  if (!create_demuxer_error.is_ok()) {
    return OnError(std::move(create_demuxer_error));
  }
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

  if (state == WebMediaPlayer::kReadyStateHaveEnoughData &&
      demuxer_manager_->DataSourceFullyBuffered() &&
      network_state_ == WebMediaPlayer::kNetworkStateLoading) {
    SetNetworkState(WebMediaPlayer::kNetworkStateLoaded);
  }

  ready_state_ = state;
  highest_ready_state_ = std::max(highest_ready_state_, ready_state_);

  // Always notify to ensure client has the latest value.
  client_->ReadyStateChanged();
}

scoped_refptr<WebAudioSourceProviderImpl>
WebMediaPlayerImpl::GetAudioSourceProvider() {
  return audio_source_provider_;
}

scoped_refptr<media::VideoFrame>
WebMediaPlayerImpl::GetCurrentFrameFromCompositor() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl::GetCurrentFrameFromCompositor");

  // We can't copy from protected frames.
  if (cdm_context_ref_)
    return nullptr;

  // Can be null.
  scoped_refptr<media::VideoFrame> video_frame =
      compositor_->GetCurrentFrameOnAnyThread();

  // base::Unretained is safe here because `compositor_` is destroyed on
  // `vfc_task_runner_`. The destruction is queued from `this`' destructor,
  // which also runs on `main_task_runner_`, which makes it impossible for
  // UpdateCurrentFrameIfStale() to be queued after `compositor_`'s dtor.
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
    if (!at_beginning || GetPipelineMediaDuration() == media::kInfiniteDuration)
      can_auto_suspend = false;
  }

  bool is_suspended = pipeline_controller_->IsSuspended();
  bool is_backgrounded = IsBackgroundSuspendEnabled(this) && IsPageHidden();
  PlayState state = UpdatePlayState_ComputePlayState(
      is_flinging_, can_auto_suspend, is_suspended, is_backgrounded,
      IsInPictureInPicture());
  SetDelegateState(state.delegate_state, state.is_idle);
  SetMemoryReportingState(state.is_memory_reporting_enabled);
  SetSuspendState(state.is_suspended || pending_suspend_resume_cycle_);
  if (power_status_helper_) {
    // Make sure that we're in something like steady-state before recording.
    power_status_helper_->SetIsPlaying(
        !paused_ && !seeking_ && !IsPageHidden() && !state.is_suspended &&
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
                                            current_time, ended_);

  if (!MediaPositionNeedsUpdate(media_position_state_, new_position))
    return;

  DVLOG(2) << __func__ << "(" << new_position.ToString() << ")";
  media_position_state_ = new_position;
  client_->DidPlayerMediaPositionStateChange(effective_playback_rate, duration,
                                             current_time, ended_);
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
      // When delegate get PlayerGone it removes all state, need to make sure
      // it is up-to-date before calling DidPlay.
      delegate_->DidMediaMetadataChange(delegate_id_, delegate_has_audio_,
                                        HasVideo(), GetMediaContentType());
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
    memory_usage_reporting_timer_.Start(FROM_HERE, base::Seconds(2), this,
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
WebMediaPlayerImpl::UpdatePlayState_ComputePlayState(
    bool is_flinging,
    bool can_auto_suspend,
    bool is_suspended,
    bool is_backgrounded,
    bool is_in_picture_in_picture) {
  PlayState result;

  bool must_suspend =
      was_suspended_for_frame_closed_ || pending_oneshot_suspend_;
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
  bool background_suspended = can_auto_suspend && is_backgrounded && paused_ &&
                              have_future_data && !is_in_picture_in_picture;

  // Idle suspension is allowed prior to kReadyStateHaveMetadata since there
  // exist mechanisms to exit the idle state when the player is capable of
  // reaching the kReadyStateHaveMetadata state; see didLoadingProgress().
  //
  // TODO(sandersd): Make the delegate suspend idle players immediately when
  // hidden.
  bool idle_suspended = can_auto_suspend && is_stale && paused_ && !seeking_ &&
                        !overlay_info_.is_fullscreen && !needs_first_frame_;

  // If we're already suspended, see if we can wait for user interaction. Prior
  // to kReadyStateHaveMetadata, we require `is_stale` to remain suspended.
  // `is_stale` will be cleared when we receive data which may take us to
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

  // We do not treat `playback_rate_` == 0 as paused. For the media session,
  // being paused implies displaying a play button, which is incorrect in this
  // case. For memory usage reporting, we just use the same definition (but we
  // don't have to).
  //
  // Similarly, we don't consider `ended_` to be paused. Blink will immediately
  // call pause() or seek(), so `ended_` should not affect the computation.
  // Despite that, `ended_` does result in a separate paused state, to simplfy
  // the contract for SetDelegateState().
  //
  // `has_remote_controls` indicates if the player can be controlled outside the
  // page (e.g. via the notification controls or by audio focus events). Idle
  // suspension does not destroy the media session, because we expect that the
  // notification controls (and audio focus) remain. With some exceptions for
  // background videos, the player only needs to have audio to have controls
  // (requires `have_current_data`).
  //
  // `alive` indicates if the player should be present (not `GONE`) to the
  // delegate, either paused or playing. The following must be true for the
  // player:
  //   - `have_current_data`, since playback can't begin before that point, we
  //     need to know whether we are paused to correctly configure the session,
  //     and also because the tracks and duration are passed to DidPlay(),
  //   - `is_flinging` is false (RemotePlayback is not handled by the delegate)
  //   - `has_error` is false as player should have no errors,
  //   - `background_suspended` is false, otherwise `has_remote_controls` must
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

void WebMediaPlayerImpl::MakeDemuxerThreadDumper(media::Demuxer* demuxer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!media_thread_mem_dumper_);

  // base::Unretained() is safe here. `demuxer` is owned by |demuxer_manager_|,
  // which is destroyed on the main thread, but before doing it
  // ~WebMediaPlayerImpl() posts a media thread task that deletes
  // |media_thread_mem_dumper_| and  waits for it to finish.
  media_thread_mem_dumper_ = std::make_unique<media::MemoryDumpProviderProxy>(
      "WebMediaPlayer_MediaThread", media_task_runner_,
      base::BindRepeating(&WebMediaPlayerImpl::OnMediaThreadMemoryDump,
                          media_player_id_, base::Unretained(demuxer)));
}

bool WebMediaPlayerImpl::CouldPlayIfEnoughData() {
  return client_->CouldPlayIfEnoughData();
}

bool WebMediaPlayerImpl::IsMediaPlayerRendererClient() {
  // MediaPlayerRendererClientFactory is the only factory that a uses
  // MediaResource::Type::URL for the moment.
  return renderer_factory_selector_->GetCurrentFactory()
             ->GetRequiredMediaResourceType() ==
         media::MediaResource::Type::KUrl;
}

void WebMediaPlayerImpl::ReportMemoryUsage() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // About base::Unretained() usage below: We destroy `demuxer_manager_` on the
  // main thread.  Before that, however, ~WebMediaPlayerImpl() posts a task to
  // the media thread and waits for it to finish.  Hence, the GetMemoryUsage()
  // task posted here must finish earlier.
  //
  // The exception to the above is when OnError() has been called. If we're in
  // the error state we've already shut down the pipeline and can't rely on it
  // to cycle the media thread before we destroy `demuxer_manager_`. In this
  // case skip collection of the demuxer memory stats.
  if (demuxer_manager_ && !IsNetworkStateError(network_state_)) {
    demuxer_manager_->RespondToDemuxerMemoryUsageReport(base::BindOnce(
        &WebMediaPlayerImpl::FinishMemoryUsageReport, weak_this_));
  } else {
    FinishMemoryUsageReport(0);
  }
}

void WebMediaPlayerImpl::FinishMemoryUsageReport(int64_t demuxer_memory_usage) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  const auto stats = GetPipelineStatistics();
  const int64_t data_source_memory_usage =
      demuxer_manager_->GetDataSourceMemoryUsage();

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
           ? media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420,
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
  external_memory_accounter_.Update(isolate_.get(), delta);
}

void WebMediaPlayerImpl::OnMainThreadMemoryDump(
    media::MediaPlayerLoggingID id,
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  const auto stats = GetPipelineStatistics();
  auto player_node_name =
      base::StringPrintf("media/webmediaplayer/player_0x%x", id);
  auto* player_node = pmd->CreateAllocatorDump(player_node_name);
  player_node->AddScalar(
      base::trace_event::MemoryAllocatorDump::kNameObjectCount,
      base::trace_event::MemoryAllocatorDump::kUnitsObjects, 1);

  if (args.level_of_detail !=
      base::trace_event::MemoryDumpLevelOfDetail::kBackground) {
    bool suspended = pipeline_controller_->IsPipelineSuspended();
    auto player_state =
        base::StringPrintf("Paused: %d Ended: %d ReadyState: %d Suspended: %d",
                           paused_, ended_, GetReadyState(), suspended);
    player_node->AddString("player_state", "", player_state);
  }

  CreateAllocation(pmd, id, "audio", stats.audio_memory_usage);
  CreateAllocation(pmd, id, "video", stats.video_memory_usage);

  if (demuxer_manager_->HasDataSource()) {
    CreateAllocation(pmd, id, "data_source",
                     demuxer_manager_->GetDataSourceMemoryUsage());
  }
}

// static
void WebMediaPlayerImpl::OnMediaThreadMemoryDump(
    media::MediaPlayerLoggingID id,
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
  if ((paused_ && !IsPausedBecausePageHidden()) ||
      !pipeline_controller_->IsSuspended() || !HasAudio()) {
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  // Don't pause videos casted as part of RemotePlayback.
  if (is_flinging_)
    return;
#endif

  // Idle timeout chosen arbitrarily.
  background_pause_timer_.Start(
      FROM_HERE, base::Seconds(5),
      base::BindOnce(
          &WebMediaPlayerClient::PausePlayback, base::Unretained(client_),
          WebMediaPlayerClient::PauseReason::kSuspendedPlayerIdleTimeout));
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
  watch_time_reporter_ = std::make_unique<WatchTimeReporter>(
      media::mojom::PlaybackProperties::New(
          pipeline_metadata_.has_audio, has_video, false, false,
          GetDemuxerType() == media::DemuxerType::kChunkDemuxer, is_encrypted_,
          embedded_media_experience_enabled_,
          media::mojom::MediaStreamType::kNone, renderer_type_),
      pipeline_metadata_.natural_size,
      base::BindRepeating(&WebMediaPlayerImpl::GetCurrentTimeInternal,
                          base::Unretained(this)),
      base::BindRepeating(&WebMediaPlayerImpl::GetPipelineStatistics,
                          base::Unretained(this)),
      media_metrics_provider_.get(),
      frame_->GetTaskRunner(TaskType::kInternalMedia));
  watch_time_reporter_->OnVolumeChange(volume_);
  watch_time_reporter_->OnDurationChanged(GetPipelineMediaDuration());

  if (delegate_->IsPageHidden()) {
    watch_time_reporter_->OnHidden();
  } else {
    watch_time_reporter_->OnShown();
  }

  if (client_->HasNativeControls())
    watch_time_reporter_->OnNativeControlsEnabled();
  else
    watch_time_reporter_->OnNativeControlsDisabled();

  switch (client_->GetDisplayType()) {
    case DisplayType::kInline:
      watch_time_reporter_->OnDisplayTypeInline();
      break;
    case DisplayType::kFullscreen:
      watch_time_reporter_->OnDisplayTypeFullscreen();
      break;
    case DisplayType::kPictureInPicture:
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
      media::mojom::SecondaryPlaybackProperties::New(
          pipeline_metadata_.audio_decoder_config.codec(),
          pipeline_metadata_.video_decoder_config.codec(),
          pipeline_metadata_.audio_decoder_config.profile(),
          pipeline_metadata_.video_decoder_config.profile(),
          audio_decoder_type_, video_decoder_type_,
          pipeline_metadata_.audio_decoder_config.encryption_scheme(),
          pipeline_metadata_.video_decoder_config.encryption_scheme(),
          pipeline_metadata_.natural_size));
}

bool WebMediaPlayerImpl::IsPageHidden() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return delegate_->IsPageHidden() && !was_suspended_for_frame_closed_;
}

bool WebMediaPlayerImpl::IsFrameHidden() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return delegate_->IsFrameHidden() && !was_suspended_for_frame_closed_;
}

bool WebMediaPlayerImpl::IsPausedBecausePageHidden() const {
  return visibility_pause_reason_ &&
         visibility_pause_reason_ ==
             WebMediaPlayerClient::PauseReason::kPageHidden;
}

bool WebMediaPlayerImpl::IsPausedBecauseFrameHidden() const {
  return visibility_pause_reason_ &&
         visibility_pause_reason_ ==
             WebMediaPlayerClient::PauseReason::kFrameHidden;
}

bool WebMediaPlayerImpl::IsStreaming() const {
  return demuxer_manager_->IsStreaming();
}

bool WebMediaPlayerImpl::DoesOverlaySupportMetadata() const {
  return pipeline_metadata_.video_decoder_config.video_transformation() ==
         media::kNoTransformation;
}

void WebMediaPlayerImpl::UpdateRemotePlaybackCompatibility(bool is_compatible) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  client_->RemotePlaybackCompatibilityChanged(demuxer_manager_->LoadedUrl(),
                                              is_compatible);
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

std::optional<viz::SurfaceId> WebMediaPlayerImpl::GetSurfaceId() {
  if (!surface_layer_for_video_enabled_)
    return std::nullopt;
  return bridge_->GetSurfaceId();
}

void WebMediaPlayerImpl::RequestVideoFrameCallback() {
  // If the first frame hasn't been received, kick off a request to generate one
  // since we may not always do so for hidden preload=metadata playbacks.
  if (!has_first_frame_) {
    OnBecameVisible();
  }

  compositor_->SetOnFramePresentedCallback(
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &WebMediaPlayerImpl::OnNewFramePresentedCallback, weak_this_)));
}

void WebMediaPlayerImpl::OnNewFramePresentedCallback() {
  client_->OnRequestVideoFrameCallback();
}

std::unique_ptr<WebMediaPlayer::VideoFramePresentationMetadata>
WebMediaPlayerImpl::GetVideoFramePresentationMetadata() {
  return compositor_->GetLastPresentedFrameMetadata();
}

void WebMediaPlayerImpl::UpdateFrameIfStale() {
  // base::Unretained is safe here because `compositor_` is destroyed on
  // `vfc_task_runner_`. The destruction is queued from `this`' destructor,
  // which also runs on `main_task_runner_`, which makes it impossible for
  // UpdateCurrentFrameIfStale() to be queued after `compositor_`'s dtor.
  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameCompositor::UpdateCurrentFrameIfStale,
                     base::Unretained(compositor_.get()),
                     VideoFrameCompositor::UpdateType::kBypassClient));
}

base::WeakPtr<WebMediaPlayer> WebMediaPlayerImpl::AsWeakPtr() {
  return weak_this_;
}

bool WebMediaPlayerImpl::ShouldPausePlaybackWhenHidden() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (should_pause_when_frame_is_hidden_ && IsFrameHidden()) {
    return true;
  }

  const bool preserve_audio =
      should_pause_background_muted_audio_
          ? HasUnmutedAudio() || audio_source_provider_->IsAudioBeingCaptured()
          : HasAudio();

  // Audio only stream is allowed to play when in background.
  if (!HasVideo() && preserve_audio)
    return false;

  // MediaPlayer always signals audio and video, so use an empty natural size to
  // determine if there's really video or not.
  if (using_media_player_renderer_ &&
      pipeline_metadata_.natural_size.IsEmpty() && preserve_audio) {
    return false;
  }

  // PiP is the only exception when background video playback is disabled.
  if (HasVideo() && IsInPictureInPicture())
    return false;

  // This takes precedent over every restriction except PiP.
  if (!is_background_video_playback_enabled_)
    return true;

  if (is_flinging_)
    return false;

  // If suspending background video, pause any video that's not unlocked to play
  // in the background.
  if (IsBackgroundSuspendEnabled(this)) {
    return !preserve_audio || (IsResumeBackgroundVideosEnabled() &&
                               video_locked_when_paused_when_hidden_);
  }

  if (HasVideo() && IsVideoBeingCaptured())
    return false;

  return !preserve_audio;
}

bool WebMediaPlayerImpl::ShouldDisableVideoWhenHidden() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (!is_background_video_track_optimization_supported_) {
    return false;
  }

  // Only disable the video track on audio + video playbacks, otherwise they
  // should be paused or left alone.
  if (!HasVideo() || !HasAudio()) {
    return false;
  }

  // Disabling tracks causes seeks which can cause problematic network delays
  // on streaming resources.
  if (IsStreaming()) {
    return false;
  }

  // In these cases something external needs the frames.
  if (IsInPictureInPicture() || IsVideoBeingCaptured() || is_flinging_) {
    return false;
  }

  // Videos shorter than the maximum allowed keyframe distance can be optimized.
  base::TimeDelta duration = GetPipelineMediaDuration();
  if (duration < kMaxKeyframeDistanceToDisableBackgroundVideo) {
    return true;
  }

  // Otherwise, only optimize videos with shorter average keyframe distance.
  auto stats = GetPipelineStatistics();
  return stats.video_keyframe_distance_average <
         kMaxKeyframeDistanceToDisableBackgroundVideo;
}

void WebMediaPlayerImpl::UpdateBackgroundVideoOptimizationState() {
  if (IsPageHidden() ||
      (IsFrameHidden() && should_pause_when_frame_is_hidden_)) {
    if (ShouldPausePlaybackWhenHidden()) {
      update_background_status_cb_.Cancel();
      is_background_status_change_cancelled_ = true;
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
          base::Seconds(10));
    }
  } else {
    update_background_status_cb_.Cancel();
    is_background_status_change_cancelled_ = true;
    // There no visibility-related reason to pause the video.
    visibility_pause_reason_.reset();

    EnableVideoTrackIfNeeded();
  }
}

void WebMediaPlayerImpl::PauseVideoIfNeeded() {
  DCHECK(IsPageHidden() || IsFrameHidden());

  // Don't pause video while the pipeline is stopped, resuming or seeking.
  // Also if the video is paused already.
  if (!pipeline_controller_->IsPipelineRunning() || is_pipeline_resuming_ ||
      seeking_ || paused_)
    return;

  auto pause_reason = WebMediaPlayerClient::PauseReason::kPageHidden;
  if (IsFrameHidden() && should_pause_when_frame_is_hidden_) {
    pause_reason = WebMediaPlayerClient::PauseReason::kFrameHidden;
  }

  // client_->PausePlayback() will get `visibility_pause_reason_` set to
  // std::nullopt and UpdatePlayState() called, so set
  // `visibility_pause_reason_` to the correct value after and then return.
  // TODO(crbug.com/351354996): To avoid resetting `visibility_pause_reason_`,
  // we should plumb the pause reason from here all the way through to
  // `WebMediaPlayerImpl::Pause`, where the reset is done.
  client_->PausePlayback(pause_reason);
  visibility_pause_reason_ = pause_reason;
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
      SelectedVideoTrackChanged(client_->GetSelectedVideoTrackId());
    }
  }
}

void WebMediaPlayerImpl::DisableVideoTrackIfNeeded() {
  DCHECK(IsPageHidden() || IsFrameHidden());

  // Don't change video track while the pipeline is resuming or seeking.
  if (is_pipeline_resuming_ || seeking_)
    return;

  if (!video_track_disabled_ && ShouldDisableVideoWhenHidden()) {
    video_track_disabled_ = true;
    SelectedVideoTrackChanged(std::nullopt);
  }
}

void WebMediaPlayerImpl::SetPipelineStatisticsForTest(
    const media::PipelineStatistics& stats) {
  pipeline_statistics_for_test_ = std::make_optional(stats);
}

media::PipelineStatistics WebMediaPlayerImpl::GetPipelineStatistics() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_statistics_for_test_.value_or(
      pipeline_controller_->GetStatistics());
}

void WebMediaPlayerImpl::SetPipelineMediaDurationForTest(
    base::TimeDelta duration) {
  pipeline_media_duration_for_test_ = std::make_optional(duration);
}

base::TimeDelta WebMediaPlayerImpl::GetPipelineMediaDuration() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_media_duration_for_test_.value_or(
      pipeline_controller_->GetMediaDuration());
}

media::MediaContentType WebMediaPlayerImpl::GetMediaContentType() const {
  return media::DurationToMediaContentType(GetPipelineMediaDuration());
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
  // the `renderer_factory_selector_`.
  ScheduleRestart();
  if (client_) {
    client_->MediaRemotingStarted(
        WebString::FromUTF8(remote_device_friendly_name));
  }
}

void WebMediaPlayerImpl::SwitchToLocalRenderer(
    media::MediaObserverClient::ReasonToSwitchToLocal reason) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!is_remote_rendering_)
    return;  // Is currently with local renderer.
  is_remote_rendering_ = false;

  DCHECK(disable_pipeline_auto_suspend_);
  disable_pipeline_auto_suspend_ = false;

  // Capabilities reporting may resume now that playback is local.
  CreateVideoDecodeStatsReporter();

  // Requests to restart media pipeline. A local renderer will be created via
  // the `renderer_factory_selector_`.
  ScheduleRestart();
  if (client_)
    client_->MediaRemotingStopped(GetSwitchToLocalMessage(reason));
}

template <uint32_t Flags, typename... T>
void WebMediaPlayerImpl::WriteSplitHistogram(
    void (*UmaFunction)(const std::string&, T...),
    SplitHistogramName key,
    const T&... values) {
  std::string strkey = std::string(GetHistogramName(key));

  if constexpr (Flags & kEncrypted) {
    if (is_encrypted_)
      UmaFunction(strkey + ".EME", values...);
  }

  if constexpr (Flags & kTotal)
    UmaFunction(strkey + ".All", values...);

  if constexpr (Flags & kPlaybackType) {
    auto demuxer_type = GetDemuxerType();
    if (!demuxer_type.has_value())
      return;
    switch (*demuxer_type) {
      case media::DemuxerType::kChunkDemuxer:
        UmaFunction(strkey + ".MSE", values...);
        break;
      case media::DemuxerType::kManifestDemuxer:
      case media::DemuxerType::kMediaUrlDemuxer:
        UmaFunction(strkey + ".HLS", values...);
        break;
      default:
        UmaFunction(strkey + ".SRC", values...);
        break;
    }
  }
}

void WebMediaPlayerImpl::RecordUnderflowDuration(base::TimeDelta duration) {
  DCHECK(demuxer_manager_->HasDataSource() ||
         GetDemuxerType() == media::DemuxerType::kChunkDemuxer ||
         GetDemuxerType() == media::DemuxerType::kManifestDemuxer);
  WriteSplitHistogram<kPlaybackType | kEncrypted>(
      &base::UmaHistogramTimes, SplitHistogramName::kUnderflowDuration2,
      duration);
}

void WebMediaPlayerImpl::RecordVideoNaturalSize(const gfx::Size& natural_size) {
  // Always report video natural size to MediaLog.
  media_log_->AddEvent<MediaLogEvent::kVideoSizeChanged>(natural_size);
  media_log_->SetProperty<MediaLogProperty::kResolution>(natural_size);

  if (initial_video_height_recorded_)
    return;

  initial_video_height_recorded_ = true;

  int height = natural_size.height();

  WriteSplitHistogram<kPlaybackType | kEncrypted | kTotal>(
      &base::UmaHistogramCustomCounts, SplitHistogramName::kVideoHeightInitial,
      height, 100, 10000, size_t{50});

  if (playback_events_recorder_)
    playback_events_recorder_->OnNaturalSizeChanged(natural_size);
}

void WebMediaPlayerImpl::SetTickClockForTest(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
  buffered_data_source_host_->SetTickClockForTest(tick_clock);
}

void WebMediaPlayerImpl::OnFirstFrame(base::TimeTicks frame_time,
                                      bool is_frame_readable) {
  DCHECK(!load_start_time_.is_null());
  DCHECK(!skip_metrics_due_to_startup_suspend_);

  has_first_frame_ = true;
  needs_first_frame_ = false;
  is_frame_readable_ = is_frame_readable;

  const base::TimeDelta elapsed = frame_time - load_start_time_;
  media_metrics_provider_->SetTimeToFirstFrame(elapsed);
  WriteSplitHistogram<kPlaybackType | kEncrypted>(
      &base::UmaHistogramMediumTimes, SplitHistogramName::kTimeToFirstFrame,
      elapsed);

  media::PipelineStatistics ps = GetPipelineStatistics();
  if (client_) {
    client_->OnFirstFrame(frame_time, ps.video_bytes_decoded);

    // Needed to signal HTMLVideoElement that it should remove the poster image.
    if (has_poster_) {
      client_->Repaint();
    }
  }
}

void WebMediaPlayerImpl::RecordEncryptionScheme(
    const std::string& stream_name,
    media::EncryptionScheme encryption_scheme) {
  DCHECK(stream_name == "Audio" || stream_name == "Video");

  // If the stream is not encrypted, don't record it.
  if (encryption_scheme == media::EncryptionScheme::kUnencrypted)
    return;

  base::UmaHistogramEnumeration(
      "Media.EME.EncryptionScheme.Initial." + stream_name,
      DetermineEncryptionSchemeUMAValue(encryption_scheme),
      EncryptionSchemeUMA::kCount);
}

bool WebMediaPlayerImpl::IsInPictureInPicture() const {
  DCHECK(client_);
  return client_->GetDisplayType() == DisplayType::kPictureInPicture;
}

void WebMediaPlayerImpl::MaybeSetContainerNameForMetrics() {
  // Pipeline startup failed before even getting a demuxer setup.
  if (!demuxer_manager_->HasDemuxer()) {
    return;
  }

  // Container has already been set.
  if (highest_ready_state_ >= WebMediaPlayer::kReadyStateHaveMetadata)
    return;

  // Only report metrics for demuxers that provide container information.
  auto container = demuxer_manager_->GetContainerForMetrics();
  if (container.has_value())
    media_metrics_provider_->SetContainerName(container.value());
}

void WebMediaPlayerImpl::MaybeUpdateBufferSizesForPlayback() {
  // Don't increase the MultiBufferDataSource buffer size until we've reached
  // kReadyStateHaveEnoughData. Otherwise we will unnecessarily slow down
  // playback startup -- it can instead be done for free after playback starts.
  if (highest_ready_state_ < kReadyStateHaveEnoughData) {
    return;
  }

  demuxer_manager_->OnDataSourcePlaybackRateChange(playback_rate_, paused_);
}

void WebMediaPlayerImpl::OnSimpleWatchTimerTick() {
  if (playback_events_recorder_)
    playback_events_recorder_->OnPipelineStatistics(GetPipelineStatistics());
}

GURL WebMediaPlayerImpl::GetSrcAfterRedirects() {
  return demuxer_manager_->GetDataSourceUrlAfterRedirects().value_or(GURL());
}

void WebMediaPlayerImpl::UpdateSmoothnessHelper() {
  // If the experiment flag is off, then do nothing.
  if (!base::FeatureList::IsEnabled(media::kMediaLearningSmoothnessExperiment))
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
  features.push_back(learning::FeatureValue(
      static_cast<int>(pipeline_metadata_.video_decoder_config.codec())));
  features.push_back(learning::FeatureValue(
      pipeline_metadata_.video_decoder_config.profile()));
  features.push_back(
      learning::FeatureValue(pipeline_metadata_.natural_size.width()));
  features.push_back(learning::FeatureValue(*last_reported_fps_));

  // If we have a smoothness helper, and we're not changing the features, then
  // do nothing.  This prevents restarting the helper for no reason.
  if (smoothness_helper_ && features == smoothness_helper_->features())
    return;

  // Create or restart the smoothness helper with `features`.
  smoothness_helper_ = SmoothnessHelper::Create(
      GetLearningTaskController(learning::tasknames::kConsecutiveBadWindows),
      GetLearningTaskController(learning::tasknames::kConsecutiveNNRs),
      features, this);
}

std::unique_ptr<learning::LearningTaskController>
WebMediaPlayerImpl::GetLearningTaskController(const char* task_name) {
  // Get the LearningTaskController for `task_id`.
  learning::LearningTask task = learning::MediaLearningTasks::Get(task_name);
  DCHECK_EQ(task.name, task_name);

  mojo::Remote<learning::mojom::LearningTaskController> remote_ltc;
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

bool WebMediaPlayerImpl::IsVideoBeingCaptured() const {
  // 5 seconds chosen arbitrarily since most videos are never captured.
  return tick_clock_->NowTicks() - last_frame_request_time_ < base::Seconds(5);
}

void WebMediaPlayerImpl::RegisterFrameSinkHierarchy() {
  if (bridge_)
    bridge_->RegisterFrameSinkHierarchy();
}

void WebMediaPlayerImpl::UnregisterFrameSinkHierarchy() {
  if (bridge_)
    bridge_->UnregisterFrameSinkHierarchy();
}

void WebMediaPlayerImpl::RecordVideoOcclusionState(
    std::string_view occlusion_state) {
  media_log_->AddEvent<MediaLogEvent::kVideoOcclusionState>(
      std::string(occlusion_state));
}

void WebMediaPlayerImpl::ReportSessionUMAs() const {
  if (renderer_type_ != media::RendererType::kRendererImpl &&
      renderer_type_ != media::RendererType::kMediaFoundation) {
    return;
  }

  // Report the `Media.DroppedFrameCount2.{RendererType}.{EncryptedOrClear}`
  // UMA.
  constexpr char kDroppedFrameUmaPrefix[] = "Media.DroppedFrameCount2.";
  std::string uma_name = kDroppedFrameUmaPrefix;
  uma_name += GetRendererName(renderer_type_);
  if (is_encrypted_)
    uma_name += ".Encrypted";
  else
    uma_name += ".Clear";
  base::UmaHistogramCounts1M(uma_name, DroppedFrameCount());

  if (!is_encrypted_) {
    // Report the `Media.FrameReadBackCount.{RendererType}` UMA.
    constexpr char kFrameReadBackUmaPrefix[] = "Media.FrameReadBackCount.";
    uma_name = kFrameReadBackUmaPrefix;
    uma_name += GetRendererName(renderer_type_);
    base::UmaHistogramCounts10M(uma_name, video_frame_readback_count_);
  }

  if (cdm_config_) {
    // Report the `Media.EME.{KeySystem}.{Robustness}.WaitingForKey` UMA.
    auto key_system_name_for_uma = media::GetKeySystemNameForUMA(
        cdm_config_->key_system, cdm_config_->use_hw_secure_codecs);
    uma_name = "Media.EME." + key_system_name_for_uma + ".WaitingForKey";
    base::UmaHistogramBoolean(uma_name, has_waiting_for_key_);
  }
}

bool WebMediaPlayerImpl::PassedTimingAllowOriginCheck() const {
  return demuxer_manager_->PassedDataSourceTimingAllowOriginCheck();
}

void WebMediaPlayerImpl::DidMediaMetadataChange() {
  media::MediaContentType content_type = GetMediaContentType();
  bool is_encrypted_media =
      pipeline_metadata_.audio_decoder_config.is_encrypted() ||
      pipeline_metadata_.video_decoder_config.is_encrypted();

  client_->DidMediaMetadataChange(
      delegate_has_audio_, HasVideo(),
      pipeline_metadata_.audio_decoder_config.codec(),
      pipeline_metadata_.video_decoder_config.codec(), content_type,
      is_encrypted_media);

  delegate_->DidMediaMetadataChange(delegate_id_, delegate_has_audio_,
                                    HasVideo(), content_type);
}

}  // namespace blink
