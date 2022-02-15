// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/webrtc_video_perf_history.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/mojo/mojom/media_types.mojom.h"

namespace media {

namespace {
constexpr float kSmoothnessThresholdDecodeDefault = 1.0f;
constexpr float kSmoothnessThresholdEncodeDefault = 1.0f;
constexpr float kSmoothDecisionRatioThresholdDefault = 0.5f;
// Field trial parameter names.
constexpr char kSmoothnessThresholdDecodeParamName[] =
    "smoothness_threshold_decode";
constexpr char kSmoothnessThresholdEncodeParamName[] =
    "smoothness_threshold_encode";
constexpr char kSmoothDecisionRatioThresholdParamName[] =
    "smooth_decision_ratio_threshold";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SmoothVolatility {
  kNotSmooth2NotSmooth = 0,
  kSmooth2NotSmooth = 1,
  kNotSmooth2Smooth = 2,
  kSmooth2Smooth = 3,
  kMaxValue = kSmooth2Smooth,
};

constexpr SmoothVolatility SmoothVolatilityFromBool(bool smooth_before,
                                                    bool smooth_after) {
  if (smooth_before && smooth_after)
    return SmoothVolatility::kSmooth2Smooth;
  if (smooth_before && !smooth_after)
    return SmoothVolatility::kSmooth2NotSmooth;
  if (!smooth_before && smooth_after)
    return SmoothVolatility::kNotSmooth2Smooth;
  return SmoothVolatility::kNotSmooth2NotSmooth;
}

bool PredictSmoothFromStats(const WebrtcVideoStatsDB::VideoStats& stats,
                            int frames_per_second,
                            float threshold) {
  // `kMillisecondsPerSecond` / `frames_per_second` is the maximum average
  // number of ms that the processing can take in order to keep up with the fps.
  return stats.p99_processing_time_ms /
             (static_cast<float>(base::Time::kMillisecondsPerSecond) /
              frames_per_second) <
         threshold;
}

constexpr int MakeBucketedFramerate(int framerate) {
  // Quantize the framerate to the closest of the two framerates.
  constexpr int kFramerateBuckets[] = {30, 60};
  constexpr int kFramerateThreshold =
      (kFramerateBuckets[0] + kFramerateBuckets[1]) / 2;
  return framerate < kFramerateThreshold ? kFramerateBuckets[0]
                                         : kFramerateBuckets[1];
}

bool AreFeaturesInvalid(
    const media::mojom::WebrtcPredictionFeatures& features) {
  return features.video_pixels <= 0 ||
         features.video_pixels > WebrtcVideoStatsDB::kPixelsAbsoluteMaxValue ||
         features.profile < VIDEO_CODEC_PROFILE_MIN ||
         features.profile > VIDEO_CODEC_PROFILE_MAX ||
         features.profile == VIDEO_CODEC_PROFILE_UNKNOWN;
}

bool IsFramesPerSecondInvalid(int frames_per_second) {
  // The min/max check of `frames_per_second` is only to filter out number that
  // are completely out of range. The frame rate will be bucketed later on in
  // the code.
  constexpr int kMaxFramesPerSecond = 1000;
  return frames_per_second <= 0 || frames_per_second > kMaxFramesPerSecond;
}

bool AreVideoStatsInvalid(const media::mojom::WebrtcVideoStats& video_stats) {
  return video_stats.frames_processed <
             WebrtcVideoStatsDB::kFramesProcessedMinValue ||
         video_stats.frames_processed >
             WebrtcVideoStatsDB::kFramesProcessedMaxValue ||
         video_stats.key_frames_processed > video_stats.frames_processed ||
         video_stats.p99_processing_time_ms <
             WebrtcVideoStatsDB::kP99ProcessingTimeMinValueMs ||
         video_stats.p99_processing_time_ms >
             WebrtcVideoStatsDB::kP99ProcessingTimeMaxValueMs;
}

}  // namespace

WebrtcVideoPerfHistory::WebrtcVideoPerfHistory(
    std::unique_ptr<WebrtcVideoStatsDB> db)
    : db_(std::move(db)) {
  DVLOG(2) << __func__;
  DCHECK(db_);
}

WebrtcVideoPerfHistory::~WebrtcVideoPerfHistory() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebrtcVideoPerfHistory::BindReceiver(
    mojo::PendingReceiver<media::mojom::WebrtcVideoPerfHistory> receiver) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver));
}

void WebrtcVideoPerfHistory::InitDatabase() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (db_init_status_ == PENDING)
    return;

  // DB should be initialized only once! We hand out references to the
  // initialized DB via GetWebrtcVideoStatsDB(). Dependents expect DB to remain
  // initialized during their lifetime.
  DCHECK_EQ(db_init_status_, UNINITIALIZED);

  db_->Initialize(base::BindOnce(&WebrtcVideoPerfHistory::OnDatabaseInit,
                                 weak_ptr_factory_.GetWeakPtr()));
  db_init_status_ = PENDING;
}

void WebrtcVideoPerfHistory::OnDatabaseInit(bool success) {
  DVLOG(2) << __func__ << " " << success;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(db_init_status_, PENDING);

  db_init_status_ = success ? COMPLETE : FAILED;

  // Post all the deferred API calls as if they're just now coming in.
  for (auto& deferred_call : init_deferred_api_calls_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(deferred_call));
  }
  init_deferred_api_calls_.clear();
}

void WebrtcVideoPerfHistory::GetPerfInfo(
    media::mojom::WebrtcPredictionFeaturesPtr features,
    int frames_per_second,
    GetPerfInfoCallback got_info_cb) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (db_init_status_ == FAILED) {
    // Optimistically claim perf is smooth.
    std::move(got_info_cb).Run(true);
    return;
  }

  // Defer this request until the DB is initialized.
  if (db_init_status_ != COMPLETE) {
    init_deferred_api_calls_.push_back(base::BindOnce(
        &WebrtcVideoPerfHistory::GetPerfInfo, weak_ptr_factory_.GetWeakPtr(),
        std::move(features), frames_per_second, std::move(got_info_cb)));
    InitDatabase();
    return;
  }

  // `features` and `frames_per_second` are coming over the mojo interface and
  // may be compromised. They must be sanity checked before they are used.
  if (AreFeaturesInvalid(*features) ||
      IsFramesPerSecondInvalid(frames_per_second)) {
    // Something must have happened if the features are not valid, perf is not
    // smooth in this case.
    std::move(got_info_cb).Run(false);
    return;
  }

  WebrtcVideoStatsDB::VideoDescKey video_key =
      WebrtcVideoStatsDB::VideoDescKey::MakeBucketedKey(
          features->is_decode_stats, features->profile,
          features->hardware_accelerated, features->video_pixels);

  int frames_per_second_bucketed = MakeBucketedFramerate(frames_per_second);

  db_->GetVideoStats(
      video_key,
      base::BindOnce(&WebrtcVideoPerfHistory::OnGotStatsForRequest,
                     weak_ptr_factory_.GetWeakPtr(), video_key,
                     frames_per_second_bucketed, std::move(got_info_cb)));
}

void WebrtcVideoPerfHistory::OnGotStatsForRequest(
    const WebrtcVideoStatsDB::VideoDescKey& video_key,
    int frames_per_second,
    GetPerfInfoCallback got_info_cb,
    bool database_success,
    std::unique_ptr<WebrtcVideoStatsDB::VideoStatsEntry> stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(got_info_cb);
  DCHECK_EQ(db_init_status_, COMPLETE);

  // TODO(crbug.com/1187565): Predict from the closest key that have data if we
  // have no data for `video_key`.
  bool is_smooth =
      PredictSmooth(video_key.is_decode_stats, stats.get(), frames_per_second);

  DVLOG(3) << __func__
           << base::StringPrintf(
                  " is_decode:%d profile:%s pixels:%d hw:%d --> ",
                  video_key.is_decode_stats,
                  GetProfileName(video_key.codec_profile).c_str(),
                  video_key.pixels, video_key.hardware_accelerated)
           << (stats.get() ? base::StringPrintf("smooth:%d entries:%zu",
                                                is_smooth, stats->size())
                           : (database_success ? "no info" : "query FAILED"));

  std::move(got_info_cb).Run(is_smooth);
}

WebrtcVideoPerfHistory::SaveCallback WebrtcVideoPerfHistory::GetSaveCallback() {
  return base::BindRepeating(&WebrtcVideoPerfHistory::SavePerfRecord,
                             weak_ptr_factory_.GetWeakPtr());
}

void WebrtcVideoPerfHistory::SavePerfRecord(
    media::mojom::WebrtcPredictionFeatures features,
    media::mojom::WebrtcVideoStats video_stats,
    base::OnceClosure save_done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(3) << __func__
           << base::StringPrintf(
                  " profile:%d size:%d hw:%d processed:%u "
                  "p99_processing_time:%.2f",
                  features.profile, features.video_pixels,
                  features.hardware_accelerated, video_stats.frames_processed,
                  video_stats.p99_processing_time_ms);

  if (db_init_status_ == FAILED) {
    DVLOG(3) << __func__ << " Can't save stats. No DB!";
    return;
  }

  // `features` and `video_stats` are coming over the mojo interface and may be
  // compromised. They must be sanity checked before they are used.
  if (AreFeaturesInvalid(features) || AreVideoStatsInvalid(video_stats)) {
    DVLOG(3) << __func__ << " features or video_stats are invalid.";
    return;
  }

  // Defer this request until the DB is initialized.
  if (db_init_status_ != COMPLETE) {
    init_deferred_api_calls_.push_back(base::BindOnce(
        &WebrtcVideoPerfHistory::SavePerfRecord, weak_ptr_factory_.GetWeakPtr(),
        std::move(features), std::move(video_stats), std::move(save_done_cb)));
    InitDatabase();
    return;
  }

  // Don't save entries with pixel sizes that are outside the specified
  // range.
  if (features.video_pixels < WebrtcVideoStatsDB::kPixelsMinValueToSave ||
      features.video_pixels > WebrtcVideoStatsDB::kPixelsMaxValueToSave) {
    DVLOG(3) << __func__
             << " video_pixels is out of range and won't be stored.";
    return;
  }

  WebrtcVideoStatsDB::VideoDescKey video_key =
      WebrtcVideoStatsDB::VideoDescKey::MakeBucketedKey(
          features.is_decode_stats, features.profile,
          features.hardware_accelerated, features.video_pixels);

  WebrtcVideoStatsDB::VideoStats new_stats(video_stats.frames_processed,
                                           video_stats.key_frames_processed,
                                           video_stats.p99_processing_time_ms);

  // Get past perf info and report UMA metrics before saving this record.
  db_->GetVideoStats(video_key,
                     base::BindOnce(&WebrtcVideoPerfHistory::OnGotStatsForSave,
                                    weak_ptr_factory_.GetWeakPtr(), video_key,
                                    new_stats, std::move(save_done_cb)));
}

void WebrtcVideoPerfHistory::OnGotStatsForSave(
    const WebrtcVideoStatsDB::VideoDescKey& video_key,
    const WebrtcVideoStatsDB::VideoStats& new_stats,
    base::OnceClosure save_done_cb,
    bool success,
    std::unique_ptr<WebrtcVideoStatsDB::VideoStatsEntry> past_stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(db_init_status_, COMPLETE);

  if (!success) {
    DVLOG(3) << __func__ << " FAILED! Aborting save.";
    if (save_done_cb)
      std::move(save_done_cb).Run();
    return;
  }

  if (past_stats && !past_stats->empty()) {
    ReportUmaMetricsOnSave(video_key.is_decode_stats, new_stats,
                           *past_stats.get());
  }

  db_->AppendVideoStats(
      video_key, new_stats,
      base::BindOnce(&WebrtcVideoPerfHistory::OnSaveDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(save_done_cb)));
}

void WebrtcVideoPerfHistory::OnSaveDone(base::OnceClosure save_done_cb,
                                        bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__ << (success ? " succeeded" : " FAILED!");

  // Don't bother to bubble success. It's not actionable for upper layers.
  // Also, `save_done_cb` only used for test sequencing, where DB should
  // always behave (or fail the test).
  if (save_done_cb)
    std::move(save_done_cb).Run();
}

void WebrtcVideoPerfHistory::ClearHistory(base::OnceClosure clear_done_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (db_init_status_ == FAILED) {
    DVLOG(3) << __func__ << " Can't clear history - No DB!";
    std::move(clear_done_cb).Run();
    return;
  }

  // Defer this request until the DB is initialized.
  if (db_init_status_ != COMPLETE) {
    init_deferred_api_calls_.push_back(base::BindOnce(
        &WebrtcVideoPerfHistory::ClearHistory, weak_ptr_factory_.GetWeakPtr(),
        std::move(clear_done_cb)));
    InitDatabase();
    return;
  }

  db_->ClearStats(base::BindOnce(&WebrtcVideoPerfHistory::OnClearedHistory,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(clear_done_cb)));
}

void WebrtcVideoPerfHistory::OnClearedHistory(base::OnceClosure clear_done_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(clear_done_cb).Run();
}

void WebrtcVideoPerfHistory::GetWebrtcVideoStatsDB(GetCB get_db_cb) {
  DVLOG(3) << __func__;
  DCHECK(get_db_cb);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (db_init_status_ == FAILED) {
    std::move(get_db_cb).Run(nullptr);
    return;
  }

  // Defer this request until the DB is initialized.
  if (db_init_status_ != COMPLETE) {
    init_deferred_api_calls_.push_back(
        base::BindOnce(&WebrtcVideoPerfHistory::GetWebrtcVideoStatsDB,
                       weak_ptr_factory_.GetWeakPtr(), std::move(get_db_cb)));
    InitDatabase();
    return;
  }

  // DB is already initialized. BindToCurrentLoop to avoid reentrancy.
  std::move(BindToCurrentLoop(std::move(get_db_cb))).Run(db_.get());
}

// static
bool WebrtcVideoPerfHistory::PredictSmooth(
    bool is_decode,
    const WebrtcVideoStatsDB::VideoStatsEntry* stats_entry,
    int frames_per_second) {
  // No stats? Lets be optimistic.
  if (!stats_entry || stats_entry->empty()) {
    return true;
  }

  const float kSmoothnessThreshold = GetSmoothnessThreshold(is_decode);
  float smooth_count = 0;
  for (auto const& stats : *stats_entry) {
    smooth_count +=
        PredictSmoothFromStats(stats, frames_per_second, kSmoothnessThreshold);
  }
  return smooth_count / stats_entry->size() >=
         GetSmoothDecisionRatioThreshold();
}

// static
bool WebrtcVideoPerfHistory::PredictSmoothAfterUpdate(
    bool is_decode,
    const WebrtcVideoStatsDB::VideoStats& new_stats,
    const WebrtcVideoStatsDB::VideoStatsEntry& past_stats_entry,
    int frames_per_second) {
  const int kMaxEntriesPerConfig = WebrtcVideoStatsDB::GetMaxEntriesPerConfig();
  float kSmoothnessThreshold = GetSmoothnessThreshold(is_decode);
  // Start with the new stats.
  float smooth_count = PredictSmoothFromStats(new_stats, frames_per_second,
                                              kSmoothnessThreshold);
  int total_count = 1;
  // Continue with existing stats up to the max count.
  for (auto const& stats : past_stats_entry) {
    smooth_count +=
        PredictSmoothFromStats(stats, frames_per_second, kSmoothnessThreshold);
    ++total_count;
    if (total_count >= kMaxEntriesPerConfig) {
      break;
    }
  }

  return smooth_count / total_count >= GetSmoothDecisionRatioThreshold();
}

// static
void WebrtcVideoPerfHistory::ReportUmaMetricsOnSave(
    bool is_decode_stats,
    const WebrtcVideoStatsDB::VideoStats& new_stats,
    const WebrtcVideoStatsDB::VideoStatsEntry& past_stats_entry) {
  constexpr int kFramesPerSecondToTest[] = {30, 60};

  for (auto& frames_per_second : kFramesPerSecondToTest) {
    bool smooth_before_save =
        PredictSmooth(is_decode_stats, &past_stats_entry, frames_per_second);
    bool smooth_after_save = PredictSmoothAfterUpdate(
        is_decode_stats, new_stats, past_stats_entry, frames_per_second);
    std::string uma_name = base::StringPrintf(
        "Media.WebrtcVideoPerfHistory.SmoothVolatility.%s.%dfps",
        (is_decode_stats ? "Decode" : "Encode"), frames_per_second);
    base::UmaHistogramEnumeration(
        uma_name,
        SmoothVolatilityFromBool(smooth_before_save, smooth_after_save));
  }
}

// static
float WebrtcVideoPerfHistory::GetSmoothnessThreshold(bool is_decode) {
  return is_decode ? base::GetFieldTrialParamByFeatureAsDouble(
                         kWebrtcMediaCapabilitiesParameters,
                         kSmoothnessThresholdDecodeParamName,
                         kSmoothnessThresholdDecodeDefault)
                   : base::GetFieldTrialParamByFeatureAsDouble(
                         kWebrtcMediaCapabilitiesParameters,
                         kSmoothnessThresholdEncodeParamName,
                         kSmoothnessThresholdEncodeDefault);
}

// static
float WebrtcVideoPerfHistory::GetSmoothDecisionRatioThreshold() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kWebrtcMediaCapabilitiesParameters,
      kSmoothDecisionRatioThresholdParamName,
      kSmoothDecisionRatioThresholdDefault);
}

}  // namespace media
