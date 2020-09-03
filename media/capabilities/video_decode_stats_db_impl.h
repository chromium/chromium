// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPABILITIES_VIDEO_DECODE_STATS_DB_IMPL_H_
#define MEDIA_CAPABILITIES_VIDEO_DECODE_STATS_DB_IMPL_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/capabilities/video_decode_stats_db.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class FilePath;
class Clock;
}  // namespace base

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace media {

class DecodeStatsProto;

// LevelDB implementation of VideoDecodeStatsDB. This class is not
// thread safe. All API calls should happen on the same sequence used for
// construction. API callbacks will also occur on this sequence.
class MEDIA_EXPORT VideoDecodeStatsDBImpl : public VideoDecodeStatsDB {
 public:
  static const char kMaxFramesPerBufferParamName[];
  static const char kMaxDaysToKeepStatsParamName[];
  static const char kEnableUnweightedEntriesParamName[];

  // Create an instance! |db_dir| specifies where to store LevelDB files to
  // disk. LevelDB generates a handful of files, so its recommended to provide a
  // dedicated directory to keep them isolated.
  static std::unique_ptr<VideoDecodeStatsDBImpl> Create(
      base::FilePath db_dir,
      leveldb_proto::ProtoDatabaseProvider* db_provider);

  ~VideoDecodeStatsDBImpl() override;

  // Implement VideoDecodeStatsDB.
  void Initialize(InitializeCB init_cb) override;
  void AppendDecodeStats(const VideoDescKey& key,
                         const DecodeStatsEntry& entry,
                         AppendDecodeStatsCB append_done_cb) override;
  void GetDecodeStats(const VideoDescKey& key,
                      GetDecodeStatsCB get_stats_cb) override;
  void ClearStats(base::OnceClosure clear_done_cb) override;

 private:
  friend class VideoDecodeStatsDBImplTest;

  using PendingOpId = int;

  // Private constructor only called by tests (friends). Production code
  // should always use the static Create() method.
  VideoDecodeStatsDBImpl(
      std::unique_ptr<leveldb_proto::ProtoDatabase<DecodeStatsProto>> db);

  // Default |last_write_time| for DB entries that lack a time stamp due to
  // using an earlier version of DecodeStatsProto. Date chosen so old stats from
  // previous version will expire (unless new stats arrive) roughly 2 months
  // after the proto update hits the chrome Stable channel (M71).
  static constexpr char kDefaultWriteTime[] = "01-FEB-2019 12:00pm";

  // Number of decoded frames to keep in the rolling "window" for a given entry
  // in the database.
  static int GetMaxFramesPerBuffer();

  // Number of days after which stats will be discarded if not updated. This
  // avoids users getting stuck with a bad capability prediction that may have
  // been due to one-off circumstances.
  static int GetMaxDaysToKeepStats();

  // When true, each playback entry in the DB should be given equal weight
  // regardless of how many frames were decoded.
  static bool GetEnableUnweightedEntries();

  // Returns current feature params.
  static base::FieldTrialParams GetFieldTrialParams();

  // Creates a PendingOperation using |uma_str| and adds it to |pending_ops_|
  // map. Returns PendingOpId for newly started operation. Callers must later
  // call CompletePendingOp() with this id to destroy the PendingOperation and
  // finalize timing UMA.
  PendingOpId StartPendingOp(std::string uma_str);

  // Removes PendingOperation from |pending_ops_| using |op_id_| as a key. This
  // destroys the object and triggers timing UMA.
  void CompletePendingOp(PendingOpId op_id);

  // Unified handler for timeouts of pending DB operations. PendingOperation
  // will be notified that it timed out (to trigger timing UMA) and removed from
  // |penidng_ops_|.
  void OnPendingOpTimeout(PendingOpId id);

  // Helper to report timing information for DB operations, including when they
  // hang indefinitely.
  class PendingOperation {
   public:
    PendingOperation(
        std::string uma_str,
        std::unique_ptr<base::CancelableOnceClosure> timeout_closure);
    // Records task timing UMA if it hasn't already timed out.
    virtual ~PendingOperation();

    // Copies disallowed. Incompatible with move-only members and UMA logging in
    // the destructor.
    PendingOperation(const PendingOperation&) = delete;
    PendingOperation& operator=(const PendingOperation&) = delete;

    // Trigger UMA recording for timeout.
    void OnTimeout();

   private:
    friend class VideoDecodeStatsDBImplTest;

    std::string uma_str_;
    std::unique_ptr<base::CancelableOnceClosure> timeout_closure_;
    base::TimeTicks start_ticks_;
  };

  // Map of operation id -> outstanding PendingOperations.
  base::flat_map<PendingOpId, std::unique_ptr<PendingOperation>> pending_ops_;

  // Called when the database has been initialized. Will immediately call
  // |init_cb| to forward |success|.
  void OnInit(PendingOpId id,
              InitializeCB init_cb,
              leveldb_proto::Enums::InitStatus status);

  // Returns true if the DB is successfully initialized.
  bool IsInitialized();

  // Passed as the callback for |OnGotDecodeStats| by |AppendDecodeStats| to
  // update the database once we've read the existing stats entry.
  void WriteUpdatedEntry(PendingOpId op_id,
                         const VideoDescKey& key,
                         const DecodeStatsEntry& entry,
                         AppendDecodeStatsCB append_done_cb,
                         bool read_success,
                         std::unique_ptr<DecodeStatsProto> stats_proto);

  // Called when the database has been modified after a call to
  // |WriteUpdatedEntry|. Will run |append_done_cb| when done.
  void OnEntryUpdated(PendingOpId op_id,
                      AppendDecodeStatsCB append_done_cb,
                      bool success);

  // Called when GetDecodeStats() operation was performed. |get_stats_cb|
  // will be run with |success| and a |DecodeStatsEntry| created from
  // |stats_proto| or nullptr if no entry was found for the requested key.
  void OnGotDecodeStats(PendingOpId op_id,
                        GetDecodeStatsCB get_stats_cb,
                        bool success,
                        std::unique_ptr<DecodeStatsProto> stats_proto);

  // Internal callback for OnLoadAllKeysForClearing(), initially triggered by
  // ClearStats(). Method simply logs |success| and runs |clear_done_cb|.
  void OnStatsCleared(PendingOpId op_id,
                      base::OnceClosure clear_done_cb,
                      bool success);

  // Return true if:
  //    values aren't corrupted nonsense (e.g. way more frames dropped than
  //    decoded, or number of frames_decoded < frames_power_efficient)
  // &&
  //    stats aren't expired.
  //       ("now" - stats_proto.last_write_date > GeMaxDaysToKeepStats())
  bool AreStatsUsable(const DecodeStatsProto* const stats_proto);

  void set_wall_clock_for_test(const base::Clock* tick_clock) {
    wall_clock_ = tick_clock;
  }

  // Next PendingOpId for use in |pending_ops_| map. See StartPendingOp().
  PendingOpId next_op_id_ = 0;

  // Indicates whether initialization is completed. Does not indicate whether it
  // was successful. Will be reset upon calling DestroyStats(). Failed
  // initialization is signaled by setting |db_| to null.
  bool db_init_ = false;

  // ProtoDatabase instance. Set to nullptr if fatal database error is
  // encountered.
  std::unique_ptr<leveldb_proto::ProtoDatabase<DecodeStatsProto>> db_;

  // For getting wall-clock time. Tests may override via SetClockForTest().
  const base::Clock* wall_clock_ = nullptr;

  // Stores parsed value of |kDefaultWriteTime|.
  base::Time default_write_time_;

  // Ensures all access to class members come on the same sequence. API calls
  // and callbacks should occur on the same sequence used during construction.
  // LevelDB operations happen on a separate task runner, but all LevelDB
  // callbacks to this happen on the checked sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VideoDecodeStatsDBImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoDecodeStatsDBImpl);
};

}  // namespace media

#endif  // MEDIA_CAPABILITIES_VIDEO_DECODE_STATS_DB_IMPL_H_
