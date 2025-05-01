// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/services/webrtc_video_perf_recorder.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace media {
namespace {
constexpr int EncodeOrDecodeIndex(bool is_decode) {
  return is_decode ? 0 : 1;
}
}  // namespace

// static
void WebrtcVideoPerfRecorder::Create(
    media::WebrtcVideoPerfHistory* webrtc_video_perf_history,
    mojo::PendingReceiver<media::mojom::WebrtcVideoPerfRecorder> receiver) {
  // Only save video stats when BrowserContext provides a
  // WebrtcVideoPerfHistory. Off-the-record contexts will internally use an
  // in-memory history DB.
  media::WebrtcVideoPerfHistory::SaveCallback save_stats_cb;
  if (webrtc_video_perf_history) {
    save_stats_cb = webrtc_video_perf_history->GetSaveCallback();
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebrtcVideoPerfRecorder>(std::move(save_stats_cb)),
      std::move(receiver));
}

WebrtcVideoPerfRecorder::WebrtcVideoPerfRecorder(
    WebrtcVideoPerfHistory::SaveCallback save_cb)
    : save_cb_(std::move(save_cb)) {
  DCHECK(save_cb_);
}

WebrtcVideoPerfRecorder::~WebrtcVideoPerfRecorder() {
  DVLOG(2) << __func__ << " Finalize for IPC disconnect";
  FinalizeRecord(EncodeOrDecodeIndex(/*is_decode=*/true));
  FinalizeRecord(EncodeOrDecodeIndex(/*is_decode=*/false));
}

void WebrtcVideoPerfRecorder::UpdateRecord(
    media::mojom::WebrtcPredictionFeaturesPtr features,
    media::mojom::WebrtcVideoStatsPtr video_stats) {
  // `features` and `video_stats` are potentially compromised and should not be
  // used in any calculations directly. They are sanity checked in
  // WebrtcVideoPerfHistory before stored to the database.
  int encode_or_decode_index = EncodeOrDecodeIndex(features->is_decode_stats);

  if (features_[encode_or_decode_index].profile != features->profile ||
      features_[encode_or_decode_index].video_pixels !=
          features->video_pixels ||
      features_[encode_or_decode_index].hardware_accelerated !=
          features->hardware_accelerated) {
    StartNewRecord(std::move(features));
  }

  DVLOG(3) << __func__ << " frames_processed:" << video_stats->frames_processed
           << " key_frames_processed:" << video_stats->key_frames_processed
           << " p99_processing_time_ms:" << video_stats->p99_processing_time_ms;

  video_stats_[encode_or_decode_index] = *video_stats;
}

void WebrtcVideoPerfRecorder::StartNewRecord(
    media::mojom::WebrtcPredictionFeaturesPtr features) {
  DVLOG(3) << __func__ << " is_decode_stats:" << features->is_decode_stats
           << " profile:" << features->profile
           << " video_pixels:" << features->video_pixels
           << " hardware_accelerated:" << features->hardware_accelerated;

  // Finalize existing stats with the current state.
  int encode_or_decode_index = EncodeOrDecodeIndex(features->is_decode_stats);
  FinalizeRecord(encode_or_decode_index);

  features_[encode_or_decode_index] = *features;
  // Reinitialize to defaults.
  video_stats_[encode_or_decode_index] = media::mojom::WebrtcVideoStats();
}

void WebrtcVideoPerfRecorder::FinalizeRecord(int encode_or_decode_index) {
  if (features_[encode_or_decode_index].profile ==
          VIDEO_CODEC_PROFILE_UNKNOWN ||
      features_[encode_or_decode_index].video_pixels == 0 ||
      video_stats_[encode_or_decode_index].frames_processed == 0) {
    return;
  }

  DVLOG(3) << __func__ << " is_decode_stats"
           << features_[encode_or_decode_index].is_decode_stats
           << " profile:" << features_[encode_or_decode_index].profile
           << " video_pixels:" << features_[encode_or_decode_index].video_pixels
           << " hardware_accelerated:"
           << features_[encode_or_decode_index].hardware_accelerated
           << " frames_processed:"
           << video_stats_[encode_or_decode_index].frames_processed
           << " key_frames_processed:"
           << video_stats_[encode_or_decode_index].key_frames_processed
           << " p99_processing_time_ms:"
           << video_stats_[encode_or_decode_index].p99_processing_time_ms;

  // Final argument is an empty save-done-callback. No action to take if save
  // fails (DB already records UMAs on failure). Callback mainly used by tests.
  save_cb_.Run(features_[encode_or_decode_index],
               video_stats_[encode_or_decode_index], base::OnceClosure());
}

}  // namespace media
