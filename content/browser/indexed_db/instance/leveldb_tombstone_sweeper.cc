// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/leveldb_tombstone_sweeper.h"

#include <string>
#include <string_view>

#include "base/not_fatal_until.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"

namespace content::indexed_db {
namespace {

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
  CHECK(inner_ != container_->end(), base::NotFatalUntil::M130);
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

LevelDbTombstoneSweeper::LevelDbTombstoneSweeper(int round_iterations,
                                                 int max_iterations,
                                                 leveldb::DB* database)
    : BackingStorePreCloseTaskQueue::PreCloseTask(database),
      max_round_iterations_(round_iterations),
      max_iterations_(max_iterations) {
  sweep_state_.start_database_seed = static_cast<size_t>(base::RandUint64());
  sweep_state_.start_object_store_seed =
      static_cast<size_t>(base::RandUint64());
  sweep_state_.start_index_seed = static_cast<size_t>(base::RandUint64());
}

LevelDbTombstoneSweeper::~LevelDbTombstoneSweeper() {}

bool LevelDbTombstoneSweeper::RequiresMetadata() const {
  return true;
}

void LevelDbTombstoneSweeper::SetMetadata(
    const std::vector<IndexedDBDatabaseMetadata>* metadata) {
  database_metadata_ = metadata;
  total_indices_ = 0;
  for (const auto& db : *metadata) {
    for (const auto& os_pair : db.object_stores) {
      total_indices_ += os_pair.second.indexes.size();
    }
  }
}

LevelDbTombstoneSweeper::SweepState::SweepState() = default;

LevelDbTombstoneSweeper::SweepState::~SweepState() = default;

bool LevelDbTombstoneSweeper::RunRound() {
  DCHECK(database_metadata_);

  if (database_metadata_->empty()) {
    return true;
  }

  Status s;
  SweepStatus status = DoSweep(&s);

  if (status != SweepStatus::DONE_ERROR) {
    s = FlushDeletions();
    if (!s.ok()) {
      status = SweepStatus::DONE_ERROR;
    }
  }

  return status != SweepStatus::SWEEPING;
}

Status LevelDbTombstoneSweeper::FlushDeletions() {
  if (!has_writes_) {
    return Status::OK();
  }

  Status status(
      database()->Write(leveldb::WriteOptions(), &round_deletion_batch_));
  round_deletion_batch_.Clear();
  has_writes_ = false;
  return status;
}

bool LevelDbTombstoneSweeper::ShouldContinueIteration(
    LevelDbTombstoneSweeper::SweepStatus* sweep_status,
    Status* leveldb_status,
    int* round_iterations) {
  ++num_iterations_;
  ++(*round_iterations);

  if (!iterator_->Valid()) {
    *leveldb_status = iterator_->status();
    if (!leveldb_status->ok()) {
      *sweep_status = SweepStatus::DONE_ERROR;
      return false;
    }
    *sweep_status = SweepStatus::SWEEPING;
    return true;
  }
  if (*round_iterations >= max_round_iterations_) {
    *sweep_status = SweepStatus::SWEEPING;
    return false;
  }
  if (num_iterations_ >= max_iterations_) {
    *sweep_status = SweepStatus::DONE;
    return false;
  }
  return true;
}

LevelDbTombstoneSweeper::SweepStatus LevelDbTombstoneSweeper::DoSweep(
    Status* leveldb_status) {
  int round_iterations = 0;
  SweepStatus sweep_status;
  if (database_metadata_->empty()) {
    return SweepStatus::DONE;
  }

  if (!iterator_) {
    leveldb::ReadOptions iterator_options;
    iterator_options.fill_cache = false;
    iterator_options.verify_checksums = true;
    iterator_.reset(database()->NewIterator(iterator_options));
  }

  if (!sweep_state_.database_it) {
    size_t start_database_idx = static_cast<size_t>(
        sweep_state_.start_database_seed % database_metadata_->size());
    sweep_state_.database_it =
        WrappingIterator<IndexedDBDatabaseMetadataVector>(
            database_metadata_.get(), start_database_idx);
  }
  // Loop conditions facilitate starting at random index.
  for (; sweep_state_.database_it.value().IsValid();
       sweep_state_.database_it.value().Next()) {
    const IndexedDBDatabaseMetadata& database =
        sweep_state_.database_it.value().Value();
    if (database.object_stores.empty()) {
      continue;
    }

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

      if (object_store.indexes.empty()) {
        continue;
      }

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
        if (!can_continue) {
          return sweep_status;
        }
      }
      sweep_state_.index_it = std::nullopt;
    }
    sweep_state_.object_store_it = std::nullopt;
  }
  return SweepStatus::DONE;
}

bool LevelDbTombstoneSweeper::IterateIndex(
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBIndexMetadata& index,
    LevelDbTombstoneSweeper::SweepStatus* sweep_status,
    Status* leveldb_status,
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
    std::string_view index_key_str = leveldb_env::MakeStringView(key_slice);
    std::string_view index_value_str =
        leveldb_env::MakeStringView(iterator_->value());
    // See if we've reached the end of the current index or all indexes.
    sweep_state_.index_it_key.emplace(IndexDataKey());
    if (!IndexDataKey::Decode(&index_key_str,
                              &sweep_state_.index_it_key.value()) ||
        sweep_state_.index_it_key.value().IndexId() != index.id) {
      break;
    }

    int64_t index_data_version;
    std::unique_ptr<IndexedDBKey> primary_key;

    if (!DecodeVarInt(&index_value_str, &index_data_version)) {
      iterator_->Next();
      if (!ShouldContinueIteration(sweep_status, leveldb_status,
                                   round_iterations)) {
        return false;
      }
      continue;
    }
    std::string encoded_primary_key(index_value_str);
    std::string exists_key = ExistsEntryKey::Encode(
        database_id, object_store_id, encoded_primary_key);

    std::string exists_value;
    Status s(
        database()->Get(leveldb::ReadOptions(), exists_key, &exists_value));
    if (!s.ok()) {
      iterator_->Next();
      if (!ShouldContinueIteration(sweep_status, leveldb_status,
                                   round_iterations)) {
        return false;
      }
      continue;
    }
    std::string_view exists_value_piece(exists_value);
    int64_t decoded_exists_version;
    if (!DecodeInt(&exists_value_piece, &decoded_exists_version) ||
        !exists_value_piece.empty()) {
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
    }

    iterator_->Next();
    if (!ShouldContinueIteration(sweep_status, leveldb_status,
                                 round_iterations)) {
      return false;
    }
  }
  ++indices_scanned_;
  sweep_state_.index_it_key = std::nullopt;
  return true;
}

}  // namespace content::indexed_db
