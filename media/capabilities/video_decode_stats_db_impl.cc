// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capabilities/video_decode_stats_db_impl.h"

#include <memory>
#include <tuple>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "media/capabilities/video_decode_stats.pb.h"

namespace media {

using ProtoDecodeStatsEntry = leveldb_proto::ProtoDatabase<DecodeStatsProto>;

namespace {

// Avoid changing client name. Used in UMA.
// See comments in components/leveldb_proto/leveldb_database.h
const char kDatabaseClientName[] = "VideoDecodeStatsDB";

};  // namespace

// static
std::unique_ptr<VideoDecodeStatsDBImpl> VideoDecodeStatsDBImpl::Create(
    base::FilePath db_dir) {
  DVLOG(2) << __func__ << " db_dir:" << db_dir;

  auto proto_db =
      std::make_unique<leveldb_proto::ProtoDatabaseImpl<DecodeStatsProto>>(
          base::CreateSequencedTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));

  return base::WrapUnique(
      new VideoDecodeStatsDBImpl(std::move(proto_db), db_dir));
}

constexpr char VideoDecodeStatsDBImpl::kDefaultWriteTime[];

VideoDecodeStatsDBImpl::VideoDecodeStatsDBImpl(
    std::unique_ptr<leveldb_proto::ProtoDatabase<DecodeStatsProto>> db,
    const base::FilePath& db_dir)
    : db_(std::move(db)),
      db_dir_(db_dir),
      wall_clock_(base::DefaultClock::GetInstance()),
      weak_ptr_factory_(this) {
  bool time_parsed =
      base::Time::FromString(kDefaultWriteTime, &default_write_time_);
  DCHECK(time_parsed);

  DCHECK(db_);
  DCHECK(!db_dir_.empty());
}

VideoDecodeStatsDBImpl::~VideoDecodeStatsDBImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoDecodeStatsDBImpl::Initialize(InitializeCB init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(init_cb);
  DCHECK(!IsInitialized());

  // "Simple options" will use the default global cache of 8MB. In the worst
  // case our whole DB will be less than 35K, so we aren't worried about
  // spamming the cache.
  // TODO(chcunningham): Keep an eye on the size as the table evolves.
  db_->Init(kDatabaseClientName, db_dir_, leveldb_proto::CreateSimpleOptions(),
            base::BindOnce(&VideoDecodeStatsDBImpl::OnInit,
                           weak_ptr_factory_.GetWeakPtr(), std::move(init_cb)));
}

void VideoDecodeStatsDBImpl::OnInit(InitializeCB init_cb, bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__ << (success ? " succeeded" : " FAILED!");
  UMA_HISTOGRAM_BOOLEAN("Media.VideoDecodeStatsDB.OpSuccess.Initialize",
                        success);

  db_init_ = true;

  // Can't use DB when initialization fails.
  if (!success)
    db_.reset();

  std::move(init_cb).Run(success);
}

bool VideoDecodeStatsDBImpl::IsInitialized() {
  // |db_| will be null if Initialization failed.
  return db_init_ && db_;
}

void VideoDecodeStatsDBImpl::AppendDecodeStats(
    const VideoDescKey& key,
    const DecodeStatsEntry& entry,
    AppendDecodeStatsCB append_done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized());

  DVLOG(3) << __func__ << " Reading key " << key.ToLogString()
           << " from DB with intent to update with " << entry.ToLogString();

  db_->GetEntry(key.Serialize(),
                base::BindOnce(&VideoDecodeStatsDBImpl::WriteUpdatedEntry,
                               weak_ptr_factory_.GetWeakPtr(), key, entry,
                               std::move(append_done_cb)));
}

void VideoDecodeStatsDBImpl::GetDecodeStats(const VideoDescKey& key,
                                            GetDecodeStatsCB get_stats_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized());

  DVLOG(3) << __func__ << " " << key.ToLogString();

  db_->GetEntry(
      key.Serialize(),
      base::BindOnce(&VideoDecodeStatsDBImpl::OnGotDecodeStats,
                     weak_ptr_factory_.GetWeakPtr(), std::move(get_stats_cb)));
}

bool VideoDecodeStatsDBImpl::AreStatsExpired(
    const DecodeStatsProto* const stats_proto) {
  double last_write_date = stats_proto->last_write_date();
  if (last_write_date == 0) {
    // Set a default time if the write date is zero (no write since proto was
    // updated to include the time stamp).
    last_write_date = default_write_time_.ToJsTime();
  }

  return wall_clock_->Now() - base::Time::FromJsTime(last_write_date) >
         base::TimeDelta::FromDays(VideoDecodeStatsDBImpl::kMaxDaysToKeepStats);
}

void VideoDecodeStatsDBImpl::WriteUpdatedEntry(
    const VideoDescKey& key,
    const DecodeStatsEntry& new_entry,
    AppendDecodeStatsCB append_done_cb,
    bool read_success,
    std::unique_ptr<DecodeStatsProto> stats_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized());

  // Note: outcome of "Write" operation logged in OnEntryUpdated().
  UMA_HISTOGRAM_BOOLEAN("Media.VideoDecodeStatsDB.OpSuccess.Read",
                        read_success);

  if (!read_success) {
    DVLOG(2) << __func__ << " FAILED DB read for " << key.ToLogString()
             << "; ignoring update!";
    std::move(append_done_cb).Run(false);
    return;
  }

  if (!stats_proto || AreStatsExpired(stats_proto.get())) {
    // Default instance will have all zeros for numeric types.
    stats_proto.reset(new DecodeStatsProto());
  }

  uint64_t old_frames_decoded = stats_proto->frames_decoded();
  uint64_t old_frames_dropped = stats_proto->frames_dropped();
  uint64_t old_frames_power_efficient = stats_proto->frames_power_efficient();

  if (old_frames_decoded + new_entry.frames_decoded > kMaxFramesPerBuffer) {
    // The |new_entry| is pushing out some or all of the old data. Achieve this
    // by weighting the dropped and power efficiency stats by the ratio of the
    // the buffer that new entry fills.
    double fill_ratio = std::min(
        static_cast<double>(new_entry.frames_decoded) / kMaxFramesPerBuffer,
        1.0);

    double old_dropped_ratio =
        static_cast<double>(old_frames_dropped) / old_frames_decoded;
    double old_efficient_ratio =
        static_cast<double>(old_frames_power_efficient) / old_frames_decoded;
    double new_entry_dropped_ratio =
        static_cast<double>(new_entry.frames_dropped) /
        new_entry.frames_decoded;
    double new_entry_efficient_ratio =
        static_cast<double>(new_entry.frames_power_efficient) /
        new_entry.frames_decoded;

    double agg_dropped_ratio = fill_ratio * new_entry_dropped_ratio +
                               (1 - fill_ratio) * old_dropped_ratio;
    double agg_efficient_ratio = fill_ratio * new_entry_efficient_ratio +
                                 (1 - fill_ratio) * old_efficient_ratio;

    stats_proto->set_frames_decoded(kMaxFramesPerBuffer);
    stats_proto->set_frames_dropped(
        std::round(agg_dropped_ratio * kMaxFramesPerBuffer));
    stats_proto->set_frames_power_efficient(
        std::round(agg_efficient_ratio * kMaxFramesPerBuffer));
  } else {
    // Adding |new_entry| does not exceed |kMaxFramesPerfBuffer|. Simply sum the
    // stats.
    stats_proto->set_frames_decoded(new_entry.frames_decoded +
                                    old_frames_decoded);
    stats_proto->set_frames_dropped(new_entry.frames_dropped +
                                    old_frames_dropped);
    stats_proto->set_frames_power_efficient(new_entry.frames_power_efficient +
                                            old_frames_power_efficient);
  }

  // Update the time stamp for the current write.
  stats_proto->set_last_write_date(wall_clock_->Now().ToJsTime());

  // Push the update to the DB.
  using DBType = leveldb_proto::ProtoDatabase<DecodeStatsProto>;
  std::unique_ptr<DBType::KeyEntryVector> entries =
      std::make_unique<DBType::KeyEntryVector>();
  entries->emplace_back(key.Serialize(), *stats_proto);
  db_->UpdateEntries(std::move(entries),
                     std::make_unique<leveldb_proto::KeyVector>(),
                     base::BindOnce(&VideoDecodeStatsDBImpl::OnEntryUpdated,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(append_done_cb)));
}

void VideoDecodeStatsDBImpl::OnEntryUpdated(AppendDecodeStatsCB append_done_cb,
                                            bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UMA_HISTOGRAM_BOOLEAN("Media.VideoDecodeStatsDB.OpSuccess.Write", success);
  DVLOG(3) << __func__ << " update " << (success ? "succeeded" : "FAILED!");
  std::move(append_done_cb).Run(success);
}

void VideoDecodeStatsDBImpl::OnGotDecodeStats(
    GetDecodeStatsCB get_stats_cb,
    bool success,
    std::unique_ptr<DecodeStatsProto> stats_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UMA_HISTOGRAM_BOOLEAN("Media.VideoDecodeStatsDB.OpSuccess.Read", success);

  std::unique_ptr<DecodeStatsEntry> entry;

  if (stats_proto && !AreStatsExpired(stats_proto.get())) {
    DCHECK(success);

    entry = std::make_unique<DecodeStatsEntry>(
        stats_proto->frames_decoded(), stats_proto->frames_dropped(),
        stats_proto->frames_power_efficient());
  }

  DVLOG(3) << __func__ << " read " << (success ? "succeeded" : "FAILED!")
           << " entry: " << (entry ? entry->ToLogString() : "nullptr");

  std::move(get_stats_cb).Run(success, std::move(entry));
}

void VideoDecodeStatsDBImpl::ClearStats(base::OnceClosure clear_done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__;

  db_->LoadKeys(
      base::BindOnce(&VideoDecodeStatsDBImpl::OnLoadAllKeysForClearing,
                     weak_ptr_factory_.GetWeakPtr(), std::move(clear_done_cb)));
}

void VideoDecodeStatsDBImpl::OnLoadAllKeysForClearing(
    base::OnceClosure clear_done_cb,
    bool success,
    std::unique_ptr<std::vector<std::string>> keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__ << (success ? " succeeded" : " FAILED!");

  UMA_HISTOGRAM_BOOLEAN("Media.VideoDecodeStatsDB.OpSuccess.LoadKeys", success);

  if (success) {
    // Remove all keys.
    db_->UpdateEntries(
        std::make_unique<ProtoDecodeStatsEntry::KeyEntryVector>(),
        std::move(keys) /* keys_to_remove */,
        base::BindOnce(&VideoDecodeStatsDBImpl::OnStatsCleared,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(clear_done_cb)));
  } else {
    // Fail silently. See comment in OnStatsCleared().
    std::move(clear_done_cb).Run();
  }
}

void VideoDecodeStatsDBImpl::OnStatsCleared(base::OnceClosure clear_done_cb,
                                            bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__ << (success ? " succeeded" : " FAILED!");

  UMA_HISTOGRAM_BOOLEAN("Media.VideoDecodeStatsDB.OpSuccess.Destroy", success);

  // We don't pass success to |clear_done_cb|. Clearing is best effort and
  // there is no additional action for callers to take in case of failure.
  // TODO(chcunningham): Monitor UMA and consider more aggressive action like
  // deleting the DB directory.
  std::move(clear_done_cb).Run();
}

}  // namespace media
