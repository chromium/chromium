// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_metrics_provider.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "media/mojo/services/video_decode_stats_recorder.h"
#include "media/mojo/services/watch_time_recorder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace media {

constexpr char kInvalidInitialize[] = "Initialize() was not called correctly.";

static uint64_t g_player_id = 0;

MediaMetricsProvider::MediaMetricsProvider(
    bool is_top_frame,
    ukm::SourceId source_id,
    VideoDecodePerfHistory::SaveCallback save_cb)
    : player_id_(g_player_id++),
      is_top_frame_(is_top_frame),
      source_id_(source_id),
      save_cb_(std::move(save_cb)) {}

MediaMetricsProvider::~MediaMetricsProvider() {
  // UKM may be unavailable in content_shell or other non-chrome/ builds; it
  // may also be unavailable if browser shutdown has started; so this may be a
  // nullptr. If it's unavailable, UKM reporting will be skipped.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder || !initialized_)
    return;

  ukm::builders::Media_WebMediaPlayerState builder(source_id_);
  builder.SetPlayerID(player_id_);
  builder.SetIsTopFrame(is_top_frame_);
  builder.SetIsEME(is_eme_);
  builder.SetIsMSE(is_mse_);
  builder.SetFinalPipelineStatus(pipeline_status_);
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

  // Buffered bytes are reported from a different source for EME/MSE.
  std::string suffix;
  if (is_eme_)
    suffix = "EME";
  else if (is_mse_)
    suffix = "MSE";
  else
    suffix = "SRC";
  base::UmaHistogramMemoryKB("Media.BytesReceived." + suffix,
                             total_bytes_received_ >> 10);
  if (is_ad_media_) {
    base::UmaHistogramMemoryKB("Ads.Media.BytesReceived",
                               total_bytes_received_ >> 10);
    base::UmaHistogramMemoryKB("Ads.Media.BytesReceived." + suffix,
                               total_bytes_received_ >> 10);
  }
}

// static
void MediaMetricsProvider::Create(bool is_top_frame,
                                  GetSourceIdCallback get_source_id_cb,
                                  VideoDecodePerfHistory::SaveCallback save_cb,
                                  mojom::MediaMetricsProviderRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<MediaMetricsProvider>(
          is_top_frame, get_source_id_cb.Run(), std::move(save_cb)),
      std::move(request));
}

void MediaMetricsProvider::Initialize(bool is_mse,
                                      mojom::MediaURLScheme url_scheme) {
  if (initialized_) {
    mojo::ReportBadMessage(kInvalidInitialize);
    return;
  }

  is_mse_ = is_mse;
  initialized_ = true;
  url_scheme_ = url_scheme;
}

void MediaMetricsProvider::OnError(PipelineStatus status) {
  DCHECK(initialized_);
  pipeline_status_ = status;
}

void MediaMetricsProvider::SetIsAdMedia() {
  // This may be called before Initialize().
  is_ad_media_ = true;
}

void MediaMetricsProvider::SetIsEME() {
  // This may be called before Initialize().
  is_eme_ = true;
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
    mojom::WatchTimeRecorderRequest request) {
  if (!initialized_) {
    mojo::ReportBadMessage(kInvalidInitialize);
    return;
  }

  mojo::MakeStrongBinding(
      std::make_unique<WatchTimeRecorder>(std::move(properties), source_id_,
                                          is_top_frame_, player_id_),
      std::move(request));
}

void MediaMetricsProvider::AcquireVideoDecodeStatsRecorder(
    mojom::VideoDecodeStatsRecorderRequest request) {
  if (!initialized_) {
    mojo::ReportBadMessage(kInvalidInitialize);
    return;
  }

  if (!save_cb_) {
    DVLOG(3) << __func__ << " Ignoring request, SaveCallback is null";
    return;
  }

  mojo::MakeStrongBinding(std::make_unique<VideoDecodeStatsRecorder>(
                              save_cb_, source_id_, is_top_frame_, player_id_),
                          std::move(request));
}

void MediaMetricsProvider::AddBytesReceived(uint64_t bytes_received) {
  total_bytes_received_ += bytes_received;
}

}  // namespace media
