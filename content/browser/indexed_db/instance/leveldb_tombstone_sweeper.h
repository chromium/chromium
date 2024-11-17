// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_LEVELDB_TOMBSTONE_SWEEPER_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_LEVELDB_TOMBSTONE_SWEEPER_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/instance/backing_store_pre_close_task_queue.h"
#include "content/browser/indexed_db/status.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace blink {
struct IndexedDBDatabaseMetadata;
struct IndexedDBIndexMetadata;
struct IndexedDBObjectStoreMetadata;
}  // namespace blink

namespace leveldb {
class DB;
class Iterator;
}  // namespace leveldb

namespace content::indexed_db {
class BackingStore;

// Facilitates iterating a whole container with an abnormal starting position.
// If the starting position is not 0, then the iteration will wrap to the
// beginning of the container until the starting position is reached again.
template <typename T>
class WrappingIterator {
 public:
  WrappingIterator();
  WrappingIterator(const T* container, size_t start_position);
  ~WrappingIterator();
  WrappingIterator& operator=(const WrappingIterator& other) = default;

  void Next();
  bool IsValid() const { return valid_; }
  const typename T::value_type& Value() const;

 private:
  bool valid_ = false;
  size_t iterations_done_ = 0;
  typename T::const_iterator inner_;
  raw_ptr<const T> container_ = nullptr;
};

// Sweeps the IndexedDB leveldb database looking for index tombstones. These
// occur when the indexed fields of rows are modified, and stay around if script
// doesn't do a cursor iteration of the database.
//
// Owned by the BackingStore.
//
// TODO(dmurph) Describe this class in a README.md file.
// See bit.ly/idb-tombstone-sweeper for more information.
class CONTENT_EXPORT LevelDbTombstoneSweeper
    : public BackingStorePreCloseTaskQueue::PreCloseTask {
 public:
  // The |database| must outlive this instance.
  LevelDbTombstoneSweeper(int round_iterations,
                          int max_iterations,
                          leveldb::DB* database);

  LevelDbTombstoneSweeper(const LevelDbTombstoneSweeper&) = delete;
  LevelDbTombstoneSweeper& operator=(const LevelDbTombstoneSweeper&) = delete;

  ~LevelDbTombstoneSweeper() override;

  bool RequiresMetadata() const override;

  void SetMetadata(
      const std::vector<blink::IndexedDBDatabaseMetadata>* metadata) override;

  bool RunRound() override;

 private:
  using IndexedDBDatabaseMetadataVector =
      std::vector<blink::IndexedDBDatabaseMetadata>;
  using ObjectStoreMetadataMap =
      std::map<int64_t, blink::IndexedDBObjectStoreMetadata>;
  using IndexMetadataMap = std::map<int64_t, blink::IndexedDBIndexMetadata>;

  friend class LevelDbTombstoneSweeperTest;

  enum class SweepStatus { SWEEPING, DONE_ERROR, DONE };

  // Contains the current sweeping state and position for the sweeper.
  struct SweepState {
    SweepState();
    ~SweepState();

    // Stores the random starting database seed. Not bounded.
    size_t start_database_seed = 0;
    std::optional<WrappingIterator<IndexedDBDatabaseMetadataVector>>
        database_it;

    // Stores the random starting object store seed. Not bounded.
    size_t start_object_store_seed = 0;
    std::optional<WrappingIterator<ObjectStoreMetadataMap>> object_store_it;

    // Stores the random starting object store seed. Not bounded.
    size_t start_index_seed = 0;
    std::optional<WrappingIterator<IndexMetadataMap>> index_it;
    std::optional<IndexDataKey> index_it_key;
  };

  void SetStartSeedsForTesting(size_t database_seed,
                               size_t object_store_seed,
                               size_t index_seed) {
    sweep_state_.start_database_seed = database_seed;
    sweep_state_.start_object_store_seed = object_store_seed;
    sweep_state_.start_index_seed = index_seed;
  }

  Status FlushDeletions();

  bool ShouldContinueIteration(SweepStatus* sweep_status,
                               Status* leveldb_status,
                               int* round_iters);

  SweepStatus DoSweep(Status* status);

  // Returns true if sweeper can continue iterating.
  bool IterateIndex(int64_t database_id,
                    int64_t object_store_id,
                    const blink::IndexedDBIndexMetadata& index,
                    SweepStatus* sweep_status,
                    Status* leveldb_status,
                    int* round_iterations);

  int num_iterations_ = 0;
  const int max_round_iterations_;
  const int max_iterations_;

  int indices_scanned_ = 0;
  int total_indices_ = 0;

  bool has_writes_ = false;
  leveldb::WriteBatch round_deletion_batch_;

  raw_ptr<const std::vector<blink::IndexedDBDatabaseMetadata>>
      database_metadata_ = nullptr;
  std::unique_ptr<leveldb::Iterator> iterator_;

  SweepState sweep_state_;

  base::WeakPtrFactory<LevelDbTombstoneSweeper> ptr_factory_{this};
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_LEVELDB_TOMBSTONE_SWEEPER_H_
