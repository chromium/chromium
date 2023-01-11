// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/video_decode_stats_recorder.h"

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"

#include "base/logging.h"

namespace media {

VideoDecodeStatsRecorder::VideoDecodeStatsRecorder(
    VideoDecodePerfHistory::SaveCallback save_cb,
    ukm::SourceId source_id,
    learning::FeatureValue origin,
    bool is_top_frame,
    uint64_t player_id)
    : save_cb_(std::move(save_cb)),
      source_id_(source_id),
      origin_(origin),
      is_top_frame_(is_top_frame),
      player_id_(player_id) {
  DCHECK(save_cb_);
}

VideoDecodeStatsRecorder::~VideoDecodeStatsRecorder() {
  DVLOG(2) << __func__ << " Finalize for IPC disconnect";
  FinalizeRecord();
}

void VideoDecodeStatsRecorder::StartNewRecord(
    mojom::PredictionFeaturesPtr features) {
  DCHECK_NE(features->profile, VIDEO_CODEC_PROFILE_UNKNOWN);
  DCHECK_GT(features->frames_per_sec, 0);
  DCHECK(features->video_size.width() > 0 && features->video_size.height() > 0);

  // DO THIS FIRST! Finalize existing stats with the current state.
  FinalizeRecord();

  features_ = *features;

  DVLOG(2) << __func__ << "profile: " << features_.profile
           << " sz:" << features_.video_size.ToString()
           << " fps:" << features_.frames_per_sec
           << " key_system:" << features_.key_system
           << " use_hw_secure_codecs:" << features_.use_hw_secure_codecs;

  // Reinitialize to defaults.
  targets_ = mojom::PredictionTargets();
}

void VideoDecodeStatsRecorder::UpdateRecord(
    mojom::PredictionTargetsPtr targets) {
  DVLOG(3) << __func__ << " decoded:" << targets->frames_decoded
           << " dropped:" << targets->frames_dropped;

  // Dropped can never exceed decoded.
  DCHECK_LE(targets->frames_dropped, targets->frames_decoded);
  // Power efficient frames can never exceed decoded frames.
  DCHECK_LE(targets->frames_power_efficient, targets->frames_decoded);
  // Should never go backwards.
  DCHECK_GE(targets->frames_decoded, targets_.frames_decoded);
  DCHECK_GE(targets->frames_dropped, targets_.frames_dropped);
  DCHECK_GE(targets->frames_power_efficient, targets_.frames_power_efficient);

  targets_ = *targets;
}

void VideoDecodeStatsRecorder::FinalizeRecord() {
  if (features_.profile == VIDEO_CODEC_PROFILE_UNKNOWN ||
      targets_.frames_decoded == 0) {
    return;
  }

  DVLOG(2) << __func__ << " profile: " << features_.profile
           << " size:" << features_.video_size.ToString()
           << " fps:" << features_.frames_per_sec
           << " decoded:" << targets_.frames_decoded
           << " dropped:" << targets_.frames_dropped
           << " power efficient decoded:" << targets_.frames_power_efficient;

  // Final argument is an empty save-done-callback. No action to take if save
  // fails (DB already records UMAs on failure). Callback mainly used by tests.
  save_cb_.Run(source_id_, origin_, is_top_frame_, features_, targets_,
               player_id_, base::OnceClosure());
}

}  // namespace media
