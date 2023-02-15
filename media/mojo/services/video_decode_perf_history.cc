// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/video_decode_perf_history.h"

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/key_systems.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/capabilities/learning_helper.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace media {

namespace {

const double kMaxSmoothDroppedFramesPercentParamDefault = .05;

}  // namespace

const char VideoDecodePerfHistory::kMaxSmoothDroppedFramesPercentParamName[] =
    "smooth_threshold";

const char
    VideoDecodePerfHistory::kEmeMaxSmoothDroppedFramesPercentParamName[] =
        "eme_smooth_threshold";

// static
double VideoDecodePerfHistory::GetMaxSmoothDroppedFramesPercent(bool is_eme) {
  double threshold = base::GetFieldTrialParamByFeatureAsDouble(
      kMediaCapabilitiesWithParameters, kMaxSmoothDroppedFramesPercentParamName,
      kMaxSmoothDroppedFramesPercentParamDefault);

  // For EME, the precedence of overrides is:
  // 1. EME specific override, |k*Eme*MaxSmoothDroppedFramesPercentParamName
  // 2. Non-EME override, |kMaxSmoothDroppedFramesPercentParamName|
  // 3. |kMaxSmoothDroppedFramesPercentParamDefault|
  if (is_eme) {
    threshold = base::GetFieldTrialParamByFeatureAsDouble(
        kMediaCapabilitiesWithParameters,
        kEmeMaxSmoothDroppedFramesPercentParamName, threshold);
  }

  return threshold;
}

// static
base::FieldTrialParams VideoDecodePerfHistory::GetFieldTrialParams() {
  base::FieldTrialParams actual_trial_params;

  const bool result = base::GetFieldTrialParamsByFeature(
      kMediaCapabilitiesWithParameters, &actual_trial_params);
  DCHECK(result);

  return actual_trial_params;
}

VideoDecodePerfHistory::VideoDecodePerfHistory(
    std::unique_ptr<VideoDecodeStatsDB> db,
    learning::FeatureProviderFactoryCB feature_factory_cb)
    : db_(std::move(db)),
      db_init_status_(UNINITIALIZED),
      feature_factory_cb_(std::move(feature_factory_cb)) {
  DVLOG(2) << __func__;
  DCHECK(db_);

  // If the local learning experiment is enabled, then also create
  // |learning_helper_| to send data to it.
  if (base::FeatureList::IsEnabled(kMediaLearningExperiment))
    learning_helper_ = std::make_unique<LearningHelper>(feature_factory_cb_);
}

VideoDecodePerfHistory::~VideoDecodePerfHistory() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoDecodePerfHistory::BindReceiver(
    mojo::PendingReceiver<mojom::VideoDecodePerfHistory> receiver) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver));
}

void VideoDecodePerfHistory::InitDatabase() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (db_init_status_ == PENDING)
    return;

  // DB should be initialized only once! We hand out references to the
  // initialized DB via GetVideoDecodeStatsDB(). Dependents expect DB to remain
  // initialized during their lifetime.
  DCHECK_EQ(db_init_status_, UNINITIALIZED);

  db_->Initialize(base::BindOnce(&VideoDecodePerfHistory::OnDatabaseInit,
                                 weak_ptr_factory_.GetWeakPtr()));
  db_init_status_ = PENDING;
}

void VideoDecodePerfHistory::OnDatabaseInit(bool success) {
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

void VideoDecodePerfHistory::GetPerfInfo(mojom::PredictionFeaturesPtr features,
                                         GetPerfInfoCallback got_info_cb) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_NE(features->profile, VIDEO_CODEC_PROFILE_UNKNOWN);
  DCHECK_GT(features->frames_per_sec, 0);
  DCHECK(features->video_size.width() > 0 && features->video_size.height() > 0);

  if (db_init_status_ == FAILED) {
    // Optimistically claim perf is both smooth and power efficient.
    std::move(got_info_cb).Run(true, true);
    return;
  }

  // Defer this request until the DB is initialized.
  if (db_init_status_ != COMPLETE) {
    init_deferred_api_calls_.push_back(base::BindOnce(
        &VideoDecodePerfHistory::GetPerfInfo, weak_ptr_factory_.GetWeakPtr(),
        std::move(features), std::move(got_info_cb)));
    InitDatabase();
    return;
  }

  VideoDecodeStatsDB::VideoDescKey video_key =
      VideoDecodeStatsDB::VideoDescKey::MakeBucketedKey(
          features->profile, features->video_size, features->frames_per_sec,
          features->key_system, features->use_hw_secure_codecs);

  db_->GetDecodeStats(
      video_key, base::BindOnce(&VideoDecodePerfHistory::OnGotStatsForRequest,
                                weak_ptr_factory_.GetWeakPtr(), video_key,
                                std::move(got_info_cb)));
}

void VideoDecodePerfHistory::AssessStats(
    const VideoDecodeStatsDB::VideoDescKey& key,
    const VideoDecodeStatsDB::DecodeStatsEntry* stats,
    bool* is_smooth,
    bool* is_power_efficient) {
  // TODO(chcunningham/mlamouri): Refactor database API to give us nearby
  // stats whenever we don't have a perfect match. If higher
  // resolutions/frame rates are known to be smooth, we can report this as
  /// smooth. If lower resolutions/frames are known to be janky, we can assume
  // this will be janky.

  // No stats? Lets be optimistic.
  if (!stats || stats->frames_decoded == 0) {
    *is_power_efficient = true;
    *is_smooth = true;
    return;
  }

  double percent_dropped =
      static_cast<double>(stats->frames_dropped) / stats->frames_decoded;
  double percent_power_efficient =
      static_cast<double>(stats->frames_power_efficient) /
      stats->frames_decoded;

  *is_power_efficient =
      percent_power_efficient >= kMinPowerEfficientDecodedFramePercent;

  *is_smooth = percent_dropped <=
               GetMaxSmoothDroppedFramesPercent(!key.key_system.empty());
}

void VideoDecodePerfHistory::OnGotStatsForRequest(
    const VideoDecodeStatsDB::VideoDescKey& video_key,
    GetPerfInfoCallback got_info_cb,
    bool database_success,
    std::unique_ptr<VideoDecodeStatsDB::DecodeStatsEntry> stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(got_info_cb);
  DCHECK_EQ(db_init_status_, COMPLETE);

  bool is_power_efficient = false;
  bool is_smooth = false;
  double percent_dropped = 0;
  double percent_power_efficient = 0;

  AssessStats(video_key, stats.get(), &is_smooth, &is_power_efficient);

  if (stats && stats->frames_decoded) {
    DCHECK(database_success);
    percent_dropped =
        static_cast<double>(stats->frames_dropped) / stats->frames_decoded;
    percent_power_efficient =
        static_cast<double>(stats->frames_power_efficient) /
        stats->frames_decoded;
  }

  DVLOG(3) << __func__
           << base::StringPrintf(
                  " profile:%s size:%s fps:%d --> ",
                  GetProfileName(video_key.codec_profile).c_str(),
                  video_key.size.ToString().c_str(), video_key.frame_rate)
           << (stats.get()
                   ? base::StringPrintf(
                         "smooth:%d frames_decoded:%" PRIu64 " pcnt_dropped:%f"
                         " pcnt_power_efficent:%f",
                         is_smooth, stats->frames_decoded, percent_dropped,
                         percent_power_efficient)
                   : (database_success ? "no info" : "query FAILED"));

  std::move(got_info_cb).Run(is_smooth, is_power_efficient);
}

VideoDecodePerfHistory::SaveCallback VideoDecodePerfHistory::GetSaveCallback() {
  return base::BindRepeating(&VideoDecodePerfHistory::SavePerfRecord,
                             weak_ptr_factory_.GetWeakPtr());
}

void VideoDecodePerfHistory::SavePerfRecord(ukm::SourceId source_id,
                                            learning::FeatureValue origin,
                                            bool is_top_frame,
                                            mojom::PredictionFeatures features,
                                            mojom::PredictionTargets targets,
                                            uint64_t player_id,
                                            base::OnceClosure save_done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3)
      << __func__
      << base::StringPrintf(
             " profile:%s size:%s fps:%f decoded:%d dropped:%d efficient:%d",
             GetProfileName(features.profile).c_str(),
             features.video_size.ToString().c_str(), features.frames_per_sec,
             targets.frames_decoded, targets.frames_dropped,
             targets.frames_power_efficient);

  if (db_init_status_ == FAILED) {
    DVLOG(3) << __func__ << " Can't save stats. No DB!";
    return;
  }

  // Defer this request until the DB is initialized.
  if (db_init_status_ != COMPLETE) {
    init_deferred_api_calls_.push_back(base::BindOnce(
        &VideoDecodePerfHistory::SavePerfRecord, weak_ptr_factory_.GetWeakPtr(),
        source_id, origin, is_top_frame, std::move(features),
        std::move(targets), player_id, std::move(save_done_cb)));
    InitDatabase();
    return;
  }

  VideoDecodeStatsDB::VideoDescKey video_key =
      VideoDecodeStatsDB::VideoDescKey::MakeBucketedKey(
          features.profile, features.video_size, features.frames_per_sec,
          features.key_system, features.use_hw_secure_codecs);
  VideoDecodeStatsDB::DecodeStatsEntry new_stats(
      targets.frames_decoded, targets.frames_dropped,
      targets.frames_power_efficient);

  if (learning_helper_)
    learning_helper_->AppendStats(video_key, origin, new_stats);

  // Get past perf info and report UKM metrics before saving this record.
  db_->GetDecodeStats(
      video_key,
      base::BindOnce(&VideoDecodePerfHistory::OnGotStatsForSave,
                     weak_ptr_factory_.GetWeakPtr(), source_id, is_top_frame,
                     player_id, video_key, new_stats, std::move(save_done_cb)));
}

void VideoDecodePerfHistory::OnGotStatsForSave(
    ukm::SourceId source_id,
    bool is_top_frame,
    uint64_t player_id,
    const VideoDecodeStatsDB::VideoDescKey& video_key,
    const VideoDecodeStatsDB::DecodeStatsEntry& new_stats,
    base::OnceClosure save_done_cb,
    bool success,
    std::unique_ptr<VideoDecodeStatsDB::DecodeStatsEntry> past_stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(db_init_status_, COMPLETE);

  if (!success) {
    DVLOG(3) << __func__ << " FAILED! Aborting save.";
    if (save_done_cb)
      std::move(save_done_cb).Run();
    return;
  }

  ReportUkmMetrics(source_id, is_top_frame, player_id, video_key, new_stats,
                   past_stats.get());

  db_->AppendDecodeStats(
      video_key, new_stats,
      base::BindOnce(&VideoDecodePerfHistory::OnSaveDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(save_done_cb)));
}

void VideoDecodePerfHistory::OnSaveDone(base::OnceClosure save_done_cb,
                                        bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(chcunningham): Monitor UMA. Experiment with re-initializing DB to
  // remedy IO failures.
  DVLOG(3) << __func__ << (success ? " succeeded" : " FAILED!");

  // Don't bother to bubble success. Its not actionable for upper layers. Also,
  // save_done_cb only used for test sequencing, where DB should always behave
  // (or fail the test).
  if (save_done_cb)
    std::move(save_done_cb).Run();
}

void VideoDecodePerfHistory::ReportUkmMetrics(
    ukm::SourceId source_id,
    bool is_top_frame,
    uint64_t player_id,
    const VideoDecodeStatsDB::VideoDescKey& video_key,
    const VideoDecodeStatsDB::DecodeStatsEntry& new_stats,
    VideoDecodeStatsDB::DecodeStatsEntry* past_stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // UKM may be unavailable in content_shell or other non-chrome/ builds; it
  // may also be unavailable if browser shutdown has started; so this may be a
  // nullptr. If it's unavailable, UKM reporting will be skipped.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  ukm::builders::Media_VideoDecodePerfRecord builder(source_id);
  builder.SetVideo_InTopFrame(is_top_frame);
  builder.SetVideo_PlayerID(player_id);

  builder.SetVideo_CodecProfile(video_key.codec_profile);
  builder.SetVideo_FramesPerSecond(video_key.frame_rate);
  builder.SetVideo_NaturalHeight(video_key.size.height());
  builder.SetVideo_NaturalWidth(video_key.size.width());

  if (!video_key.key_system.empty()) {
    builder.SetVideo_EME_KeySystem(GetKeySystemIntForUKM(video_key.key_system));
    builder.SetVideo_EME_UseHwSecureCodecs(video_key.use_hw_secure_codecs);
  }

  bool past_is_smooth = false;
  bool past_is_efficient = false;
  AssessStats(video_key, past_stats, &past_is_smooth, &past_is_efficient);
  builder.SetPerf_ApiWouldClaimIsSmooth(past_is_smooth);
  builder.SetPerf_ApiWouldClaimIsPowerEfficient(past_is_efficient);
  if (past_stats) {
    builder.SetPerf_PastVideoFramesDecoded(past_stats->frames_decoded);
    builder.SetPerf_PastVideoFramesDropped(past_stats->frames_dropped);
    builder.SetPerf_PastVideoFramesPowerEfficient(
        past_stats->frames_power_efficient);
  } else {
    builder.SetPerf_PastVideoFramesDecoded(0);
    builder.SetPerf_PastVideoFramesDropped(0);
    builder.SetPerf_PastVideoFramesPowerEfficient(0);
  }

  bool new_is_smooth = false;
  bool new_is_efficient = false;
  AssessStats(video_key, &new_stats, &new_is_smooth, &new_is_efficient);
  builder.SetPerf_RecordIsSmooth(new_is_smooth);
  builder.SetPerf_RecordIsPowerEfficient(new_is_efficient);
  builder.SetPerf_VideoFramesDecoded(new_stats.frames_decoded);
  builder.SetPerf_VideoFramesDropped(new_stats.frames_dropped);
  builder.SetPerf_VideoFramesPowerEfficient(new_stats.frames_power_efficient);

  builder.Record(ukm_recorder);
}

void VideoDecodePerfHistory::ClearHistory(base::OnceClosure clear_done_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If we have a learning helper, then replace it.  This will erase any data
  // that it currently has.
  if (learning_helper_)
    learning_helper_ = std::make_unique<LearningHelper>(feature_factory_cb_);

  if (db_init_status_ == FAILED) {
    DVLOG(3) << __func__ << " Can't clear history - No DB!";
    std::move(clear_done_cb).Run();
    return;
  }

  // Defer this request until the DB is initialized.
  if (db_init_status_ != COMPLETE) {
    init_deferred_api_calls_.push_back(base::BindOnce(
        &VideoDecodePerfHistory::ClearHistory, weak_ptr_factory_.GetWeakPtr(),
        std::move(clear_done_cb)));
    InitDatabase();
    return;
  }

  db_->ClearStats(base::BindOnce(&VideoDecodePerfHistory::OnClearedHistory,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(clear_done_cb)));
}

void VideoDecodePerfHistory::OnClearedHistory(base::OnceClosure clear_done_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(clear_done_cb).Run();
}

void VideoDecodePerfHistory::GetVideoDecodeStatsDB(GetCB get_db_cb) {
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
        base::BindOnce(&VideoDecodePerfHistory::GetVideoDecodeStatsDB,
                       weak_ptr_factory_.GetWeakPtr(), std::move(get_db_cb)));
    InitDatabase();
    return;
  }

  // DB is already initialized. base::BindPostTaskToCurrentDefault to avoid
  // reentrancy.
  std::move(base::BindPostTaskToCurrentDefault(std::move(get_db_cb)))
      .Run(db_.get());
}

}  // namespace media
