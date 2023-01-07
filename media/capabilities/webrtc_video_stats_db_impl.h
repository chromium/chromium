// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPABILITIES_WEBRTC_VIDEO_STATS_DB_IMPL_H_
#define MEDIA_CAPABILITIES_WEBRTC_VIDEO_STATS_DB_IMPL_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/capabilities/pending_operations.h"
#include "media/capabilities/webrtc_video_stats_db.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class FilePath;
class Clock;
}  // namespace base

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace media {

class WebrtcVideoStatsEntryProto;

// LevelDB implementation of WebrtcVideoStatsDB. This class is not
// thread safe. All API calls should happen on the same sequence used for
// construction. API callbacks will also occur on this sequence.
class MEDIA_EXPORT WebrtcVideoStatsDBImpl : public WebrtcVideoStatsDB {
 public:
  // Create an instance! `db_dir` specifies where to store LevelDB files to
  // disk. LevelDB generates a handful of files, so its recommended to provide a
  // dedicated directory to keep them isolated.
  static std::unique_ptr<WebrtcVideoStatsDBImpl> Create(
      base::FilePath db_dir,
      leveldb_proto::ProtoDatabaseProvider* db_provider);

  ~WebrtcVideoStatsDBImpl() override;
  WebrtcVideoStatsDBImpl(const WebrtcVideoStatsDBImpl&) = delete;
  WebrtcVideoStatsDBImpl& operator=(const WebrtcVideoStatsDBImpl&) = delete;

  // Implement WebrtcVideoStatsDB.
  void Initialize(InitializeCB init_cb) override;
  void AppendVideoStats(const VideoDescKey& key,
                        const VideoStats& video_stats,
                        AppendVideoStatsCB append_done_cb) override;
  void GetVideoStats(const VideoDescKey& key,
                     GetVideoStatsCB get_stats_cb) override;
  void GetVideoStatsCollection(const VideoDescKey& key,
                               GetVideoStatsCollectionCB get_stats_cb) override;
  void ClearStats(base::OnceClosure clear_done_cb) override;

 private:
  // Test classes are friends, see comment below.
  friend class WebrtcVideoStatsDBImplTest;
  friend class WebrtcVideoPerfLPMFuzzerHelper;

  // Private constructor only called by tests (friends). Production code
  // should always use the static Create() method.
  explicit WebrtcVideoStatsDBImpl(
      std::unique_ptr<leveldb_proto::ProtoDatabase<WebrtcVideoStatsEntryProto>>
          db);

  // Called when the database has been initialized. Will immediately call
  // `init_cb` to forward `success`.
  void OnInit(PendingOperations::Id id,
              InitializeCB init_cb,
              leveldb_proto::Enums::InitStatus status);

  // Returns true if the DB is successfully initialized.
  bool IsInitialized();

  // Passed as the callback for `OnGotVideoStats` by `AppendVideoStats` to
  // update the database once we've read the existing stats entry.
  void WriteUpdatedEntry(
      PendingOperations::Id op_id,
      const VideoDescKey& key,
      const VideoStats& new_video_stats,
      AppendVideoStatsCB append_done_cb,
      bool read_success,
      std::unique_ptr<WebrtcVideoStatsEntryProto> stats_proto);

  // Called when the database has been modified after a call to
  // `WriteUpdatedEntry`. Will run `append_done_cb` when done.
  void OnEntryUpdated(PendingOperations::Id op_id,
                      AppendVideoStatsCB append_done_cb,
                      bool success);

  // Called when GetVideoStats() operation was performed. `get_stats_cb`
  // will be run with `success` and a `VideoStatsEntry` created from
  // `stats_proto` or nullptr if no entry was found for the requested key.
  void OnGotVideoStats(PendingOperations::Id op_id,
                       GetVideoStatsCB get_stats_cb,
                       bool success,
                       std::unique_ptr<WebrtcVideoStatsEntryProto> stats_proto);

  // Called when GetVideoStatsCollection() operation was performed.
  // `get_stats_cb` will be run with `success` and a `VideoStatsCollection`
  // created from the `stats_proto` map or nullptr if no entries were found for
  // the filtered key.
  void OnGotVideoStatsCollection(
      PendingOperations::Id op_id,
      GetVideoStatsCollectionCB get_stats_cb,
      bool success,
      std::unique_ptr<std::map<std::string, WebrtcVideoStatsEntryProto>>
          stats_proto);

  // Internal callback for OnLoadAllKeysForClearing(), initially triggered by
  // ClearStats(). Method simply logs `success` and runs `clear_done_cb`.
  void OnStatsCleared(PendingOperations::Id op_id,
                      base::OnceClosure clear_done_cb,
                      bool success);

  // Validates the stats entry. If true is returned the stats are sorted in the
  // correct order and contain values that are somewhat reasonable.
  bool AreStatsValid(const WebrtcVideoStatsEntryProto* const stats_proto);

  void set_wall_clock_for_test(const base::Clock* tick_clock) {
    wall_clock_ = tick_clock;
  }

  PendingOperations pending_operations_;

  // Indicates whether initialization is completed. Does not indicate whether it
  // was successful. Will be reset upon calling DestroyStats(). Failed
  // initialization is signaled by setting `db_` to null.
  bool db_init_ = false;

  // ProtoDatabase instance. Set to nullptr if fatal database error is
  // encountered. Each entry in the DB is expected to be around 200 bytes. It is
  // expected that there will be at most ~100 entries so the total database size
  // is expected to not exceed 20 kB.
  std::unique_ptr<leveldb_proto::ProtoDatabase<WebrtcVideoStatsEntryProto>> db_;

  // For getting wall-clock time. Tests may override via
  // set_wall_clock_for_test().
  raw_ptr<const base::Clock> wall_clock_ = nullptr;

  // Ensures all access to class members come on the same sequence. API calls
  // and callbacks should occur on the same sequence used during construction.
  // LevelDB operations happen on a separate task runner, but all LevelDB
  // callbacks to this happen on the checked sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebrtcVideoStatsDBImpl> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPABILITIES_WEBRTC_VIDEO_STATS_DB_IMPL_H_
