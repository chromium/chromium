// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capabilities/in_memory_video_decode_stats_db_impl.h"

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capabilities/video_decode_stats_db_impl.h"
#include "media/capabilities/video_decode_stats_db_provider.h"

namespace media {

InMemoryVideoDecodeStatsDBImpl::InMemoryVideoDecodeStatsDBImpl(
    VideoDecodeStatsDBProvider* seed_db_provider)
    : seed_db_provider_(seed_db_provider) {
  DVLOG(2) << __func__;
}

InMemoryVideoDecodeStatsDBImpl::~InMemoryVideoDecodeStatsDBImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (seed_db_)
    seed_db_->set_dependent_db(nullptr);
}

void InMemoryVideoDecodeStatsDBImpl::Initialize(InitializeCB init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(init_cb);
  DCHECK(!db_init_);

  // Fetch an *initialized* seed DB.
  if (seed_db_provider_) {
    seed_db_provider_->GetVideoDecodeStatsDB(
        base::BindOnce(&InMemoryVideoDecodeStatsDBImpl::OnGotSeedDB,
                       weak_ptr_factory_.GetWeakPtr(), std::move(init_cb)));
  } else {
    // No seed DB provider (e.g. guest session) means no work to do.
    DVLOG(2) << __func__ << " NO seed db";
    db_init_ = true;

    // Bind to avoid reentrancy.
    std::move(BindToCurrentLoop(std::move(init_cb))).Run(true);
  }
}

void InMemoryVideoDecodeStatsDBImpl::OnGotSeedDB(InitializeCB init_cb,
                                                 VideoDecodeStatsDB* db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__ << (db ? " has" : " null") << " seed db";

  db_init_ = true;

  CHECK(!seed_db_) << __func__ << " Already have a seed_db_?";
  seed_db_ = db;

  if (seed_db_)
    seed_db_->set_dependent_db(this);

  // Hard coding success = true. There are rare cases (e.g. disk corruption)
  // where an incognito profile may fail to acquire a reference to the base
  // profile's DB. But this just means incognito is in the same boat as guest
  // profiles (never have a seed DB) and is not a show stopper.
  std::move(init_cb).Run(true);
}

void InMemoryVideoDecodeStatsDBImpl::AppendDecodeStats(
    const VideoDescKey& key,
    const DecodeStatsEntry& entry,
    AppendDecodeStatsCB append_done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_init_);

  DVLOG(3) << __func__ << " Reading key " << key.ToLogString()
           << " from DB with intent to update with " << entry.ToLogString();

  auto it = in_memory_db_.find(key.Serialize());
  if (it == in_memory_db_.end()) {
    if (seed_db_) {
      // |seed_db_| exists and no in-memory entry is found for this key, means
      // we haven't checked the |seed_db_| yet. Query |seed_db_| and append new
      // stats to any seed values.
      seed_db_->GetDecodeStats(
          key, base::BindOnce(
                   &InMemoryVideoDecodeStatsDBImpl::CompleteAppendWithSeedData,
                   weak_ptr_factory_.GetWeakPtr(), key, entry,
                   std::move(append_done_cb)));
      return;
    }

    // Otherwise, these are the first stats for this key. Add a a copy of
    // |entry| to the database.
    in_memory_db_.emplace(key.Serialize(), entry);
  } else {
    // We've already asked the |seed_db_| for its data. Just add the new stats
    // to our local copy via the iterators reference.
    it->second += entry;
  }

  // Bind to avoid reentrancy.
  std::move(BindToCurrentLoop(std::move(append_done_cb))).Run(true);
}

void InMemoryVideoDecodeStatsDBImpl::GetDecodeStats(
    const VideoDescKey& key,
    GetDecodeStatsCB get_stats_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_init_);

  DVLOG(3) << __func__ << " " << key.ToLogString();

  auto it = in_memory_db_.find(key.Serialize());
  if (it == in_memory_db_.end()) {
    if (seed_db_) {
      // |seed_db_| exists and no in-memory entry is found for this key, means
      // we haven't checked the |seed_db_| yet.
      seed_db_->GetDecodeStats(
          key, base::BindOnce(&InMemoryVideoDecodeStatsDBImpl::OnGotSeedEntry,
                              weak_ptr_factory_.GetWeakPtr(), key,
                              std::move(get_stats_cb)));
    } else {
      // No seed data. Return an empty entry. Bind to avoid reentrancy.
      std::move(BindToCurrentLoop(std::move(get_stats_cb)))
          .Run(true, std::make_unique<DecodeStatsEntry>(0, 0, 0));
    }
  } else {
    // Return whatever what we found. Bind to avoid reentrancy.
    std::move(BindToCurrentLoop(std::move(get_stats_cb)))
        .Run(true, std::make_unique<DecodeStatsEntry>(it->second));
  }
}

void InMemoryVideoDecodeStatsDBImpl::CompleteAppendWithSeedData(
    const VideoDescKey& key,
    const DecodeStatsEntry& entry,
    AppendDecodeStatsCB append_done_cb,
    bool read_success,
    std::unique_ptr<DecodeStatsEntry> seed_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_init_);

  if (!read_success) {
    // Not a show stopper. Log it and carry on as if the seed DB were empty.
    DVLOG(2) << __func__ << " FAILED seed DB read for " << key.ToLogString();
    DCHECK(!seed_entry);
  }

  if (!seed_entry)
    seed_entry = std::make_unique<DecodeStatsEntry>(0, 0, 0);

  // Add new stats to the seed entry and store in memory.
  *seed_entry += entry;
  in_memory_db_.emplace(key.Serialize(), *seed_entry);

  DVLOG(3) << __func__ << " Updating " << key.ToLogString() << " with "
           << entry.ToLogString() << " aggregate:" << seed_entry->ToLogString();

  std::move(append_done_cb).Run(true);
}

void InMemoryVideoDecodeStatsDBImpl::OnGotSeedEntry(
    const VideoDescKey& key,
    GetDecodeStatsCB get_stats_cb,
    bool success,
    std::unique_ptr<DecodeStatsEntry> seed_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Failure is not a show stopper. Just a debug log...
  DVLOG(3) << __func__ << " read " << (success ? "succeeded" : "FAILED!")
           << " entry: " << (seed_entry ? seed_entry->ToLogString() : "null");

  if (!seed_entry)
    seed_entry = std::make_unique<DecodeStatsEntry>(0, 0, 0);

  // Always write to |in_memory_db_| to avoid querying |seed_db_| for this key
  // going forward.
  in_memory_db_.emplace(key.Serialize(), *seed_entry);

  std::move(get_stats_cb).Run(true, std::move(seed_entry));
}

void InMemoryVideoDecodeStatsDBImpl::ClearStats(
    base::OnceClosure destroy_done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__;

  // Really, this is not reachable code because user's can't clear the history
  // for a guest/incognito account. But if that ever changes, the reasonable
  // thing is to wipe only the |in_memory_db_|. |seed_db_| can be cleared by the
  // profile that owns it.
  in_memory_db_.clear();

  // Bind to avoid reentrancy.
  std::move(BindToCurrentLoop(std::move(destroy_done_cb))).Run();
}

}  // namespace media
