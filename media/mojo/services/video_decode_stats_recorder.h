// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_VIDEO_DECODE_STATS_RECORDER_H_
#define MEDIA_MOJO_SERVICES_VIDEO_DECODE_STATS_RECORDER_H_

#include <stdint.h>

#include "media/base/video_codecs.h"
#include "media/learning/common/value.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/mojom/video_decode_stats_recorder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "url/gurl.h"

namespace media {

// See mojom::VideoDecodeStatsRecorder for documentation.
class MEDIA_MOJO_EXPORT VideoDecodeStatsRecorder
    : public mojom::VideoDecodeStatsRecorder {
 public:
  // |perf_history| required to save decode stats to local database and report
  // metrics. Callers must ensure that |perf_history| outlives this object; may
  // be nullptr if database recording is currently disabled.
  VideoDecodeStatsRecorder(VideoDecodePerfHistory::SaveCallback save_cb,
                           ukm::SourceId source_id,
                           learning::FeatureValue origin,
                           bool is_top_frame,
                           uint64_t player_id);

  VideoDecodeStatsRecorder(const VideoDecodeStatsRecorder&) = delete;
  VideoDecodeStatsRecorder& operator=(const VideoDecodeStatsRecorder&) = delete;

  ~VideoDecodeStatsRecorder() override;

  // mojom::VideoDecodeStatsRecorder implementation:
  void StartNewRecord(mojom::PredictionFeaturesPtr features) override;
  void UpdateRecord(mojom::PredictionTargetsPtr targets) override;

 private:
  // Save most recent stats values to disk. Called during destruction and upon
  // starting a new record.
  void FinalizeRecord();

  const VideoDecodePerfHistory::SaveCallback save_cb_;
  const ukm::SourceId source_id_;
  const learning::FeatureValue origin_;
  const bool is_top_frame_;
  const uint64_t player_id_;

  mojom::PredictionFeatures features_;
  mojom::PredictionTargets targets_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_VIDEO_DECODE_STATS_RECORDER_H_
