// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capabilities/webrtc_video_stats_db_impl.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "media/base/media_switches.h"
#include "media/capabilities/webrtc_video_stats.pb.h"

namespace media {

using ProtoVideoStatsEntry =
    leveldb_proto::ProtoDatabase<WebrtcVideoStatsEntryProto>;

// static
std::unique_ptr<WebrtcVideoStatsDBImpl> WebrtcVideoStatsDBImpl::Create(
    base::FilePath db_dir,
    leveldb_proto::ProtoDatabaseProvider* db_provider) {
  DVLOG(2) << __func__ << " db_dir:" << db_dir;

  auto proto_db = db_provider->GetDB<WebrtcVideoStatsEntryProto>(
      leveldb_proto::ProtoDbType::WEBRTC_VIDEO_STATS_DB, db_dir,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));

  return base::WrapUnique(new WebrtcVideoStatsDBImpl(std::move(proto_db)));
}

WebrtcVideoStatsDBImpl::WebrtcVideoStatsDBImpl(
    std::unique_ptr<leveldb_proto::ProtoDatabase<WebrtcVideoStatsEntryProto>>
        db)
    : pending_operations_(/*uma_prefix=*/"Media.WebrtcVideoStatsDB.OpTiming."),
      db_(std::move(db)),
      wall_clock_(base::DefaultClock::GetInstance()) {
  DCHECK(db_);
}

WebrtcVideoStatsDBImpl::~WebrtcVideoStatsDBImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebrtcVideoStatsDBImpl::Initialize(InitializeCB init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(init_cb);
  DCHECK(!IsInitialized());

  db_->Init(base::BindOnce(
      &WebrtcVideoStatsDBImpl::OnInit, weak_ptr_factory_.GetWeakPtr(),
      pending_operations_.Start("Initialize"), std::move(init_cb)));
}

void WebrtcVideoStatsDBImpl::OnInit(PendingOperations::Id op_id,
                                    InitializeCB init_cb,
                                    leveldb_proto::Enums::InitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(status, leveldb_proto::Enums::InitStatus::kInvalidOperation);
  bool success = status == leveldb_proto::Enums::InitStatus::kOK;
  DVLOG(2) << __func__ << (success ? " succeeded" : " FAILED!");
  pending_operations_.Complete(op_id);
  UMA_HISTOGRAM_BOOLEAN("Media.WebrtcVideoStatsDB.OpSuccess.Initialize",
                        success);

  db_init_ = true;

  // Can't use DB when initialization fails.
  if (!success)
    db_.reset();

  std::move(init_cb).Run(success);
}

bool WebrtcVideoStatsDBImpl::IsInitialized() {
  // `db_` will be null if Initialization failed.
  return db_init_ && db_;
}

void WebrtcVideoStatsDBImpl::AppendVideoStats(
    const VideoDescKey& key,
    const VideoStats& video_stats,
    AppendVideoStatsCB append_done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized());

  DVLOG(3) << __func__ << " Reading " << key.ToLogStringForDebug()
           << " from DB with intent to update with "
           << video_stats.ToLogString();

  db_->GetEntry(key.Serialize(),
                base::BindOnce(&WebrtcVideoStatsDBImpl::WriteUpdatedEntry,
                               weak_ptr_factory_.GetWeakPtr(),
                               pending_operations_.Start("Read"), key,
                               video_stats, std::move(append_done_cb)));
}

void WebrtcVideoStatsDBImpl::GetVideoStats(const VideoDescKey& key,
                                           GetVideoStatsCB get_stats_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized());

  DVLOG(3) << __func__ << " " << key.ToLogStringForDebug();

  db_->GetEntry(key.Serialize(),
                base::BindOnce(&WebrtcVideoStatsDBImpl::OnGotVideoStats,
                               weak_ptr_factory_.GetWeakPtr(),
                               pending_operations_.Start("Read"),
                               std::move(get_stats_cb)));
}

void WebrtcVideoStatsDBImpl::GetVideoStatsCollection(
    const VideoDescKey& key,
    GetVideoStatsCollectionCB get_stats_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized());

  DVLOG(3) << __func__ << " " << key.ToLogStringForDebug();

  // Filter out all entries starting as the serialized key without pixels. This
  // corresponds to all entries with the same codec profile, hardware
  // accelerate, and decode/encode.
  std::string key_without_pixels = key.SerializeWithoutPixels();
  auto key_iterator_controller = base::BindRepeating(
      [](const std::string& key_filter, const std::string& key) {
        if (base::StartsWith(key, key_filter)) {
          // Include this entry and continue the search if the key has the
          // same beginning as `key_without_pixels`.
          return leveldb_proto::Enums::kLoadAndContinue;
        } else {
          // Cancel otherwise.
          return leveldb_proto::Enums::kSkipAndStop;
        }
      },
      key_without_pixels);

  db_->LoadKeysAndEntriesWhile(
      key_without_pixels, key_iterator_controller,
      base::BindOnce(&WebrtcVideoStatsDBImpl::OnGotVideoStatsCollection,
                     weak_ptr_factory_.GetWeakPtr(),
                     pending_operations_.Start("Read"),
                     std::move(get_stats_cb)));
}

bool WebrtcVideoStatsDBImpl::AreStatsValid(
    const WebrtcVideoStatsEntryProto* const stats_proto) {
  // Check for corruption.
  bool are_stats_valid = stats_proto->stats_size() > 0 &&
                         stats_proto->stats_size() <= GetMaxEntriesPerConfig();

  // Verify each entry.
  double previous_timestamp = std::numeric_limits<double>::max();
  for (auto const& stats_entry : stats_proto->stats()) {
    // The stats are ordered with the latest entry first.
    are_stats_valid &= previous_timestamp > stats_entry.timestamp();
    are_stats_valid &=
        stats_entry.frames_processed() >= kFramesProcessedMinValue &&
        stats_entry.frames_processed() <= kFramesProcessedMaxValue;
    are_stats_valid &=
        stats_entry.frames_processed() >= stats_entry.key_frames_processed();
    are_stats_valid &=
        stats_entry.p99_processing_time_ms() >= kP99ProcessingTimeMinValueMs &&
        stats_entry.p99_processing_time_ms() <= kP99ProcessingTimeMaxValueMs;
    previous_timestamp = stats_entry.timestamp();
  }

  UMA_HISTOGRAM_BOOLEAN("Media.WebrtcVideoStatsDB.OpSuccess.Validate",
                        are_stats_valid);
  return are_stats_valid;
}

void WebrtcVideoStatsDBImpl::WriteUpdatedEntry(
    PendingOperations::Id op_id,
    const VideoDescKey& key,
    const VideoStats& new_video_stats,
    AppendVideoStatsCB append_done_cb,
    bool read_success,
    std::unique_ptr<WebrtcVideoStatsEntryProto> existing_entry_proto) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized());
  pending_operations_.Complete(op_id);

  // Note: outcome of "Write" operation logged in OnEntryUpdated().
  UMA_HISTOGRAM_BOOLEAN("Media.WebrtcVideoStatsDB.OpSuccess.Read",
                        read_success);

  if (!read_success) {
    DVLOG(2) << __func__ << " FAILED DB read for " << key.ToLogStringForDebug()
             << "; ignoring update!";
    std::move(append_done_cb).Run(false);
    return;
  }

  if (!existing_entry_proto || !AreStatsValid(existing_entry_proto.get())) {
    // Default instance will not have any stats entries.
    existing_entry_proto = std::make_unique<WebrtcVideoStatsEntryProto>();
  }

  // Create a new entry, with new stats in the front and copy any existing stats
  // to the back.
  WebrtcVideoStatsEntryProto new_entry_proto;
  media::WebrtcVideoStatsProto* new_stats = new_entry_proto.add_stats();
  DCHECK(new_stats);
  new_stats->set_frames_processed(new_video_stats.frames_processed);
  new_stats->set_key_frames_processed(new_video_stats.key_frames_processed);
  new_stats->set_p99_processing_time_ms(new_video_stats.p99_processing_time_ms);
  new_stats->set_timestamp(
      wall_clock_->Now().InMillisecondsFSinceUnixEpochIgnoringNull());

  DVLOG(3) << "Adding new stats entry:" << new_stats->timestamp() << ", "
           << new_stats->frames_processed() << ",  "
           << new_stats->key_frames_processed() << ", "
           << new_stats->p99_processing_time_ms();

  // Append existing entries.
  const base::TimeDelta max_time_to_keep_stats = GetMaxTimeToKeepStats();
  const int max_entries_per_config = GetMaxEntriesPerConfig();
  DCHECK_GT(max_time_to_keep_stats, base::Days(0));
  double previous_timestamp = new_stats->timestamp();
  for (auto const& existing_stats : existing_entry_proto->stats()) {
    // Discard existing stats that have expired, if the entry is full, or if the
    // timestamps come in the wrong order.
    if (wall_clock_->Now() - base::Time::FromMillisecondsSinceUnixEpoch(
                                 existing_stats.timestamp()) <=
            max_time_to_keep_stats &&
        new_entry_proto.stats_size() < max_entries_per_config &&
        existing_stats.timestamp() < previous_timestamp) {
      previous_timestamp = existing_stats.timestamp();
      media::WebrtcVideoStatsProto* stats = new_entry_proto.add_stats();
      DCHECK(stats);
      *stats = existing_stats;
      DVLOG(3) << " appending existing stats:" << existing_stats.timestamp()
               << ", " << existing_stats.frames_processed() << ",  "
               << existing_stats.key_frames_processed() << ", "
               << existing_stats.p99_processing_time_ms();
    }
  }

  // Make sure we never write bogus stats into the DB! While its possible the DB
  // may experience some corruption (disk), we should have detected that above
  // and discarded any bad data prior to this upcoming save.
  DCHECK(AreStatsValid(&new_entry_proto));

  // Push the update to the DB.
  using DBType = leveldb_proto::ProtoDatabase<WebrtcVideoStatsEntryProto>;
  std::unique_ptr<DBType::KeyEntryVector> entries =
      std::make_unique<DBType::KeyEntryVector>();
  entries->emplace_back(key.Serialize(), new_entry_proto);
  db_->UpdateEntries(std::move(entries),
                     std::make_unique<leveldb_proto::KeyVector>(),
                     base::BindOnce(&WebrtcVideoStatsDBImpl::OnEntryUpdated,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    pending_operations_.Start("Write"),
                                    std::move(append_done_cb)));
}

void WebrtcVideoStatsDBImpl::OnEntryUpdated(PendingOperations::Id op_id,
                                            AppendVideoStatsCB append_done_cb,
                                            bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__ << " update " << (success ? "succeeded" : "FAILED!");
  pending_operations_.Complete(op_id);
  UMA_HISTOGRAM_BOOLEAN("Media.WebrtcVideoStatsDB.OpSuccess.Write", success);
  std::move(append_done_cb).Run(success);
}

void WebrtcVideoStatsDBImpl::OnGotVideoStats(
    PendingOperations::Id op_id,
    GetVideoStatsCB get_stats_cb,
    bool success,
    std::unique_ptr<WebrtcVideoStatsEntryProto> stats_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__ << " get " << (success ? "succeeded" : "FAILED!");
  pending_operations_.Complete(op_id);
  UMA_HISTOGRAM_BOOLEAN("Media.WebrtcVideoStatsDB.OpSuccess.Read", success);

  // Convert from WebrtcVideoStatsEntryProto to VideoStatsEntry.
  std::optional<VideoStatsEntry> entry;
  if (stats_proto && AreStatsValid(stats_proto.get())) {
    DCHECK(success);
    const base::TimeDelta max_time_to_keep_stats = GetMaxTimeToKeepStats();
    entry.emplace();
    for (auto const& stats : stats_proto->stats()) {
      if (wall_clock_->Now() -
              base::Time::FromMillisecondsSinceUnixEpoch(stats.timestamp()) <=
          max_time_to_keep_stats) {
        entry->emplace_back(stats.timestamp(), stats.frames_processed(),
                            stats.key_frames_processed(),
                            stats.p99_processing_time_ms());
      }
    }

    // Clear the pointer if all stats were expired.
    if (entry->size() == 0) {
      entry.reset();
    }
  }

  std::move(get_stats_cb).Run(success, std::move(entry));
}

void WebrtcVideoStatsDBImpl::OnGotVideoStatsCollection(
    PendingOperations::Id op_id,
    GetVideoStatsCollectionCB get_stats_cb,
    bool success,
    std::unique_ptr<std::map<std::string, WebrtcVideoStatsEntryProto>>
        stats_proto_collection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__ << " get " << (success ? "succeeded" : "FAILED!");
  pending_operations_.Complete(op_id);
  UMA_HISTOGRAM_BOOLEAN("Media.WebrtcVideoStatsDB.OpSuccess.Read", success);
  // Convert from map of WebrtcVideoStatsEntryProto to VideoStatsCollection.
  std::optional<VideoStatsCollection> collection;
  if (stats_proto_collection) {
    DCHECK(success);
    collection.emplace();
    const base::TimeDelta max_time_to_keep_stats = GetMaxTimeToKeepStats();

    for (auto const& [pixel_key, video_stats_entry] : *stats_proto_collection) {
      if (AreStatsValid(&video_stats_entry)) {
        VideoStatsEntry entry;
        for (auto const& stats : video_stats_entry.stats()) {
          if (wall_clock_->Now() - base::Time::FromMillisecondsSinceUnixEpoch(
                                       stats.timestamp()) <=
              max_time_to_keep_stats) {
            entry.emplace_back(stats.timestamp(), stats.frames_processed(),
                               stats.key_frames_processed(),
                               stats.p99_processing_time_ms());
          }
        }

        if (!entry.empty()) {
          std::optional<int> pixels =
              VideoDescKey::ParsePixelsFromKey(pixel_key);
          if (pixels) {
            collection->insert({*pixels, std::move(entry)});
          }
        }
      }
    }
    if (collection->empty()) {
      collection.reset();
    }
  }

  std::move(get_stats_cb).Run(success, std::move(collection));
}

void WebrtcVideoStatsDBImpl::ClearStats(base::OnceClosure clear_done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__;

  db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<ProtoVideoStatsEntry::KeyEntryVector>(),
      base::BindRepeating([](const std::string&) { return true; }),
      base::BindOnce(&WebrtcVideoStatsDBImpl::OnStatsCleared,
                     weak_ptr_factory_.GetWeakPtr(),
                     pending_operations_.Start("Clear"),
                     std::move(clear_done_cb)));
}

void WebrtcVideoStatsDBImpl::OnStatsCleared(PendingOperations::Id op_id,
                                            base::OnceClosure clear_done_cb,
                                            bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__ << (success ? " succeeded" : " FAILED!");

  pending_operations_.Complete(op_id);

  UMA_HISTOGRAM_BOOLEAN("Media.WebrtcVideoStatsDB.OpSuccess.Clear", success);

  // We don't pass success to `clear_done_cb`. Clearing is best effort and
  // there is no additional action for callers to take in case of failure.
  std::move(clear_done_cb).Run();
}

}  // namespace media
