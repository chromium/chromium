// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/webrtc_video_perf_history.h"

#include <math.h>

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/capabilities/bucket_utility.h"
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SmoothPrediction {
  kSmoothByDefault = 0,
  kSmoothFromData = 1,
  kNotSmoothFromData = 2,
  kImplicitlySmooth = 3,
  kSmoothOverride = 4,
  kImplicitlyNotSmooth = 5,
  kMaxValue = kImplicitlyNotSmooth,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LimitedCodecProfile {
  kOther = 0,
  kH264 = 1,
  kVP8 = 2,
  kVP9Profile0 = 3,
  kVP9Profile2 = 4,
  kAv1 = 5,
  kMaxValue = kAv1,
};

LimitedCodecProfile LimitedCodecProfileFromCodecProfile(
    VideoCodecProfile codec_profile) {
  if (codec_profile >= H264PROFILE_MIN && codec_profile <= H264PROFILE_MAX) {
    return LimitedCodecProfile::kH264;
  }
  if (codec_profile >= VP8PROFILE_MIN && codec_profile <= VP8PROFILE_MAX) {
    return LimitedCodecProfile::kVP8;
  }
  if (codec_profile == VP9PROFILE_PROFILE0) {
    return LimitedCodecProfile::kVP9Profile0;
  }
  if (codec_profile == VP9PROFILE_PROFILE2) {
    return LimitedCodecProfile::kVP9Profile2;
  }
  if (codec_profile >= AV1PROFILE_MIN && codec_profile <= AV1PROFILE_MAX) {
    return LimitedCodecProfile::kAv1;
  }
  return LimitedCodecProfile::kOther;
}

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

// Returns a UMA index for logging. The index corresponds to the key and the
// outcome of the smoothness prediction. Each bit in the index has the
// following meaning:
// bit | 12   11   10  |  9    8    7    6  |  5  |  4  |  3    2    1    0  |
//     |   pixels ix   |    codec profile   | res |is_hw| smooth prediction  |
int UmaSmoothPredictionData(const WebrtcVideoStatsDB::VideoDescKey& key,
                            SmoothPrediction prediction) {
  static_assert(static_cast<int>(SmoothPrediction::kMaxValue) < (1 << 4));
  static_assert(static_cast<int>(LimitedCodecProfile::kMaxValue) < (1 << 4));
  return GetWebrtcPixelsBucketIndex(key.pixels) << 10 |
         static_cast<int>(
             LimitedCodecProfileFromCodecProfile(key.codec_profile))
             << 6 |
         key.hardware_accelerated << 4 | static_cast<int>(prediction);
}

void ReportUmaSmoothPredictionData(const WebrtcVideoStatsDB::VideoDescKey& key,
                                   SmoothPrediction prediction) {
  std::string uma_name =
      base::StringPrintf("Media.WebrtcVideoPerfHistory.SmoothPrediction.%s",
                         (key.is_decode_stats ? "Decode" : "Encode"));
  base::UmaHistogramSparse(uma_name, UmaSmoothPredictionData(key, prediction));
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
         isnan(video_stats.p99_processing_time_ms) ||
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

  db_init_status_ = PENDING;
  db_->Initialize(base::BindOnce(&WebrtcVideoPerfHistory::OnDatabaseInit,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void WebrtcVideoPerfHistory::OnDatabaseInit(bool success) {
  DVLOG(2) << __func__ << " " << success;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(db_init_status_, PENDING);

  db_init_status_ = success ? COMPLETE : FAILED;

  // Post all the deferred API calls as if they're just now coming in.
  for (auto& deferred_call : init_deferred_api_calls_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(deferred_call));
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

  // Log the requested codec profile. Useful in determining if the number of
  // tracked profiles should be changed.
  UMA_HISTOGRAM_ENUMERATION(
      "Media.WebrtcVideoPerfHistory.GetPerfInfoCodecProfile", features->profile,
      media::VIDEO_CODEC_PROFILE_MAX + 1);

  WebrtcVideoStatsDB::VideoDescKey video_key =
      WebrtcVideoStatsDB::VideoDescKey::MakeBucketedKey(
          features->is_decode_stats, features->profile,
          features->hardware_accelerated, features->video_pixels);

  if (video_key.codec_profile == VIDEO_CODEC_PROFILE_UNKNOWN) {
    // This is a codec profile that is not tracked. Return smooth=true.
    DVLOG(2) << __func__
             << base::StringPrintf(
                    " The specified codec profile (%s) is not tracked. "
                    "Returning default value.",
                    GetProfileName(features->profile).c_str());
    std::move(got_info_cb).Run(true);
    return;
  }

  int frames_per_second_bucketed = MakeBucketedFramerate(frames_per_second);

  db_->GetVideoStatsCollection(
      video_key,
      base::BindOnce(&WebrtcVideoPerfHistory::OnGotStatsCollectionForRequest,
                     weak_ptr_factory_.GetWeakPtr(), video_key,
                     frames_per_second_bucketed, std::move(got_info_cb)));
}

void WebrtcVideoPerfHistory::OnGotStatsCollectionForRequest(
    const WebrtcVideoStatsDB::VideoDescKey& video_key,
    int frames_per_second,
    GetPerfInfoCallback got_info_cb,
    bool database_success,
    std::optional<WebrtcVideoStatsDB::VideoStatsCollection> stats_collection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(got_info_cb);
  DCHECK_EQ(db_init_status_, COMPLETE);

  // Be optimistic if there's no data.
  bool is_smooth = true;
  SmoothPrediction prediction = SmoothPrediction::kSmoothByDefault;
  if (stats_collection) {
    // Create a vector filled with smoothness
    // predictions for all entries in the collection. `specific_key_index`
    // will point to the entry corresponding to the requested `video_key`. If
    // there is no entry corresponding to `video_key` an std::nullopt will be
    // inserted as a placeholder.
    std::vector<std::optional<bool>> smooth_per_pixel;
    std::optional<size_t> specific_key_index;
    for (auto const& [key_index, video_stats_entry] : *stats_collection) {
      if (key_index >= video_key.pixels && !specific_key_index) {
        specific_key_index = smooth_per_pixel.size();
        if (key_index > video_key.pixels) {
          // No exact match found, insert a nullopt.
          smooth_per_pixel.push_back(std::nullopt);
        }
      }
      smooth_per_pixel.push_back(PredictSmooth(
          video_key.is_decode_stats, video_stats_entry, frames_per_second));
    }
    if (!specific_key_index) {
      // Pixels for the specific key is higher than any pixels number that
      // exists in the database.
      specific_key_index = smooth_per_pixel.size();
      smooth_per_pixel.push_back(std::nullopt);
    }

    if (smooth_per_pixel[*specific_key_index].has_value()) {
      prediction = smooth_per_pixel[*specific_key_index].value()
                       ? SmoothPrediction::kSmoothFromData
                       : SmoothPrediction::kNotSmoothFromData;
    }

    // Traverse from highest pixels value to lowest and propagate smooth=true,
    // override smooth=false.
    std::optional<bool> previous_entry;
    for (auto it = smooth_per_pixel.rbegin(); it != smooth_per_pixel.rend();
         ++it) {
      if (previous_entry.has_value() && previous_entry.value()) {
        if (!it->has_value()) {
          // Fill empty slot.
          prediction = SmoothPrediction::kImplicitlySmooth;
          *it = previous_entry;
        } else if (!it->value()) {
          // Override (because smooth=true has precedence over smooth=false) and
          // log this since it's anomalous.
          prediction = SmoothPrediction::kSmoothOverride;
          *it = previous_entry;
        }
      }
      previous_entry = *it;
    }

    // Traverse from lowest to highest pixels value and propagate smooth=false
    // if there are empty slots.
    previous_entry.reset();
    for (auto& it : smooth_per_pixel) {
      if (previous_entry.has_value() && !previous_entry.value()) {
        if (!it.has_value()) {
          // Fill empty slot.
          prediction = SmoothPrediction::kImplicitlyNotSmooth;
          it = previous_entry;
        }
      }
      previous_entry = it;
    }

    DCHECK(specific_key_index);
    if (smooth_per_pixel[*specific_key_index].has_value()) {
      is_smooth = smooth_per_pixel[*specific_key_index].value();
    }
  }

  ReportUmaSmoothPredictionData(video_key, prediction);

  DVLOG(3) << __func__
           << base::StringPrintf(
                  " is_decode:%d profile:%s pixels:%d hw:%d --> ",
                  video_key.is_decode_stats,
                  GetProfileName(video_key.codec_profile).c_str(),
                  video_key.pixels, video_key.hardware_accelerated)
           << (stats_collection
                   ? base::StringPrintf("smooth:%d entries:%zu prediction:%d",
                                        is_smooth, stats_collection->size(),
                                        static_cast<int>(prediction))
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

  // Log the codec profile. Useful in determining if the number of tracked
  // profiles should be changed.
  UMA_HISTOGRAM_ENUMERATION(
      "Media.WebrtcVideoPerfHistory.SavePerfRecordCodecProfile",
      features.profile, media::VIDEO_CODEC_PROFILE_MAX + 1);

  WebrtcVideoStatsDB::VideoDescKey video_key =
      WebrtcVideoStatsDB::VideoDescKey::MakeBucketedKey(
          features.is_decode_stats, features.profile,
          features.hardware_accelerated, features.video_pixels);

  if (video_key.codec_profile == VIDEO_CODEC_PROFILE_UNKNOWN) {
    DVLOG(3) << __func__
             << " codec profile is not tracked and won't be stored.";
    return;
  }

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
    std::optional<WebrtcVideoStatsDB::VideoStatsEntry> past_stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(db_init_status_, COMPLETE);

  if (!success) {
    DVLOG(3) << __func__ << " FAILED! Aborting save.";
    if (save_done_cb)
      std::move(save_done_cb).Run();
    return;
  }

  if (past_stats && !past_stats->empty()) {
    ReportUmaMetricsOnSave(video_key.is_decode_stats, new_stats, *past_stats);
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

  // DB is already initialized. base::BindPostTaskToCurrentDefault to avoid
  // reentrancy.
  std::move(base::BindPostTaskToCurrentDefault(std::move(get_db_cb)))
      .Run(db_.get());
}

// static
bool WebrtcVideoPerfHistory::PredictSmooth(
    bool is_decode,
    const WebrtcVideoStatsDB::VideoStatsEntry& stats_entry,
    int frames_per_second) {
  // No stats? Lets be optimistic.
  if (stats_entry.empty()) {
    return true;
  }

  const float kSmoothnessThreshold = GetSmoothnessThreshold(is_decode);
  float smooth_count = 0;
  for (auto const& stats : stats_entry) {
    smooth_count +=
        PredictSmoothFromStats(stats, frames_per_second, kSmoothnessThreshold);
  }
  return smooth_count / stats_entry.size() >= GetSmoothDecisionRatioThreshold();
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
        PredictSmooth(is_decode_stats, past_stats_entry, frames_per_second);
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
