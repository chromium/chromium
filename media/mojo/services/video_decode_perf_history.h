// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_VIDEO_DECODE_PERF_HISTORY_H_
#define MEDIA_MOJO_SERVICES_VIDEO_DECODE_PERF_HISTORY_H_

#include <stdint.h>
#include <memory>
#include <queue>

#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "media/base/video_codecs.h"
#include "media/capabilities/video_decode_stats_db.h"
#include "media/capabilities/video_decode_stats_db_provider.h"
#include "media/learning/impl/feature_provider.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class LearningHelper;

// This class saves and retrieves video decode performance statistics on behalf
// of the MediaCapabilities API. It also helps to grade the accuracy of the API
// by comparing its history-based assessment of smoothness/power-efficiency to
// the observed performance as new stats are saved.
//
// The database is lazily initialized/loaded upon the first API call requiring
// DB access. DB implementations must take care to perform work on a separate
// task runner.
//
// Retrieving stats is triggered by calls to the GetPerfInfo() Mojo interface.
// The raw values are reduced to booleans (is_smooth, is_power_efficient) which
// are sent along the Mojo callback.
//
// Saving stats is performed by SavePerfRecord(), where a record is defined as a
// continuous playback of a stream with fixed decode characteristics (profile,
// natural size, frame rate).
//
// THREAD SAFETY:
// This class is not thread safe. All API calls should be made on the same
// sequence.
class MEDIA_MOJO_EXPORT VideoDecodePerfHistory
    : public mojom::VideoDecodePerfHistory,
      public VideoDecodeStatsDBProvider,
      public base::SupportsUserData::Data {
 public:
  static const char kMaxSmoothDroppedFramesPercentParamName[];
  static const char kEmeMaxSmoothDroppedFramesPercentParamName[];

  explicit VideoDecodePerfHistory(
      std::unique_ptr<VideoDecodeStatsDB> db,
      learning::FeatureProviderFactoryCB feature_factory_cb =
          learning::FeatureProviderFactoryCB());

  VideoDecodePerfHistory(const VideoDecodePerfHistory&) = delete;
  VideoDecodePerfHistory& operator=(const VideoDecodePerfHistory&) = delete;

  ~VideoDecodePerfHistory() override;

  // Bind the mojo receiver to this instance. Single instance will be used to
  // serve multiple receivers.
  void BindReceiver(
      mojo::PendingReceiver<mojom::VideoDecodePerfHistory> receiver);

  // mojom::VideoDecodePerfHistory implementation:
  void GetPerfInfo(mojom::PredictionFeaturesPtr features,
                   GetPerfInfoCallback got_info_cb) override;

  // Provides a callback for saving a stats record for the described stream.
  // This callback will silently fail if called after |this| is destroyed.
  // Saving is generally fire-and-forget, but |save_done_cb| may be provided
  // for tests to know the save is complete.
  using SaveCallback =
      base::RepeatingCallback<void(ukm::SourceId source_id,
                                   learning::FeatureValue origin,
                                   bool is_top_frame,
                                   mojom::PredictionFeatures features,
                                   mojom::PredictionTargets targets,
                                   uint64_t player_id,
                                   base::OnceClosure save_done_cb)>;
  SaveCallback GetSaveCallback();

  // Clear all history from the underlying database. Run |clear_done_cb| when
  // complete.
  void ClearHistory(base::OnceClosure clear_done_cb);

  // From VideoDecodeStatsDBProvider. |cb| receives a pointer to the
  // *initialized* VideoDecodeStatsDB, or null in case of error.
  void GetVideoDecodeStatsDB(GetCB cb) override;

 private:
  friend class VideoDecodePerfHistoryTest;

  // Decode capabilities will be described as "smooth" whenever the percentage
  // of dropped frames is less-than-or-equal-to this value.
  static double GetMaxSmoothDroppedFramesPercent(bool is_eme);

  // Returns current feature params.
  static base::FieldTrialParams GetFieldTrialParams();

  // Track the status of database lazy initialization.
  enum InitStatus {
    UNINITIALIZED,
    PENDING,
    COMPLETE,
    FAILED,
  };

  // Decode capabilities will be described as "power efficient" whenever the
  // percentage of power efficient decoded frames is higher-than-or-equal-to
  // this value.
  static constexpr double kMinPowerEfficientDecodedFramePercent = .50;

  // Initialize the database. Will return early if initialization is
  // already PENDING.
  void InitDatabase();

  // Callback from |db_->Initialize()|.
  void OnDatabaseInit(bool success);

  // Initiate saving of the provided record. See GetSaveCallback().
  void SavePerfRecord(ukm::SourceId source_id,
                      learning::FeatureValue origin,
                      bool is_top_frame,
                      mojom::PredictionFeatures features,
                      mojom::PredictionTargets targets,
                      uint64_t player_id,
                      base::OnceClosure save_done_cb);

  // Internal callback for database queries made from GetPerfInfo() (mojo API).
  // Assesses performance from database stats and passes results to
  // |got_info_cb|.
  void OnGotStatsForRequest(
      const VideoDecodeStatsDB::VideoDescKey& video_key,
      GetPerfInfoCallback got_info_cb,
      bool database_success,
      std::unique_ptr<VideoDecodeStatsDB::DecodeStatsEntry> stats);

  // Internal callback for database queries made from SavePerfRecord(). Compares
  // past performance to this latest record as means of "grading" the accuracy
  // of the GetPerfInfo() API. Comparison is recorded via UKM. Then saves the
  // |new_*| performance stats to the database.
  void OnGotStatsForSave(
      ukm::SourceId source_id,
      bool is_top_frame,
      uint64_t player_id,
      const VideoDecodeStatsDB::VideoDescKey& video_key,
      const VideoDecodeStatsDB::DecodeStatsEntry& new_stats,
      base::OnceClosure save_done_cb,
      bool success,
      std::unique_ptr<VideoDecodeStatsDB::DecodeStatsEntry> past_stats);

  // Internal callback for saving to database. Will run |save_done_cb| if
  // nonempty.
  void OnSaveDone(base::OnceClosure save_done_cb, bool success);

  // Report UKM metrics to grade the claims of the API by evaluating how well
  // |past_stats| predicts |new_stats|.
  void ReportUkmMetrics(ukm::SourceId source_id,
                        bool is_top_frame,
                        uint64_t player_id,
                        const VideoDecodeStatsDB::VideoDescKey& video_key,
                        const VideoDecodeStatsDB::DecodeStatsEntry& new_stats,
                        VideoDecodeStatsDB::DecodeStatsEntry* past_stats);

  void AssessStats(const VideoDecodeStatsDB::VideoDescKey& key,
                   const VideoDecodeStatsDB::DecodeStatsEntry* stats,
                   bool* is_smooth,
                   bool* is_power_efficient);

  // Internal callback for ClearHistory(). Reinitializes the database and runs
  // |clear_done_cb|.
  void OnClearedHistory(base::OnceClosure clear_done_cb);

  // Underlying database for managing/coalescing decode stats. Const to enforce
  // assignment during construction and never cleared. We hand out references to
  // the db via GetVideoDecodeStatsDB(), so clearing or reassigning breaks those
  // dependencies.
  const std::unique_ptr<VideoDecodeStatsDB> db_;

  // Tracks whether we've received OnDatabaseIniti() callback. All database
  // operations should be deferred until initialization is complete.
  InitStatus db_init_status_;

  // Vector of bound public API calls, to be run once DB initialization
  // completes.
  std::vector<base::OnceClosure> init_deferred_api_calls_;

  // Maps receivers from several render-processes to this single browser-process
  // service.
  mojo::ReceiverSet<mojom::VideoDecodePerfHistory> receivers_;

  // Optional helper for local learning.
  std::unique_ptr<LearningHelper> learning_helper_;

  // Optional callback to create a FeatureProvider for |learning_helper_|.
  learning::FeatureProviderFactoryCB feature_factory_cb_;

  // Ensures all access to class members come on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VideoDecodePerfHistory> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_VIDEO_DECODE_PERF_HISTORY_H_
