// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_metrics_provider.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/learning/mojo/mojo_learning_task_controller_service.h"
#include "media/mojo/services/video_decode_stats_recorder.h"
#include "media/mojo/services/watch_time_recorder.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

#if !defined(OS_ANDROID)
#include "media/filters/decrypting_video_decoder.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_FUCHSIA) || (BUILDFLAG(IS_CHROMECAST) && defined(OS_ANDROID))
#include "media/mojo/services/playback_events_recorder.h"
#endif

namespace media {

constexpr char kInvalidInitialize[] = "Initialize() was not called correctly.";

static uint64_t g_player_id = 0;

MediaMetricsProvider::PipelineInfo::PipelineInfo(bool is_incognito)
    : is_incognito(is_incognito) {}

MediaMetricsProvider::PipelineInfo::~PipelineInfo() = default;

MediaMetricsProvider::MediaMetricsProvider(
    BrowsingMode is_incognito,
    FrameStatus is_top_frame,
    ukm::SourceId source_id,
    learning::FeatureValue origin,
    VideoDecodePerfHistory::SaveCallback save_cb,
    GetLearningSessionCallback learning_session_cb,
    RecordAggregateWatchTimeCallback record_playback_cb)
    : player_id_(g_player_id++),
      is_top_frame_(is_top_frame == FrameStatus::kTopFrame),
      source_id_(source_id),
      origin_(origin),
      save_cb_(std::move(save_cb)),
      learning_session_cb_(std::move(learning_session_cb)),
      record_playback_cb_(std::move(record_playback_cb)),
      uma_info_(is_incognito == BrowsingMode::kIncognito) {}

MediaMetricsProvider::~MediaMetricsProvider() {
  // These UKM and UMA metrics do not apply to MediaStreams.
  if (media_stream_type_ != mojom::MediaStreamType::kNone)
    return;

  // UKM may be unavailable in content_shell or other non-chrome/ builds; it
  // may also be unavailable if browser shutdown has started; so this may be a
  // nullptr. If it's unavailable, UKM reporting will be skipped.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder || !initialized_)
    return;

  ukm::builders::Media_WebMediaPlayerState builder(source_id_);
  builder.SetPlayerID(player_id_);
  builder.SetIsTopFrame(is_top_frame_);
  builder.SetIsEME(uma_info_.is_eme);
  builder.SetIsMSE(is_mse_);
  builder.SetFinalPipelineStatus(uma_info_.last_pipeline_status);
  if (!is_mse_) {
    builder.SetURLScheme(static_cast<int64_t>(url_scheme_));
    if (container_name_)
      builder.SetContainerName(*container_name_);
  }

  if (time_to_metadata_ != kNoTimestamp)
    builder.SetTimeToMetadata(time_to_metadata_.InMilliseconds());
  if (time_to_first_frame_ != kNoTimestamp)
    builder.SetTimeToFirstFrame(time_to_first_frame_.InMilliseconds());
  if (time_to_play_ready_ != kNoTimestamp)
    builder.SetTimeToPlayReady(time_to_play_ready_.InMilliseconds());

  builder.Record(ukm_recorder);
  ReportPipelineUMA();
}

std::string MediaMetricsProvider::GetUMANameForAVStream(
    const PipelineInfo& player_info) {
  constexpr char kPipelineUmaPrefix[] = "Media.PipelineStatus.AudioVideo.";
  std::string uma_name = kPipelineUmaPrefix;
  if (player_info.video_codec == kCodecVP8)
    uma_name += "VP8.";
  else if (player_info.video_codec == kCodecVP9)
    uma_name += "VP9.";
  else if (player_info.video_codec == kCodecH264)
    uma_name += "H264.";
  else if (player_info.video_codec == kCodecAV1)
    uma_name += "AV1.";
  else
    return uma_name + "Other";

#if !defined(OS_ANDROID)
  if (player_info.video_pipeline_info.decoder_type ==
      VideoDecoderType::kDecrypting)
    return uma_name + "DVD";
#endif

  if (player_info.video_pipeline_info.has_decrypting_demuxer_stream)
    uma_name += "DDS.";

  // Note that HW essentially means 'platform' anyway. MediaCodec has been
  // reported as HW forever, regardless of the underlying platform
  // implementation.
  uma_name += player_info.video_pipeline_info.is_platform_decoder ? "HW" : "SW";
  return uma_name;
}

void MediaMetricsProvider::ReportPipelineUMA() {
  if (uma_info_.has_video && uma_info_.has_audio) {
    base::UmaHistogramExactLinear(GetUMANameForAVStream(uma_info_),
                                  uma_info_.last_pipeline_status,
                                  PIPELINE_STATUS_MAX + 1);
  } else if (uma_info_.has_audio) {
    base::UmaHistogramExactLinear("Media.PipelineStatus.AudioOnly",
                                  uma_info_.last_pipeline_status,
                                  PIPELINE_STATUS_MAX + 1);
  } else if (uma_info_.has_video) {
    base::UmaHistogramExactLinear("Media.PipelineStatus.VideoOnly",
                                  uma_info_.last_pipeline_status,
                                  PIPELINE_STATUS_MAX + 1);
  } else {
    // Note: This metric can be recorded as a result of normal operation with
    // Media Source Extensions. If a site creates a MediaSource object but never
    // creates a source buffer or appends data, PIPELINE_OK will be recorded.
    base::UmaHistogramExactLinear("Media.PipelineStatus.Unsupported",
                                  uma_info_.last_pipeline_status,
                                  PIPELINE_STATUS_MAX + 1);
  }

  // Report whether video decoder fallback happened, but only if a video decoder
  // was reported.
  if (uma_info_.video_pipeline_info.decoder_type !=
      VideoDecoderType::kUnknown) {
    base::UmaHistogramBoolean("Media.VideoDecoderFallback",
                              uma_info_.video_decoder_changed);
  }

  // Report whether this player ever saw a playback event. Used to measure the
  // effectiveness of efforts to reduce loaded-but-never-used players.
  if (uma_info_.has_reached_have_enough)
    base::UmaHistogramBoolean("Media.HasEverPlayed", uma_info_.has_ever_played);

  // Report whether an encrypted playback is in incognito window, excluding
  // never-used players.
  if (uma_info_.is_eme && uma_info_.has_ever_played)
    base::UmaHistogramBoolean("Media.EME.IsIncognito", uma_info_.is_incognito);
}

// static
void MediaMetricsProvider::Create(
    BrowsingMode is_incognito,
    FrameStatus is_top_frame,
    GetSourceIdCallback get_source_id_cb,
    GetOriginCallback get_origin_cb,
    VideoDecodePerfHistory::SaveCallback save_cb,
    GetLearningSessionCallback learning_session_cb,
    GetRecordAggregateWatchTimeCallback get_record_playback_cb,
    mojo::PendingReceiver<mojom::MediaMetricsProvider> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MediaMetricsProvider>(
          is_incognito, is_top_frame, get_source_id_cb.Run(),
          get_origin_cb.Run(), std::move(save_cb),
          std::move(learning_session_cb),
          std::move(get_record_playback_cb).Run()),
      std::move(receiver));
}

void MediaMetricsProvider::SetHasPlayed() {
  uma_info_.has_ever_played = true;
}

void MediaMetricsProvider::SetHasAudio(AudioCodec audio_codec) {
  uma_info_.audio_codec = audio_codec;
  uma_info_.has_audio = true;
}

void MediaMetricsProvider::SetHasVideo(VideoCodec video_codec) {
  uma_info_.video_codec = video_codec;
  uma_info_.has_video = true;
}

void MediaMetricsProvider::SetHaveEnough() {
  uma_info_.has_reached_have_enough = true;
}

void MediaMetricsProvider::SetVideoPipelineInfo(const VideoDecoderInfo& info) {
  auto old_decoder = uma_info_.video_pipeline_info.decoder_type;
  if (old_decoder != VideoDecoderType::kUnknown &&
      old_decoder != info.decoder_type)
    uma_info_.video_decoder_changed = true;
  uma_info_.video_pipeline_info = info;
}

void MediaMetricsProvider::SetAudioPipelineInfo(const AudioDecoderInfo& info) {
  uma_info_.audio_pipeline_info = info;
}

void MediaMetricsProvider::Initialize(
    bool is_mse,
    mojom::MediaURLScheme url_scheme,
    mojom::MediaStreamType media_stream_type) {
  if (initialized_) {
    mojo::ReportBadMessage(kInvalidInitialize);
    return;
  }

  is_mse_ = is_mse;
  initialized_ = true;
  url_scheme_ = url_scheme;
  media_stream_type_ = media_stream_type;
}

void MediaMetricsProvider::OnError(PipelineStatus status) {
  DCHECK(initialized_);
  uma_info_.last_pipeline_status = status;
}

void MediaMetricsProvider::SetIsEME() {
  // This may be called before Initialize().
  uma_info_.is_eme = true;
}

void MediaMetricsProvider::SetTimeToMetadata(base::TimeDelta elapsed) {
  DCHECK(initialized_);
  DCHECK_EQ(time_to_metadata_, kNoTimestamp);
  time_to_metadata_ = elapsed;
}

void MediaMetricsProvider::SetTimeToFirstFrame(base::TimeDelta elapsed) {
  DCHECK(initialized_);
  DCHECK_EQ(time_to_first_frame_, kNoTimestamp);
  time_to_first_frame_ = elapsed;
}

void MediaMetricsProvider::SetTimeToPlayReady(base::TimeDelta elapsed) {
  DCHECK(initialized_);
  DCHECK_EQ(time_to_play_ready_, kNoTimestamp);
  time_to_play_ready_ = elapsed;
}

void MediaMetricsProvider::SetContainerName(
    container_names::MediaContainerName container_name) {
  DCHECK(initialized_);
  DCHECK(!container_name_.has_value());
  container_name_ = container_name;
}

void MediaMetricsProvider::AcquireWatchTimeRecorder(
    mojom::PlaybackPropertiesPtr properties,
    mojo::PendingReceiver<mojom::WatchTimeRecorder> receiver) {
  if (!initialized_) {
    mojo::ReportBadMessage(kInvalidInitialize);
    return;
  }

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WatchTimeRecorder>(std::move(properties), source_id_,
                                          is_top_frame_, player_id_,
                                          record_playback_cb_),
      std::move(receiver));
}

void MediaMetricsProvider::AcquireVideoDecodeStatsRecorder(
    mojo::PendingReceiver<mojom::VideoDecodeStatsRecorder> receiver) {
  if (!initialized_) {
    mojo::ReportBadMessage(kInvalidInitialize);
    return;
  }

  if (!save_cb_) {
    DVLOG(3) << __func__ << " Ignoring request, SaveCallback is null";
    return;
  }

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<VideoDecodeStatsRecorder>(save_cb_, source_id_, origin_,
                                                 is_top_frame_, player_id_),
      std::move(receiver));
}

void MediaMetricsProvider::AcquirePlaybackEventsRecorder(
    mojo::PendingReceiver<mojom::PlaybackEventsRecorder> receiver) {
#if defined(OS_FUCHSIA) || (BUILDFLAG(IS_CHROMECAST) && defined(OS_ANDROID))
  PlaybackEventsRecorder::Create(std::move(receiver));
#endif
}

void MediaMetricsProvider::AcquireLearningTaskController(
    const std::string& taskName,
    mojo::PendingReceiver<learning::mojom::LearningTaskController> receiver) {
  learning::LearningSession* session = learning_session_cb_.Run();
  if (!session) {
    DVLOG(3) << __func__ << " Ignoring request, unable to get LearningSession.";
    return;
  }

  auto controller = session->GetController(taskName);

  if (!controller) {
    DVLOG(3) << __func__ << " Ignoring request, no controller found for task: '"
             << taskName << "'.";
    return;
  }

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<learning::MojoLearningTaskControllerService>(
          controller->GetLearningTask(), source_id_, std::move(controller)),
      std::move(receiver));
}

}  // namespace media
