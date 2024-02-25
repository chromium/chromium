// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_WEBRTC_VIDEO_PERF_HISTORY_H_
#define MEDIA_MOJO_SERVICES_WEBRTC_VIDEO_PERF_HISTORY_H_

#include <stdint.h>
#include <memory>
#include <queue>

#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "media/base/video_codecs.h"
#include "media/capabilities/webrtc_video_stats_db.h"
#include "media/capabilities/webrtc_video_stats_db_provider.h"
#include "media/mojo/mojom/webrtc_video_perf.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace media {

// This class saves and retrieves WebRTC video performance statistics on behalf
// of the MediaCapabilities API.
//
// The database is lazily initialized/loaded upon the first API call requiring
// DB access. DB implementations must take care to perform work on a separate
// task runner.
//
// Retrieving stats is triggered by calls to the GetPerfInfo() Mojo interface.
// The raw values are reduced to a boolean (is_smooth) which is sent along the
// Mojo callback.
//
// Saving stats is performed by SavePerfRecord(), where a record is defined as a
// continuous playback/encoding of a stream with fixed characteristics (codec
// profile, number of pixels, hardware accelerated).
//
// THREAD SAFETY:
// This class is not thread safe. All API calls should be made on the same
// sequence.
class MEDIA_MOJO_EXPORT WebrtcVideoPerfHistory
    : public media::mojom::WebrtcVideoPerfHistory,
      public WebrtcVideoStatsDBProvider,
      public base::SupportsUserData::Data {
 public:
  explicit WebrtcVideoPerfHistory(std::unique_ptr<WebrtcVideoStatsDB> db);
  ~WebrtcVideoPerfHistory() override;
  WebrtcVideoPerfHistory(const WebrtcVideoPerfHistory&) = delete;
  WebrtcVideoPerfHistory& operator=(const WebrtcVideoPerfHistory&) = delete;

  // Bind the mojo receiver to this instance. Single instance will be used to
  // serve multiple receivers.
  void BindReceiver(
      mojo::PendingReceiver<media::mojom::WebrtcVideoPerfHistory> receiver);

  // mojom::WebrtcVideoPerfHistory implementation.
  void GetPerfInfo(media::mojom::WebrtcPredictionFeaturesPtr features,
                   int frames_per_second,
                   GetPerfInfoCallback got_info_cb) override;

  // Provides a callback for saving a stats record for the described stream.
  // This callback will silently fail if called after `this` is destroyed.
  // Saving is generally fire-and-forget, but `save_done_cb` may be provided
  // for tests to know the save is complete.
  using SaveCallback = base::RepeatingCallback<void(
      media::mojom::WebrtcPredictionFeatures features,
      media::mojom::WebrtcVideoStats video_stats,
      base::OnceClosure save_done_cb)>;
  SaveCallback GetSaveCallback();

  // Clear all history from the underlying database. Run `clear_done_cb` when
  // complete.
  void ClearHistory(base::OnceClosure clear_done_cb);

  // WebrtcVideoStatsDBProvider implementation. `cb` receives a pointer to the
  // *initialized* WebrtcVideoStatsDB, or null in case of error.
  void GetWebrtcVideoStatsDB(GetCB cb) override;

 private:
  friend class WebrtcVideoPerfHistoryTest;

  // Threshold that is used when determining if the processing can be handled
  // smoothly. A threshold of 1.0 means that the 99th percentile is allowed to
  // the same a 1 s / `frames_per_second` and still considered smooth.
  static float GetSmoothnessThreshold(bool is_decode);

  // Threshold that determines how several entries are combined into a
  // smoothness decision. A threshold of 0.5 means that 50% of the individual
  // sessions should be assessed as smooth for the combined outcome to be
  // smooth.
  static float GetSmoothDecisionRatioThreshold();

  // Returns the estimated smoothness response for the specified `stats_entry`
  // at the rate given by `frames_per_second`.
  static bool PredictSmooth(
      bool is_decode,
      const WebrtcVideoStatsDB::VideoStatsEntry& stats_entry,
      int frames_per_second);

  // Simulates what the smoothness response will be after the update to the DB
  // at the rate given by `frames_per_second`.
  static bool PredictSmoothAfterUpdate(
      bool is_decode,
      const WebrtcVideoStatsDB::VideoStats& new_stats,
      const WebrtcVideoStatsDB::VideoStatsEntry& past_stats_entry,
      int frames_per_second);

  // Reports UMA metrics comparing the predicted smoothness before and after a
  // save operation.
  static void ReportUmaMetricsOnSave(
      bool is_decode_stats,
      const WebrtcVideoStatsDB::VideoStats& new_stats,
      const WebrtcVideoStatsDB::VideoStatsEntry& past_stats_entry);

  // Track the status of database lazy initialization.
  enum InitStatus {
    UNINITIALIZED,
    PENDING,
    COMPLETE,
    FAILED,
  };

  // Initialize the database. Will return early if initialization is
  // already PENDING.
  void InitDatabase();

  // Callback from `db_->Initialize()`.
  void OnDatabaseInit(bool success);

  // Initiate saving of the provided record. See GetSaveCallback().
  void SavePerfRecord(media::mojom::WebrtcPredictionFeatures features,
                      media::mojom::WebrtcVideoStats video_stats,
                      base::OnceClosure save_done_cb);

  // Internal callback for database queries made from GetPerfInfo() (mojo API).
  // Assesses performance from database stats and passes results to
  // `got_info_cb`.
  void OnGotStatsCollectionForRequest(
      const WebrtcVideoStatsDB::VideoDescKey& video_key,
      int frames_per_second,
      GetPerfInfoCallback got_info_cb,
      bool database_success,
      std::optional<WebrtcVideoStatsDB::VideoStatsCollection> stats);

  // Internal callback for database queries made from SavePerfRecord(). Compares
  // past performance to this latest record as means of "grading" the accuracy
  // of the GetPerfInfo() API. Then updates the database with the `new_stats`.
  void OnGotStatsForSave(
      const WebrtcVideoStatsDB::VideoDescKey& video_key,
      const WebrtcVideoStatsDB::VideoStats& new_stats,
      base::OnceClosure save_done_cb,
      bool success,
      std::optional<WebrtcVideoStatsDB::VideoStatsEntry> past_stats);

  // Internal callback for saving to database. Will run `save_done_cb` if
  // nonempty.
  void OnSaveDone(base::OnceClosure save_done_cb, bool success);

  // Internal callback for ClearHistory(). Reinitializes the database and runs
  // `clear_done_cb`.
  void OnClearedHistory(base::OnceClosure clear_done_cb);

  // Underlying database for managing/coalescing decode stats. Const to enforce
  // assignment during construction and never cleared. We hand out references to
  // the db via GetWebrtcVideoStatsDB(), so clearing or reassigning breaks
  // those dependencies.
  const std::unique_ptr<WebrtcVideoStatsDB> db_;

  // Tracks whether we've received OnDatabaseInit() callback. All database
  // operations should be deferred until initialization is complete.
  InitStatus db_init_status_ = UNINITIALIZED;

  // Vector of bound public API calls, to be run once DB initialization
  // completes.
  std::vector<base::OnceClosure> init_deferred_api_calls_;

  // Maps receivers from several render-processes to this single browser-process
  // service.
  mojo::ReceiverSet<media::mojom::WebrtcVideoPerfHistory> receivers_;

  // Ensures all access to class members come on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebrtcVideoPerfHistory> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_WEBRTC_VIDEO_PERF_HISTORY_H_
