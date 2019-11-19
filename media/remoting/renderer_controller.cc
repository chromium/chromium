// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/renderer_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

namespace media {
namespace remoting {

namespace {

// The duration to delay the start of media remoting to ensure all preconditions
// are held stable before switching to media remoting.
constexpr base::TimeDelta kDelayedStart = base::TimeDelta::FromSeconds(5);

constexpr int kPixelPerSec4K = 3840 * 2160 * 30;  // 4k 30fps.
constexpr int kPixelPerSec2K = 1920 * 1080 * 30;  // 1080p 30fps.

// The minimum media element duration that is allowed for media remoting.
// Frequent switching into and out of media remoting for short-duration media
// can feel "janky" to the user.
constexpr double kMinRemotingMediaDurationInSec = 60;

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
    case MOJO_PIPE_ERROR:
    case MESSAGE_SEND_FAILED:
    case DATA_SEND_FAILED:
    case UNEXPECTED_FAILURE:
      return MediaObserverClient::ReasonToSwitchToLocal::PIPELINE_ERROR;
    case ROUTE_TERMINATED:
    case MEDIA_ELEMENT_DESTROYED:
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
    : rpc_broker_(base::BindRepeating(&RendererController::SendMessageToSink,
                                      base::Unretained(this))),
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

  sink_metadata_ = *metadata;

  if (!HasFeatureCapability(mojom::RemotingSinkFeature::RENDERING)) {
    OnSinkGone();
    return;
  }
  UpdateAndMaybeSwitch(SINK_AVAILABLE, UNKNOWN_STOP_TRIGGER);
}

void RendererController::OnSinkGone() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Prevent the clients to start any future remoting sessions. Won't affect the
  // behavior of the currently-running session (if any).
  sink_metadata_ = mojom::RemotingSinkMetadata();
}

void RendererController::OnStarted() {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Remoting started successively.";
  if (remote_rendering_started_ && client_) {
    metrics_recorder_.DidStartSession();
    client_->SwitchToRemoteRenderer(sink_metadata_.friendly_name);
  }
}

void RendererController::OnStartFailed(mojom::RemotingStartFailReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Failed to start remoting:" << reason;
  if (remote_rendering_started_) {
    metrics_recorder_.WillStopSession(START_RACE);
    remote_rendering_started_ = false;
  }
}

void RendererController::OnStopped(mojom::RemotingStopReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Remoting stopped: " << reason;
  OnSinkGone();
  UpdateAndMaybeSwitch(UNKNOWN_START_TRIGGER, GetStopTrigger(reason));
}

void RendererController::OnMessageFromSink(
    const std::vector<uint8_t>& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  if (!rpc->ParseFromArray(message.data(), message.size())) {
    VLOG(1) << "corrupted Rpc message";
    OnSinkGone();
    UpdateAndMaybeSwitch(UNKNOWN_START_TRIGGER, RPC_INVALID);
    return;
  }

  rpc_broker_.ProcessMessageFromRemote(std::move(rpc));
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

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
base::WeakPtr<RpcBroker> RendererController::GetRpcBroker() {
  DCHECK(thread_checker_.CalledOnValidThread());

  return rpc_broker_.GetWeakPtr();
}
#endif

void RendererController::StartDataPipe(
    std::unique_ptr<mojo::DataPipe> audio_data_pipe,
    std::unique_ptr<mojo::DataPipe> video_data_pipe,
    DataPipeStartCallback done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!done_callback.is_null());

  bool audio = audio_data_pipe != nullptr;
  bool video = video_data_pipe != nullptr;
  if (!audio && !video) {
    LOG(ERROR) << "No audio nor video to establish data pipe";
    std::move(done_callback)
        .Run(mojom::RemotingDataStreamSenderPtrInfo(),
             mojom::RemotingDataStreamSenderPtrInfo(),
             mojo::ScopedDataPipeProducerHandle(),
             mojo::ScopedDataPipeProducerHandle());
    return;
  }
  mojom::RemotingDataStreamSenderPtr audio_stream_sender;
  mojom::RemotingDataStreamSenderPtr video_stream_sender;
  remoter_->StartDataStreams(audio ? std::move(audio_data_pipe->consumer_handle)
                                   : mojo::ScopedDataPipeConsumerHandle(),
                             video ? std::move(video_data_pipe->consumer_handle)
                                   : mojo::ScopedDataPipeConsumerHandle(),
                             audio ? mojo::MakeRequest(&audio_stream_sender)
                                   : mojom::RemotingDataStreamSenderRequest(),
                             video ? mojo::MakeRequest(&video_stream_sender)
                                   : mojom::RemotingDataStreamSenderRequest());
  std::move(done_callback)
      .Run(audio_stream_sender.PassInterface(),
           video_stream_sender.PassInterface(),
           audio ? std::move(audio_data_pipe->producer_handle)
                 : mojo::ScopedDataPipeProducerHandle(),
           video ? std::move(video_data_pipe->producer_handle)
                 : mojo::ScopedDataPipeProducerHandle());
}

void RendererController::OnMetadataChanged(const PipelineMetadata& metadata) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const bool was_audio_codec_supported = has_audio() && IsAudioCodecSupported();
  const bool was_video_codec_supported = has_video() && IsVideoCodecSupported();
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

void RendererController::UpdateRemotePlaybackAvailabilityMonitoringState() {
// Currently RemotePlayback-initated media remoting only supports URL flinging
// thus the source is supported when the URL is either http or https, video and
// audio codecs are supported by the remote playback device; HLS is playable by
// Chrome on Android (which is not detected by the pipeline metadata atm).
#if defined(OS_ANDROID)
  // TODO(tguilbert): Detect the presence of HLS based on demuxing results,
  // rather than the URL string. See crbug.com/663503.
  const bool is_media_supported =
      MediaCodecUtil::IsHLSURL(url_after_redirects_) ||
      IsRemotePlaybackSupported();
#else
  const bool is_media_supported = IsAudioOrVideoSupported();
#endif
  // TODO(avayvod): add a check for CORS.
  bool is_source_supported = url_after_redirects_.has_scheme() &&
                             (url_after_redirects_.SchemeIs("http") ||
                              url_after_redirects_.SchemeIs("https")) &&
                             is_media_supported;

  if (client_)
    client_->UpdateRemotePlaybackCompatibility(is_source_supported);
}

bool RendererController::IsVideoCodecSupported() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_video());

  // Media Remoting doesn't support encrypted media.
  if (pipeline_metadata_.video_decoder_config.is_encrypted())
    return false;

  switch (pipeline_metadata_.video_decoder_config.codec()) {
    case VideoCodec::kCodecH264:
      return HasVideoCapability(mojom::RemotingSinkVideoCapability::CODEC_H264);
    case VideoCodec::kCodecVP8:
      return HasVideoCapability(mojom::RemotingSinkVideoCapability::CODEC_VP8);
    case VideoCodec::kCodecVP9:
      return HasVideoCapability(mojom::RemotingSinkVideoCapability::CODEC_VP9);
    case VideoCodec::kCodecHEVC:
      return HasVideoCapability(mojom::RemotingSinkVideoCapability::CODEC_HEVC);
    default:
      VLOG(2) << "Remoting does not support video codec: "
              << pipeline_metadata_.video_decoder_config.codec();
      return false;
  }
}

bool RendererController::IsAudioCodecSupported() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_audio());

  // Media Remoting doesn't support encrypted media.
  if (pipeline_metadata_.audio_decoder_config.is_encrypted())
    return false;

  switch (pipeline_metadata_.audio_decoder_config.codec()) {
    case AudioCodec::kCodecAAC:
      return HasAudioCapability(mojom::RemotingSinkAudioCapability::CODEC_AAC);
    case AudioCodec::kCodecOpus:
      return HasAudioCapability(mojom::RemotingSinkAudioCapability::CODEC_OPUS);
    case AudioCodec::kCodecMP3:
    case AudioCodec::kCodecPCM:
    case AudioCodec::kCodecVorbis:
    case AudioCodec::kCodecFLAC:
    case AudioCodec::kCodecAMR_NB:
    case AudioCodec::kCodecAMR_WB:
    case AudioCodec::kCodecPCM_MULAW:
    case AudioCodec::kCodecGSM_MS:
    case AudioCodec::kCodecPCM_S16BE:
    case AudioCodec::kCodecPCM_S24BE:
    case AudioCodec::kCodecEAC3:
    case AudioCodec::kCodecPCM_ALAW:
    case AudioCodec::kCodecALAC:
    case AudioCodec::kCodecAC3:
      return HasAudioCapability(
          mojom::RemotingSinkAudioCapability::CODEC_BASELINE_SET);
    default:
      VLOG(2) << "Remoting does not support audio codec: "
              << pipeline_metadata_.audio_decoder_config.codec();
      return false;
  }
}

void RendererController::OnPlaying() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_paused_ = false;
  UpdateAndMaybeSwitch(PLAY_COMMAND, UNKNOWN_STOP_TRIGGER);
}

void RendererController::OnPaused() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_paused_ = true;
  // Cancel the start if in the middle of delayed start.
  CancelDelayedStart();
}

bool RendererController::CanBeRemoting() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!client_)
    return false;  // No way to switch to the remoting renderer.

  if (permanently_disable_remoting_)
    return false;

  if (!IsAudioOrVideoSupported())
    return false;

  if (is_remote_playback_disabled_)
    return false;

  if (client_->Duration() <= kMinRemotingMediaDurationInSec)
    return false;

  return true;
}

bool RendererController::IsAudioOrVideoSupported() const {
  return ((has_audio() || has_video()) &&
          (!has_video() || IsVideoCodecSupported()) &&
          (!has_audio() || IsAudioCodecSupported()));
}

void RendererController::UpdateAndMaybeSwitch(StartTrigger start_trigger,
                                              StopTrigger stop_trigger) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool should_be_remoting = CanBeRemoting();
  if (client_)
    client_->ActivateViewportIntersectionMonitoring(should_be_remoting);

  // Being the dominant visible content is the signal that starts remote
  // rendering.
  should_be_remoting &=
      (is_dominant_content_ && !encountered_renderer_fatal_error_);

  if ((remote_rendering_started_ ||
       delayed_start_stability_timer_.IsRunning()) == should_be_remoting) {
    return;
  }

  // Only switch to remoting when media is playing. Since the renderer is
  // created when video starts loading/playing, receiver will display a black
  // screen before video starts playing if switching to remoting when paused.
  // Thus, the user experience is improved by not starting remoting until
  // playback resumes.
  if (should_be_remoting && is_paused_)
    return;

  if (should_be_remoting) {
    WaitForStabilityBeforeStart(start_trigger);
  } else if (delayed_start_stability_timer_.IsRunning()) {
    DCHECK(!remote_rendering_started_);
    CancelDelayedStart();
  } else {
    remote_rendering_started_ = false;
    DCHECK_NE(UNKNOWN_STOP_TRIGGER, stop_trigger);
    metrics_recorder_.WillStopSession(stop_trigger);
    if (client_)
      client_->SwitchToLocalRenderer(GetSwitchReason(stop_trigger));
    VLOG(2) << "Request to stop remoting: stop_trigger=" << stop_trigger;
    remoter_->Stop(mojom::RemotingStopReason::LOCAL_PLAYBACK);
  }
}

void RendererController::WaitForStabilityBeforeStart(
    StartTrigger start_trigger) {
  DCHECK(!delayed_start_stability_timer_.IsRunning());
  DCHECK(!remote_rendering_started_);
  DCHECK(client_);

  delayed_start_stability_timer_.Start(
      FROM_HERE, kDelayedStart,
      base::BindRepeating(&RendererController::OnDelayedStartTimerFired,
                          base::Unretained(this), start_trigger,
                          client_->DecodedFrameCount(), clock_->NowTicks()));
}

void RendererController::CancelDelayedStart() {
  delayed_start_stability_timer_.Stop();
}

void RendererController::OnDelayedStartTimerFired(
    StartTrigger start_trigger,
    unsigned decoded_frame_count_before_delay,
    base::TimeTicks delayed_start_time) {
  DCHECK(is_dominant_content_);
  DCHECK(!remote_rendering_started_);
  DCHECK(client_);  // This task is canceled otherwise.

  base::TimeDelta elapsed = clock_->NowTicks() - delayed_start_time;
  DCHECK(!elapsed.is_zero());
  if (has_video()) {
    const double frame_rate =
        (client_->DecodedFrameCount() - decoded_frame_count_before_delay) /
        elapsed.InSecondsF();
    const double pixel_per_sec =
        frame_rate * pipeline_metadata_.natural_size.GetArea();
    if ((pixel_per_sec > kPixelPerSec4K) ||
        ((pixel_per_sec > kPixelPerSec2K) &&
         !HasVideoCapability(mojom::RemotingSinkVideoCapability::SUPPORT_4K))) {
      VLOG(1) << "Media remoting is not supported: frame_rate = " << frame_rate
              << " resolution = " << pipeline_metadata_.natural_size.ToString();
      permanently_disable_remoting_ = true;
      return;
    }
  }

  remote_rendering_started_ = true;
  DCHECK_NE(UNKNOWN_START_TRIGGER, start_trigger);
  metrics_recorder_.WillStartSession(start_trigger);
  // |MediaObserverClient::SwitchToRemoteRenderer()| will be called after
  // remoting is started successfully.
  remoter_->Start();
}

void RendererController::OnRendererFatalError(StopTrigger stop_trigger) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Do not act on errors caused by things like Mojo pipes being closed during
  // shutdown.
  if (!remote_rendering_started_)
    return;

  encountered_renderer_fatal_error_ = true;
  UpdateAndMaybeSwitch(UNKNOWN_START_TRIGGER, stop_trigger);
}

void RendererController::SetClient(MediaObserverClient* client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  client_ = client;
  if (!client_) {
    CancelDelayedStart();
    if (remote_rendering_started_) {
      metrics_recorder_.WillStopSession(MEDIA_ELEMENT_DESTROYED);
      remoter_->Stop(mojom::RemotingStopReason::UNEXPECTED_FAILURE);
      remote_rendering_started_ = false;
    }
    return;
  }

  client_->ActivateViewportIntersectionMonitoring(CanBeRemoting());
}

bool RendererController::HasVideoCapability(
    mojom::RemotingSinkVideoCapability capability) const {
  return std::find(std::begin(sink_metadata_.video_capabilities),
                   std::end(sink_metadata_.video_capabilities),
                   capability) != std::end(sink_metadata_.video_capabilities);
}

bool RendererController::HasAudioCapability(
    mojom::RemotingSinkAudioCapability capability) const {
  return std::find(std::begin(sink_metadata_.audio_capabilities),
                   std::end(sink_metadata_.audio_capabilities),
                   capability) != std::end(sink_metadata_.audio_capabilities);
}

bool RendererController::HasFeatureCapability(
    mojom::RemotingSinkFeature capability) const {
  return std::find(std::begin(sink_metadata_.features),
                   std::end(sink_metadata_.features),
                   capability) != std::end(sink_metadata_.features);
}

void RendererController::SendMessageToSink(
    std::unique_ptr<std::vector<uint8_t>> message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  remoter_->SendMessageToSink(*message);
}

#if defined(OS_ANDROID)

bool RendererController::IsAudioRemotePlaybackSupported() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_audio());

  if (pipeline_metadata_.audio_decoder_config.is_encrypted())
    return false;

  switch (pipeline_metadata_.audio_decoder_config.codec()) {
    case AudioCodec::kCodecAAC:
    case AudioCodec::kCodecOpus:
    case AudioCodec::kCodecMP3:
    case AudioCodec::kCodecPCM:
    case AudioCodec::kCodecVorbis:
    case AudioCodec::kCodecFLAC:
    case AudioCodec::kCodecAMR_NB:
    case AudioCodec::kCodecAMR_WB:
    case AudioCodec::kCodecPCM_MULAW:
    case AudioCodec::kCodecGSM_MS:
    case AudioCodec::kCodecPCM_S16BE:
    case AudioCodec::kCodecPCM_S24BE:
    case AudioCodec::kCodecEAC3:
    case AudioCodec::kCodecPCM_ALAW:
    case AudioCodec::kCodecALAC:
    case AudioCodec::kCodecAC3:
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
    case VideoCodec::kCodecH264:
    case VideoCodec::kCodecVP8:
    case VideoCodec::kCodecVP9:
    case VideoCodec::kCodecHEVC:
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

#endif  // defined(OS_ANDROID)

}  // namespace remoting
}  // namespace media
