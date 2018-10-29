// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_METRICS_PROVIDER_H_
#define MEDIA_MOJO_SERVICES_MEDIA_METRICS_PROVIDER_H_

#include <stdint.h>

#include "media/base/container_names.h"
#include "media/base/pipeline_status.h"
#include "media/base/timestamp_constants.h"
#include "media/mojo/interfaces/media_metrics_provider.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace media {
class VideoDecodePerfHistory;

// See mojom::MediaMetricsProvider for documentation.
class MEDIA_MOJO_EXPORT MediaMetricsProvider
    : public mojom::MediaMetricsProvider {
 public:
  MediaMetricsProvider(bool is_top_frame,
                       ukm::SourceId source_id,
                       VideoDecodePerfHistory::SaveCallback save_cb);
  ~MediaMetricsProvider() override;

  // Callback for retrieving a ukm::SourceId.
  using GetSourceIdCallback = base::RepeatingCallback<ukm::SourceId(void)>;

  // Creates a MediaMetricsProvider, |perf_history| may be nullptr if perf
  // history database recording is disabled.
  static void Create(bool is_top_frame,
                     GetSourceIdCallback get_source_id_cb,
                     VideoDecodePerfHistory::SaveCallback save_cb,
                     mojom::MediaMetricsProviderRequest request);

 private:
  // mojom::MediaMetricsProvider implementation:
  void Initialize(bool is_mse, mojom::MediaURLScheme url_scheme) override;
  void OnError(PipelineStatus status) override;
  void SetIsAdMedia() override;
  void SetIsEME() override;
  void SetTimeToMetadata(base::TimeDelta elapsed) override;
  void SetTimeToFirstFrame(base::TimeDelta elapsed) override;
  void SetTimeToPlayReady(base::TimeDelta elapsed) override;
  void SetContainerName(
      container_names::MediaContainerName container_name) override;
  void AcquireWatchTimeRecorder(
      mojom::PlaybackPropertiesPtr properties,
      mojom::WatchTimeRecorderRequest request) override;
  void AcquireVideoDecodeStatsRecorder(
      mojom::VideoDecodeStatsRecorderRequest request) override;
  void AddBytesReceived(uint64_t bytes_received) override;

  // Session unique ID which maps to a given WebMediaPlayerImpl instances. Used
  // to coordinate multiply logged events with a singly logged metric.
  const uint64_t player_id_;

  // Are UKM reports for the main frame or for a subframe?
  const bool is_top_frame_;

  const ukm::SourceId source_id_;

  const VideoDecodePerfHistory::SaveCallback save_cb_;

  // These values are not always sent but have known defaults.
  PipelineStatus pipeline_status_ = PIPELINE_OK;
  bool is_eme_ = false;
  bool is_ad_media_ = false;

  // The values below are only set if |initialized_| is true.
  bool initialized_ = false;
  bool is_mse_;
  mojom::MediaURLScheme url_scheme_;

  // Total number of bytes received by the media player so far.
  uint64_t total_bytes_received_ = 0;

  base::TimeDelta time_to_metadata_ = kNoTimestamp;
  base::TimeDelta time_to_first_frame_ = kNoTimestamp;
  base::TimeDelta time_to_play_ready_ = kNoTimestamp;

  base::Optional<container_names::MediaContainerName> container_name_;

  DISALLOW_COPY_AND_ASSIGN(MediaMetricsProvider);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_METRICS_PROVIDER_H_
