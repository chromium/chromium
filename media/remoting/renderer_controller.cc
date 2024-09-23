// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/renderer_controller.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/remoting_constants.h"
#include "media/remoting/metrics.h"

#if BUILDFLAG(IS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

namespace media {
namespace remoting {

using mojom::RemotingSinkAudioCapability;
using mojom::RemotingSinkFeature;
using mojom::RemotingSinkVideoCapability;

namespace {

constexpr int kPixelsPerSec4k = 3840 * 2160 * 30;  // 4k 30fps.
constexpr int kPixelsPerSec2k = 1920 * 1080 * 30;  // 1080p 30fps.

StopTrigger GetStopTrigger(mojom::RemotingStopReason reason) {
  switch (reason) {
    case mojom::RemotingStopReason::ROUTE_TERMINATED:
      return ROUTE_TERMINATED;
    case mojom::RemotingStopReason::SOURCE_GONE:
      return MEDIA_ELEMENT_DESTROYED;
    case mojom::RemotingStopReason::MESSAGE_SEND_FAILED:
      return MESSAGE_SEND_FAILED;
    case mojom::RemotingStopReason::DATA_SEND_FAILED:
      return DATA_SEND_FAILED;
    case mojom::RemotingStopReason::UNEXPECTED_FAILURE:
      return UNEXPECTED_FAILURE;
    case mojom::RemotingStopReason::SERVICE_GONE:
      return SERVICE_GONE;
    case mojom::RemotingStopReason::USER_DISABLED:
      return USER_DISABLED;
    case mojom::RemotingStopReason::LOCAL_PLAYBACK:
      // This RemotingStopReason indicates the RendererController initiated the
      // session shutdown in the immediate past, and the trigger for that should
      // have already been recorded in the metrics. Here, this is just duplicate
      // feedback from the sink for that same event. Return UNKNOWN_STOP_TRIGGER
      // because this reason can not be a stop trigger and it would be a logic
      // flaw for this value to be recorded in the metrics.
      return UNKNOWN_STOP_TRIGGER;
  }

  return UNKNOWN_STOP_TRIGGER;  // To suppress compiler warning on Windows.
}

MediaObserverClient::ReasonToSwitchToLocal GetSwitchReason(
    StopTrigger stop_trigger) {
  switch (stop_trigger) {
    case FRAME_DROP_RATE_HIGH:
    case PACING_TOO_SLOWLY:
      return MediaObserverClient::ReasonToSwitchToLocal::POOR_PLAYBACK_QUALITY;
    case EXITED_FULLSCREEN:
    case BECAME_AUXILIARY_CONTENT:
    case DISABLED_BY_PAGE:
    case USER_DISABLED:
    case UNKNOWN_STOP_TRIGGER:
      return MediaObserverClient::ReasonToSwitchToLocal::NORMAL;
    case UNSUPPORTED_AUDIO_CODEC:
    case UNSUPPORTED_VIDEO_CODEC:
    case UNSUPPORTED_AUDIO_AND_VIDEO_CODECS:
    case DECRYPTION_ERROR:
    case RECEIVER_INITIALIZE_FAILED:
    case RECEIVER_PIPELINE_ERROR:
    case PEERS_OUT_OF_SYNC:
    case RPC_INVALID:
    case DATA_PIPE_CREATE_ERROR:
    case DATA_PIPE_WRITE_ERROR:
    case MESSAGE_SEND_FAILED:
    case DATA_SEND_FAILED:
    case UNEXPECTED_FAILURE:
      return MediaObserverClient::ReasonToSwitchToLocal::PIPELINE_ERROR;
    case MOJO_DISCONNECTED:
    case ROUTE_TERMINATED:
    case MEDIA_ELEMENT_DESTROYED:
    case MEDIA_ELEMENT_FROZEN:
    case START_RACE:
    case SERVICE_GONE:
      return MediaObserverClient::ReasonToSwitchToLocal::ROUTE_TERMINATED;
  }

  // To suppress compiler warning on Windows.
  return MediaObserverClient::ReasonToSwitchToLocal::ROUTE_TERMINATED;
}

}  // namespace

RendererController::RendererController(
    mojo::PendingReceiver<mojom::RemotingSource> source_receiver,
    mojo::PendingRemote<mojom::Remoter> remoter)
#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
    : rpc_messenger_([this](std::vector<uint8_t> message) {
        SendMessageToSink(std::move(message));
      }),
#else
    :
#endif
      receiver_(this, std::move(source_receiver)),
      remoter_(std::move(remoter)),
      clock_(base::DefaultTickClock::GetInstance()) {
  DCHECK(remoter_);
}

RendererController::~RendererController() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetClient(nullptr);
}

void RendererController::OnSinkAvailable(
    mojom::RemotingSinkMetadataPtr metadata) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sink_metadata_ = std::move(metadata);

  UpdateAndMaybeSwitch(SINK_AVAILABLE, UNKNOWN_STOP_TRIGGER);
}

void RendererController::OnSinkGone() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Prevent the clients to start any future remoting sessions. Won't affect the
  // behavior of the currently-running session (if any).
  sink_metadata_ = nullptr;
}

void RendererController::OnStarted() {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Remoting started successively.";
  if (remote_rendering_started_ && client_) {
    metrics_recorder_.DidStartSession();
    client_->SwitchToRemoteRenderer(sink_metadata_->friendly_name);
  }
}

void RendererController::OnStartFailed(mojom::RemotingStartFailReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Failed to start remoting:" << reason;
  is_media_remoting_requested_ = false;
  if (remote_rendering_started_) {
    metrics_recorder_.WillStopSession(START_RACE);
    metrics_recorder_.StartSessionFailed(reason);
    remote_rendering_started_ = false;
  }
}

void RendererController::OnStopped(mojom::RemotingStopReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Remoting stopped: " << reason;
  is_media_remoting_requested_ = false;
  OnSinkGone();
  UpdateAndMaybeSwitch(UNKNOWN_START_TRIGGER, GetStopTrigger(reason));
}

void RendererController::OnMessageFromSink(
    const std::vector<uint8_t>& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
  std::unique_ptr<openscreen::cast::RpcMessage> rpc(
      new openscreen::cast::RpcMessage());
  if (!rpc->ParseFromArray(message.data(), message.size())) {
    VLOG(1) << "corrupted Rpc message";
    OnSinkGone();
    UpdateAndMaybeSwitch(UNKNOWN_START_TRIGGER, RPC_INVALID);
    return;
  }

  rpc_messenger_.ProcessMessageFromRemote(std::move(rpc));
#endif
}

void RendererController::OnBecameDominantVisibleContent(bool is_dominant) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (is_dominant_content_ == is_dominant)
    return;
  is_dominant_content_ = is_dominant;
  // Reset the errors when the media element stops being the dominant visible
  // content in the tab.
  if (!is_dominant_content_)
    encountered_renderer_fatal_error_ = false;
  UpdateAndMaybeSwitch(BECAME_DOMINANT_CONTENT, BECAME_AUXILIARY_CONTENT);
}

void RendererController::OnRemotePlaybackDisabled(bool disabled) {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_remote_playback_disabled_ = disabled;
  metrics_recorder_.OnRemotePlaybackDisabled(disabled);
  UpdateAndMaybeSwitch(ENABLED_BY_PAGE, DISABLED_BY_PAGE);
}

void RendererController::OnMediaRemotingRequested() {
  is_media_remoting_requested_ = true;
  UpdateAndMaybeSwitch(REQUESTED_BY_BROWSER, UNKNOWN_STOP_TRIGGER);
}

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
openscreen::WeakPtr<openscreen::cast::RpcMessenger>
RendererController::GetRpcMessenger() {
  DCHECK(thread_checker_.CalledOnValidThread());

  return rpc_messenger_.GetWeakPtr();
}
#endif

void RendererController::StartDataPipe(uint32_t data_pipe_capacity,
                                       bool audio,
                                       bool video,
                                       DataPipeStartCallback done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!done_callback.is_null());

  bool ok = audio || video;

  mojo::ScopedDataPipeProducerHandle audio_producer_handle;
  mojo::ScopedDataPipeConsumerHandle audio_consumer_handle;
  if (ok && audio) {
    ok &= mojo::CreateDataPipe(data_pipe_capacity, audio_producer_handle,
                               audio_consumer_handle) == MOJO_RESULT_OK;
  }

  mojo::ScopedDataPipeProducerHandle video_producer_handle;
  mojo::ScopedDataPipeConsumerHandle video_consumer_handle;
  if (ok && video) {
    ok &= mojo::CreateDataPipe(data_pipe_capacity, video_producer_handle,
                               video_consumer_handle) == MOJO_RESULT_OK;
  }

  if (!ok) {
    VLOG(1) << "No audio nor video to establish data pipe";
    std::move(done_callback)
        .Run(mojo::NullRemote(), mojo::NullRemote(),
             mojo::ScopedDataPipeProducerHandle(),
             mojo::ScopedDataPipeProducerHandle());
    return;
  }

  mojo::PendingRemote<mojom::RemotingDataStreamSender> audio_stream_sender;
  mojo::PendingRemote<mojom::RemotingDataStreamSender> video_stream_sender;
  remoter_->StartDataStreams(
      std::move(audio_consumer_handle), std::move(video_consumer_handle),
      audio ? audio_stream_sender.InitWithNewPipeAndPassReceiver()
            : mojo::NullReceiver(),
      video ? video_stream_sender.InitWithNewPipeAndPassReceiver()
            : mojo::NullReceiver());
  std::move(done_callback)
      .Run(std::move(audio_stream_sender), std::move(video_stream_sender),
           std::move(audio_producer_handle), std::move(video_producer_handle));
}

void RendererController::OnMetadataChanged(const PipelineMetadata& metadata) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const bool was_audio_codec_supported = has_audio() && IsAudioCodecSupported();
  const bool was_video_codec_supported = has_video() && IsVideoCodecSupported();
  const bool natural_size_changed =
      pipeline_metadata_.natural_size != metadata.natural_size;
  pipeline_metadata_ = metadata;
  const bool is_audio_codec_supported = has_audio() && IsAudioCodecSupported();
  const bool is_video_codec_supported = has_video() && IsVideoCodecSupported();
  metrics_recorder_.OnPipelineMetadataChanged(metadata);

  StartTrigger start_trigger = UNKNOWN_START_TRIGGER;
  if (!was_audio_codec_supported && is_audio_codec_supported)
    start_trigger = SUPPORTED_AUDIO_CODEC;
  if (!was_video_codec_supported && is_video_codec_supported) {
    start_trigger = start_trigger == SUPPORTED_AUDIO_CODEC
                        ? SUPPORTED_AUDIO_AND_VIDEO_CODECS
                        : SUPPORTED_VIDEO_CODEC;
  }
  StopTrigger stop_trigger = UNKNOWN_STOP_TRIGGER;
  if (was_audio_codec_supported && !is_audio_codec_supported)
    stop_trigger = UNSUPPORTED_AUDIO_CODEC;
  if (was_video_codec_supported && !is_video_codec_supported) {
    stop_trigger = stop_trigger == UNSUPPORTED_AUDIO_CODEC
                       ? UNSUPPORTED_AUDIO_AND_VIDEO_CODECS
                       : UNSUPPORTED_VIDEO_CODEC;
  }

  // Reset and calculate pixel rate again when video size changes.
  if (natural_size_changed) {
    pixel_rate_timer_.Stop();
    pixels_per_second_ = 0;
    MaybeStartCalculatePixelRateTimer();
  }

  UpdateRemotePlaybackAvailabilityMonitoringState();

  UpdateAndMaybeSwitch(start_trigger, stop_trigger);
}

void RendererController::OnDataSourceInitialized(
    const GURL& url_after_redirects) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (url_after_redirects == url_after_redirects_)
    return;

  // TODO(avayvod): Does WMPI update MediaObserver when metadata becomes
  // invalid or should we reset it here?
  url_after_redirects_ = url_after_redirects;

  UpdateRemotePlaybackAvailabilityMonitoringState();
}

void RendererController::OnHlsManifestDetected() {
  is_hls_ = true;
  // TODO(crbug.com/40057824) Android used to rely solely on MediaPlayer for HLS
  // playback, but now there is an alternative native player. Should we still
  // be doing this in all cases? It does work in its current state, on both
  // android and desktop, but it is not thoroughly tested.
  UpdateRemotePlaybackAvailabilityMonitoringState();
}

void RendererController::UpdateRemotePlaybackAvailabilityMonitoringState() {
// Currently RemotePlayback-initated media remoting only supports URL flinging
// thus the source is supported when the URL is either http or https, video and
// audio codecs are supported by the remote playback device; HLS is playable by
// Chrome on Android (which is not detected by the pipeline metadata atm).
// On Desktop, `sink_metadata_` is empty until a streaming session has been
// established. So it's not possible to check if the receiver device supports
// the media's codec.
#if BUILDFLAG(IS_ANDROID)
  const bool is_media_supported = is_hls_ || IsRemotePlaybackSupported();
#else
  const bool is_media_supported =
      !pipeline_metadata_.video_decoder_config.is_encrypted() &&
      !pipeline_metadata_.audio_decoder_config.is_encrypted();
#endif
  // TODO(avayvod): add a check for CORS.
  bool is_source_supported = url_after_redirects_.has_scheme() &&
                             (url_after_redirects_.SchemeIs("http") ||
                              url_after_redirects_.SchemeIs("https") ||
                              url_after_redirects_.SchemeIs("file")) &&
                             is_media_supported;
  if (client_)
    client_->UpdateRemotePlaybackCompatibility(is_source_supported);
}

bool RendererController::IsVideoCodecSupported() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetVideoCompatibility() == RemotingCompatibility::kCompatible;
}

bool RendererController::IsAudioCodecSupported() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetAudioCompatibility() == RemotingCompatibility::kCompatible;
}

void RendererController::OnPlaying() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_paused_ = false;
  UpdateAndMaybeSwitch(PLAY_COMMAND, UNKNOWN_STOP_TRIGGER);
  MaybeStartCalculatePixelRateTimer();
}

void RendererController::OnPaused() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_paused_ = true;
  // Cancel the timer since pixel rate cannot be calculated when media is
  // paused.
  pixel_rate_timer_.Stop();
}

void RendererController::OnFrozen() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_paused_);

  // If the element is frozen we want to stop remoting.
  UpdateAndMaybeSwitch(UNKNOWN_START_TRIGGER, MEDIA_ELEMENT_FROZEN);
}

RemotingCompatibility RendererController::GetVideoCompatibility() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_video());

  // Media Remoting doesn't support encrypted media.
  if (pipeline_metadata_.video_decoder_config.is_encrypted())
    return RemotingCompatibility::kEncryptedVideo;

  bool compatible = false;
  switch (pipeline_metadata_.video_decoder_config.codec()) {
    case VideoCodec::kH264:
      compatible = HasVideoCapability(RemotingSinkVideoCapability::CODEC_H264);
      break;
    case VideoCodec::kVP8:
      compatible = HasVideoCapability(RemotingSinkVideoCapability::CODEC_VP8);
      break;
    case VideoCodec::kVP9:
      compatible = HasVideoCapability(RemotingSinkVideoCapability::CODEC_VP9);
      break;
    case VideoCodec::kHEVC:
      compatible = HasVideoCapability(RemotingSinkVideoCapability::CODEC_HEVC);
      break;
    default:
      VLOG(2) << "Remoting does not support video codec: "
              << pipeline_metadata_.video_decoder_config.codec();
  }
  return compatible ? RemotingCompatibility::kCompatible
                    : RemotingCompatibility::kIncompatibleVideoCodec;
}

RemotingCompatibility RendererController::GetAudioCompatibility() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_audio());

  // Media Remoting doesn't support encrypted media.
  if (pipeline_metadata_.audio_decoder_config.is_encrypted())
    return RemotingCompatibility::kEncryptedAudio;

  bool compatible = false;
  switch (pipeline_metadata_.audio_decoder_config.codec()) {
    case AudioCodec::kAAC:
      compatible = HasAudioCapability(RemotingSinkAudioCapability::CODEC_AAC);
      break;
    case AudioCodec::kOpus:
      compatible = HasAudioCapability(RemotingSinkAudioCapability::CODEC_OPUS);
      break;
    case AudioCodec::kMP3:
    case AudioCodec::kPCM:
    case AudioCodec::kVorbis:
    case AudioCodec::kFLAC:
    case AudioCodec::kAMR_NB:
    case AudioCodec::kAMR_WB:
    case AudioCodec::kPCM_MULAW:
    case AudioCodec::kGSM_MS:
    case AudioCodec::kPCM_S16BE:
    case AudioCodec::kPCM_S24BE:
    case AudioCodec::kEAC3:
    case AudioCodec::kPCM_ALAW:
    case AudioCodec::kALAC:
    case AudioCodec::kAC3:
    case AudioCodec::kDTS:
    case AudioCodec::kDTSXP2:
    case AudioCodec::kDTSE:
      compatible =
          HasAudioCapability(RemotingSinkAudioCapability::CODEC_BASELINE_SET);
      break;
    default:
      VLOG(2) << "Remoting does not support audio codec: "
              << pipeline_metadata_.audio_decoder_config.codec();
  }
  return compatible ? RemotingCompatibility::kCompatible
                    : RemotingCompatibility::kIncompatibleAudioCodec;
}

RemotingCompatibility RendererController::GetCompatibility() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(client_);

  if (is_remote_playback_disabled_)
    return RemotingCompatibility::kDisabledByPage;

  if (!has_video() && !has_audio())
    return RemotingCompatibility::kNoAudioNorVideo;

  // When `is_media_remoting_requested_`, it is guaranteed that the sink is
  // compatible and the media element meets the minimum duration requirement. So
  // there's no need to check for compatibilities.
  if (is_media_remoting_requested_) {
    return RemotingCompatibility::kCompatible;
  }

  if (client_->Duration() <= kMinMediaDurationForSwitchingToRemotingInSec) {
    return RemotingCompatibility::kDurationBelowThreshold;
  }

  if (has_video()) {
    RemotingCompatibility compatibility = GetVideoCompatibility();
    if (compatibility != RemotingCompatibility::kCompatible)
      return compatibility;
  }

  if (has_audio()) {
    RemotingCompatibility compatibility = GetAudioCompatibility();
    if (compatibility != RemotingCompatibility::kCompatible)
      return compatibility;
  }
  return RemotingCompatibility::kCompatible;
}

bool RendererController::IsAudioOrVideoSupported() const {
  return ((has_audio() || has_video()) &&
          (!has_video() || IsVideoCodecSupported()) &&
          (!has_audio() || IsAudioCodecSupported()));
}

void RendererController::UpdateAndMaybeSwitch(StartTrigger start_trigger,
                                              StopTrigger stop_trigger) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool should_be_remoting = ShouldBeRemoting();

  if (should_be_remoting == remote_rendering_started_) {
    return;
  }

  // Stop Remoting.
  if (!should_be_remoting) {
    remote_rendering_started_ = false;
    is_media_remoting_requested_ = false;
    DCHECK_NE(UNKNOWN_STOP_TRIGGER, stop_trigger);
    metrics_recorder_.WillStopSession(stop_trigger);
    if (client_)
      client_->SwitchToLocalRenderer(GetSwitchReason(stop_trigger));
    VLOG(2) << "Request to stop remoting: stop_trigger=" << stop_trigger;
    remoter_->Stop(mojom::RemotingStopReason::LOCAL_PLAYBACK);
    return;
  }

  // Start remoting. First, check pixel rate compatibility.
  if (pixels_per_second_ == 0) {
    MaybeStartCalculatePixelRateTimer();
    return;
  }

  auto pixel_rate_support = GetPixelRateSupport();
  metrics_recorder_.RecordVideoPixelRateSupport(pixel_rate_support);
  if (pixel_rate_support == PixelRateSupport::k2kSupported ||
      pixel_rate_support == PixelRateSupport::k4kSupported) {
    remote_rendering_started_ = true;
    DCHECK_NE(UNKNOWN_START_TRIGGER, start_trigger);
    metrics_recorder_.WillStartSession(start_trigger);
    // |MediaObserverClient::SwitchToRemoteRenderer()| will be called after
    // remoting is started successfully.
    if (is_media_remoting_requested_) {
      remoter_->StartWithPermissionAlreadyGranted();
    } else {
      remoter_->Start();
    }
  }
}

void RendererController::OnRendererFatalError(StopTrigger stop_trigger) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Do not act on errors caused by things like Mojo pipes being closed during
  // shutdown.
  if (!remote_rendering_started_)
    return;

  // MOJO_DISCONNECTED means the streaming session has stopped, which is not a
  // fatal error and should not prevent future sessions.
  // Clean sinks so that `UpdateAndMaybeSwitch` will stop remoting.
  if (stop_trigger != StopTrigger::MOJO_DISCONNECTED) {
    encountered_renderer_fatal_error_ = true;
    OnSinkGone();
  }

  UpdateAndMaybeSwitch(UNKNOWN_START_TRIGGER, stop_trigger);
}

void RendererController::SetClient(MediaObserverClient* client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  client_ = client;
  // Reset `encountered_renderer_fatal_error_` when the media element changes so
  // that the previous renderer fatal error won't prevent Remoting the new
  // content.
  encountered_renderer_fatal_error_ = false;
  if (!client_) {
    pixel_rate_timer_.Stop();
    if (remote_rendering_started_) {
      metrics_recorder_.WillStopSession(MEDIA_ELEMENT_DESTROYED);
      remoter_->Stop(mojom::RemotingStopReason::UNEXPECTED_FAILURE);
      remote_rendering_started_ = false;
    }
  }
}

bool RendererController::HasVideoCapability(
    mojom::RemotingSinkVideoCapability capability) const {
  return sink_metadata_ &&
         base::Contains(sink_metadata_->video_capabilities, capability);
}

bool RendererController::HasAudioCapability(
    mojom::RemotingSinkAudioCapability capability) const {
  return sink_metadata_ &&
         base::Contains(sink_metadata_->audio_capabilities, capability);
}

bool RendererController::HasFeatureCapability(
    RemotingSinkFeature capability) const {
  return sink_metadata_ && base::Contains(sink_metadata_->features, capability);
}

bool RendererController::SinkSupportsRemoting() const {
  // when `is_media_remoting_requested_` is true, all discovered sinks are
  // compatible with this media content.
  if (is_media_remoting_requested_) {
    return !sink_metadata_.is_null();
  }
  return HasFeatureCapability(RemotingSinkFeature::RENDERING);
}

bool RendererController::ShouldBeRemoting() {
  // Starts remote rendering when the media is the dominant content or the
  // browser has sent an explicit request.
  if (!is_dominant_content_ && !is_media_remoting_requested_) {
    return false;
  }
  // Only switch to remoting when media is playing. Since the renderer is
  // created when video starts loading, the receiver would display a black
  // screen if switching to remoting while paused. Thus, the user experience is
  // improved by not starting remoting until playback resumes.
  if (is_paused_ || encountered_renderer_fatal_error_ || !client_ ||
      !SinkSupportsRemoting()) {
    return false;
  }

  const RemotingCompatibility compatibility = GetCompatibility();
  metrics_recorder_.RecordCompatibility(compatibility);
  return compatibility == RemotingCompatibility::kCompatible;
}

void RendererController::MaybeStartCalculatePixelRateTimer() {
  if (pixel_rate_timer_.IsRunning() || !client_ || is_paused_ ||
      pixels_per_second_ != 0) {
    return;
  }

  pixel_rate_timer_.Start(
      FROM_HERE, base::Seconds(kPixelRateCalInSec),
      base::BindOnce(&RendererController::DoCalculatePixelRate,
                     base::Unretained(this), client_->DecodedFrameCount(),
                     clock_->NowTicks()));
}

void RendererController::DoCalculatePixelRate(
    int decoded_frame_count_before_delay,
    base::TimeTicks delayed_start_time) {
  DCHECK(client_);
  DCHECK(!is_paused_);

  base::TimeDelta elapsed = clock_->NowTicks() - delayed_start_time;
  const double frame_rate =
      (client_->DecodedFrameCount() - decoded_frame_count_before_delay) /
      elapsed.InSecondsF();
  pixels_per_second_ = frame_rate * pipeline_metadata_.natural_size.GetArea();
  UpdateAndMaybeSwitch(PIXEL_RATE_READY, UNKNOWN_STOP_TRIGGER);
}

PixelRateSupport RendererController::GetPixelRateSupport() const {
  DCHECK(pixels_per_second_ != 0);

  if (pixels_per_second_ <= kPixelsPerSec2k) {
    return PixelRateSupport::k2kSupported;
  }
  if (pixels_per_second_ <= kPixelsPerSec4k) {
    if (HasVideoCapability(mojom::RemotingSinkVideoCapability::SUPPORT_4K)) {
      return PixelRateSupport::k4kSupported;
    } else {
      return PixelRateSupport::k4kNotSupported;
    }
  }
  return PixelRateSupport::kOver4kNotSupported;
}

void RendererController::SendMessageToSink(std::vector<uint8_t> message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  remoter_->SendMessageToSink(message);
}

#if BUILDFLAG(IS_ANDROID)

bool RendererController::IsAudioRemotePlaybackSupported() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_audio());

  if (pipeline_metadata_.audio_decoder_config.is_encrypted())
    return false;

  switch (pipeline_metadata_.audio_decoder_config.codec()) {
    case AudioCodec::kAAC:
    case AudioCodec::kOpus:
    case AudioCodec::kMP3:
    case AudioCodec::kPCM:
    case AudioCodec::kVorbis:
    case AudioCodec::kFLAC:
    case AudioCodec::kAMR_NB:
    case AudioCodec::kAMR_WB:
    case AudioCodec::kPCM_MULAW:
    case AudioCodec::kGSM_MS:
    case AudioCodec::kPCM_S16BE:
    case AudioCodec::kPCM_S24BE:
    case AudioCodec::kEAC3:
    case AudioCodec::kPCM_ALAW:
    case AudioCodec::kALAC:
    case AudioCodec::kAC3:
    case AudioCodec::kDTS:
    case AudioCodec::kDTSXP2:
    case AudioCodec::kDTSE:
      return true;
    default:
      return false;
  }
}

bool RendererController::IsVideoRemotePlaybackSupported() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_video());

  if (pipeline_metadata_.video_decoder_config.is_encrypted())
    return false;

  switch (pipeline_metadata_.video_decoder_config.codec()) {
    case VideoCodec::kH264:
    case VideoCodec::kVP8:
    case VideoCodec::kVP9:
    case VideoCodec::kHEVC:
      return true;
    default:
      return false;
  }
}

bool RendererController::IsRemotePlaybackSupported() const {
  return ((has_audio() || has_video()) &&
          (!has_video() || IsVideoRemotePlaybackSupported()) &&
          (!has_audio() || IsAudioRemotePlaybackSupported()));
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace remoting
}  // namespace media
