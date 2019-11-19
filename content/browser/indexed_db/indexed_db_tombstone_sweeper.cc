// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_tombstone_sweeper.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"

namespace content {
namespace {

using StopReason = IndexedDBPreCloseTaskQueue::StopReason;
using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBObjectStoreMetadata;

}  // namespace

template <typename T>
WrappingIterator<T>::WrappingIterator() {}

template <typename T>
WrappingIterator<T>::WrappingIterator(const T* container,
                                      size_t start_position) {
  container_ = container;
  valid_ = true;
  iterations_done_ = 0;
  DCHECK_LT(start_position, container_->size());
  inner_ = container_->begin();
  std::advance(inner_, start_position);
  DCHECK(inner_ != container_->end());
}

template <typename T>
WrappingIterator<T>::~WrappingIterator() {}

template <typename T>
void WrappingIterator<T>::Next() {
  DCHECK(valid_);
  iterations_done_++;
  if (iterations_done_ >= container_->size()) {
    valid_ = false;
    return;
  }
  inner_++;
  if (inner_ == container_->end()) {
    inner_ = container_->begin();
  }
}

template <typename T>
const typename T::value_type& WrappingIterator<T>::Value() const {
  CHECK(valid_);
  return *inner_;
}

IndexedDBTombstoneSweeper::IndexedDBTombstoneSweeper(int round_iterations,
                                                     int max_iterations,
                                                     leveldb::DB* database)
    : max_round_iterations_(round_iterations),
      max_iterations_(max_iterations),
      database_(database) {
  sweep_state_.start_database_seed = static_cast<size_t>(base::RandUint64());
  sweep_state_.start_object_store_seed =
      static_cast<size_t>(base::RandUint64());
  sweep_state_.start_index_seed = static_cast<size_t>(base::RandUint64());
}

IndexedDBTombstoneSweeper::~IndexedDBTombstoneSweeper() {}

void IndexedDBTombstoneSweeper::SetMetadata(
    std::vector<IndexedDBDatabaseMetadata> const* metadata) {
  database_metadata_ = metadata;
  total_indices_ = 0;
  for (const auto& db : *metadata) {
    for (const auto& os_pair : db.object_stores) {
      total_indices_ += os_pair.second.indexes.size();
    }
  }
}

IndexedDBTombstoneSweeper::SweepState::SweepState() = default;

IndexedDBTombstoneSweeper::SweepState::~SweepState() = default;

void IndexedDBTombstoneSweeper::Stop(StopReason reason) {
  leveldb::Status s;
  RecordUMAStats(reason, base::nullopt, s);
}

bool IndexedDBTombstoneSweeper::RunRound() {
  DCHECK(database_metadata_);

  if (database_metadata_->empty())
    return true;

  if (!start_time_) {
    start_time_ = clock_for_testing_ ? clock_for_testing_->NowTicks()
                                     : base::TimeTicks::Now();
  }

  leveldb::Status s;
  Status status = DoSweep(&s);

  if (status != Status::DONE_ERROR) {
    s = FlushDeletions();
    if (!s.ok())
      status = Status::DONE_ERROR;
  }

  if (status == Status::SWEEPING)
    return false;

  RecordUMAStats(base::nullopt, status, s);
  return true;
}

void IndexedDBTombstoneSweeper::RecordUMAStats(
    base::Optional<StopReason> stop_reason,
    base::Optional<IndexedDBTombstoneSweeper::Status> status,
    const leveldb::Status& leveldb_error) {
  DCHECK(stop_reason || status);
  DCHECK(!stop_reason || !status);

  // Metadata error statistics are recorded in the PreCloseTaskList.
  if (stop_reason && stop_reason == StopReason::METADATA_ERROR)
    return;

  std::string uma_count_label =
      "WebCore.IndexedDB.TombstoneSweeper.NumDeletedTombstones.";
  std::string uma_size_label =
      "WebCore.IndexedDB.TombstoneSweeper.DeletedTombstonesSize.";

  if (stop_reason) {
    switch (stop_reason.value()) {
      case StopReason::NEW_CONNECTION:
        uma_count_label.append("ConnectionOpened");
        uma_size_label.append("ConnectionOpened");
        break;
      case StopReason::TIMEOUT:
        uma_count_label.append("TimeoutReached");
        uma_size_label.append("TimeoutReached");
        break;
      case StopReason::METADATA_ERROR:
        NOTREACHED();
        break;
    }
  } else if (status) {
    switch (status.value()) {
      case Status::DONE_REACHED_MAX:
        uma_count_label.append("MaxIterations");
        uma_size_label.append("MaxIterations");
        break;
      case Status::DONE_ERROR:
        base::UmaHistogramEnumeration(
            "WebCore.IndexedDB.TombstoneSweeper.SweepError",
            leveldb_env::GetLevelDBStatusUMAValue(leveldb_error),
            leveldb_env::LEVELDB_STATUS_MAX);
        uma_count_label.append("SweepError");
        uma_size_label.append("SweepError");
        break;
      case Status::DONE_COMPLETE:
        uma_count_label.append("Complete");
        uma_size_label.append("Complete");
        break;
      case Status::SWEEPING:
        NOTREACHED();
        break;
    }
  } else {
    NOTREACHED();
  }

  // Some stats are only recorded for completed runs.
  if (status && status.value() == Status::DONE_COMPLETE) {
    if (start_time_) {
      base::TimeDelta total_time =
          (clock_for_testing_ ? clock_for_testing_->NowTicks()
                              : base::TimeTicks::Now()) -
          start_time_.value();

      base::UmaHistogramTimes(
          "WebCore.IndexedDB.TombstoneSweeper.DeletionTotalTime.Complete",
          total_time);
      if (metrics_.seen_tombstones > 0) {
        // Only record deletion time if we do a deletion.
        base::UmaHistogramTimes(
            "WebCore.IndexedDB.TombstoneSweeper.DeletionCommitTime."
            "Complete",
            total_deletion_time_);
      }
    }
  }

  base::HistogramBase* count_histogram = base::Histogram::FactoryGet(
      uma_count_label, 1, 1'000'000, 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  // Range of 1 byte to 100 MB.
  base::HistogramBase* size_histogram = base::Histogram::FactoryGet(
      uma_size_label, 1, 100'000'000, 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);

  if (count_histogram)
    count_histogram->Add(metrics_.seen_tombstones);
  if (size_histogram)
    size_histogram->Add(metrics_.seen_tombstones_size);

  // We put our max at 20 instead of 100 to reduce the number of buckets.
  if (total_indices_ > 0) {
    static const int kIndexPercentageBucketCount = 20;
    base::UmaHistogramExactLinear(
        "WebCore.IndexedDB.TombstoneSweeper.IndexScanPercent",
        indices_scanned_ * kIndexPercentageBucketCount / total_indices_,
        kIndexPercentageBucketCount + 1);
  }
}

leveldb::Status IndexedDBTombstoneSweeper::FlushDeletions() {
  if (!has_writes_)
    return leveldb::Status::OK();
  base::TimeTicks start = base::TimeTicks::Now();

  leveldb::Status status =
      database_->Write(leveldb::WriteOptions(), &round_deletion_batch_);
  round_deletion_batch_.Clear();
  has_writes_ = false;

  if (!status.ok()) {
    base::UmaHistogramEnumeration(
        "WebCore.IndexedDB.TombstoneSweeper.DeletionWriteError",
        leveldb_env::GetLevelDBStatusUMAValue(status),
        leveldb_env::LEVELDB_STATUS_MAX);
    return status;
  }

  base::TimeDelta diff = base::TimeTicks::Now() - start;
  total_deletion_time_ += diff;
  return status;
}

bool IndexedDBTombstoneSweeper::ShouldContinueIteration(
    IndexedDBTombstoneSweeper::Status* sweep_status,
    leveldb::Status* leveldb_status,
    int* round_iterations) {
  ++num_iterations_;
  ++(*round_iterations);

  if (!iterator_->Valid()) {
    *leveldb_status = iterator_->status();
    if (!leveldb_status->ok()) {
      *sweep_status = Status::DONE_ERROR;
      return false;
    }
    *sweep_status = Status::SWEEPING;
    return true;
  }
  if (*round_iterations >= max_round_iterations_) {
    *sweep_status = Status::SWEEPING;
    return false;
  }
  if (num_iterations_ >= max_iterations_) {
    *sweep_status = Status::DONE_REACHED_MAX;
    return false;
  }
  return true;
}

IndexedDBTombstoneSweeper::Status IndexedDBTombstoneSweeper::DoSweep(
    leveldb::Status* leveldb_status) {
  int round_iterations = 0;
  Status sweep_status;
  if (database_metadata_->empty())
    return Status::DONE_COMPLETE;

  if (!iterator_) {
    leveldb::ReadOptions iterator_options;
    iterator_options.fill_cache = false;
    iterator_options.verify_checksums = true;
    iterator_.reset(database_->NewIterator(iterator_options));
  }

  if (!sweep_state_.database_it) {
    size_t start_database_idx = static_cast<size_t>(
        sweep_state_.start_database_seed % database_metadata_->size());
    sweep_state_.database_it = WrappingIterator<DatabaseMetadataVector>(
        database_metadata_, start_database_idx);
  }
  // Loop conditions facilitate starting at random index.
  for (; sweep_state_.database_it.value().IsValid();
       sweep_state_.database_it.value().Next()) {
    const IndexedDBDatabaseMetadata& database =
        sweep_state_.database_it.value().Value();
    if (database.object_stores.empty())
      continue;

    if (!sweep_state_.object_store_it) {
      size_t start_object_store_idx = static_cast<size_t>(
          sweep_state_.start_object_store_seed % database.object_stores.size());
      sweep_state_.object_store_it = WrappingIterator<ObjectStoreMetadataMap>(
          &database.object_stores, start_object_store_idx);
    }
    // Loop conditions facilitate starting at random index.
    for (; sweep_state_.object_store_it.value().IsValid();
         sweep_state_.object_store_it.value().Next()) {
      const IndexedDBObjectStoreMetadata& object_store =
          sweep_state_.object_store_it.value().Value().second;

      if (object_store.indexes.empty())
        continue;

      if (!sweep_state_.index_it) {
        size_t start_index_idx = static_cast<size_t>(
            sweep_state_.start_index_seed % object_store.indexes.size());
        sweep_state_.index_it = WrappingIterator<IndexMetadataMap>(
            &object_store.indexes, start_index_idx);
      }
      // Loop conditions facilitate starting at random index.
      for (; sweep_state_.index_it.value().IsValid();
           sweep_state_.index_it.value().Next()) {
        const IndexedDBIndexMetadata& index =
            sweep_state_.index_it.value().Value().second;

        bool can_continue =
            IterateIndex(database.id, object_store.id, index, &sweep_status,
                         leveldb_status, &round_iterations);
        if (!can_continue)
          return sweep_status;
      }
      sweep_state_.index_it = base::nullopt;
    }
    sweep_state_.object_store_it = base::nullopt;
  }
  return Status::DONE_COMPLETE;
}

bool IndexedDBTombstoneSweeper::IterateIndex(
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBIndexMetadata& index,
    IndexedDBTombstoneSweeper::Status* sweep_status,
    leveldb::Status* leveldb_status,
    int* round_iterations) {
  // If the sweeper exited early from an index scan, continue where it left off.
  if (sweep_state_.index_it_key) {
    iterator_->Seek(sweep_state_.index_it_key.value().Encode());
    if (!ShouldContinueIteration(sweep_status, leveldb_status,
                                 round_iterations)) {
      return false;
    }
    // Start at the first unvisited value.
    iterator_->Next();
    if (!ShouldContinueIteration(sweep_status, leveldb_status,
                                 round_iterations)) {
      return false;
    }
  } else {
    iterator_->Seek(
        IndexDataKey::EncodeMinKey(database_id, object_store_id, index.id));
    if (!ShouldContinueIteration(sweep_status, leveldb_status,
                                 round_iterations)) {
      return false;
    }
  }

  while (iterator_->Valid()) {
    leveldb::Slice key_slice = iterator_->key();
    base::StringPiece index_key_str = leveldb_env::MakeStringPiece(key_slice);
    size_t key_size = index_key_str.size();
    base::StringPiece index_value_str =
        leveldb_env::MakeStringPiece(iterator_->value());
    size_t value_size = index_value_str.size();
    // See if we've reached the end of the current index or all indexes.
    sweep_state_.index_it_key.emplace(IndexDataKey());
    if (!IndexDataKey::Decode(&index_key_str,
                              &sweep_state_.index_it_key.value()) ||
        sweep_state_.index_it_key.value().IndexId() != index.id) {
      break;
    }

    size_t entry_size = key_size + value_size;

    int64_t index_data_version;
    std::unique_ptr<IndexedDBKey> primary_key;

    if (!DecodeVarInt(&index_value_str, &index_data_version)) {
      ++metrics_.num_invalid_index_values;
      iterator_->Next();
      if (!ShouldContinueIteration(sweep_status, leveldb_status,
                                   round_iterations)) {
        return false;
      }
      continue;
    }
    std::string encoded_primary_key = index_value_str.as_string();
    std::string exists_key = ExistsEntryKey::Encode(
        database_id, object_store_id, encoded_primary_key);

    std::string exists_value;
    leveldb::Status s =
        database_->Get(leveldb::ReadOptions(), exists_key, &exists_value);
    if (!s.ok()) {
      ++metrics_.num_errors_reading_exists_table;
      iterator_->Next();
      if (!ShouldContinueIteration(sweep_status, leveldb_status,
                                   round_iterations)) {
        return false;
      }
      continue;
    }
    base::StringPiece exists_value_piece(exists_value);
    int64_t decoded_exists_version;
    if (!DecodeInt(&exists_value_piece, &decoded_exists_version) ||
        !exists_value_piece.empty()) {
      ++metrics_.num_invalid_exists_values;
      iterator_->Next();
      if (!ShouldContinueIteration(sweep_status, leveldb_status,
                                   round_iterations)) {
        return false;
      }
      continue;
    }

    if (decoded_exists_version != index_data_version) {
      has_writes_ = true;
      round_deletion_batch_.Delete(key_slice);
      ++metrics_.seen_tombstones;
      metrics_.seen_tombstones_size += entry_size;
    }

    iterator_->Next();
    if (!ShouldContinueIteration(sweep_status, leveldb_status,
                                 round_iterations)) {
      return false;
    }
  }
  ++indices_scanned_;
  sweep_state_.index_it_key = base::nullopt;
  return true;
}

}  // namespace content
