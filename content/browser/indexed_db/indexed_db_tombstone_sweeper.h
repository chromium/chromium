// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TOMBSTONE_SWEEPER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TOMBSTONE_SWEEPER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace base {
class TickClock;
}

namespace blink {
struct IndexedDBDatabaseMetadata;
struct IndexedDBIndexMetadata;
struct IndexedDBObjectStoreMetadata;
}  // namespace blink

namespace leveldb {
class DB;
class Iterator;
}  // namespace leveldb

namespace content {
class IndexedDBBackingStore;

namespace indexed_db_tombstone_sweeper_unittest {
class IndexedDBTombstoneSweeperTest;
}

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
  const T* container_ = nullptr;
};

// Sweeps the IndexedDB leveldb database looking for index tombstones. These
// occur when the indexed fields of rows are modified, and stay around if script
// doesn't do a cursor iteration of the database.
//
// Owned by the IndexedDBBackingStore.
//
// TODO(dmurph) Describe this class in a README.md file.
// See bit.ly/idb-tombstone-sweeper for more information.
class CONTENT_EXPORT IndexedDBTombstoneSweeper
    : public IndexedDBPreCloseTaskQueue::PreCloseTask {
 public:
  // The |database| must outlive this instance.
  IndexedDBTombstoneSweeper(int round_iterations,
                            int max_iterations,
                            leveldb::DB* database);
  ~IndexedDBTombstoneSweeper() override;

  void SetMetadata(
      std::vector<blink::IndexedDBDatabaseMetadata> const* metadata) override;

  void Stop(IndexedDBPreCloseTaskQueue::StopReason reason) override;

  bool RunRound() override;

 private:
  using DatabaseMetadataVector = std::vector<blink::IndexedDBDatabaseMetadata>;
  using ObjectStoreMetadataMap =
      std::map<int64_t, blink::IndexedDBObjectStoreMetadata>;
  using IndexMetadataMap = std::map<int64_t, blink::IndexedDBIndexMetadata>;

  friend class indexed_db_tombstone_sweeper_unittest::
      IndexedDBTombstoneSweeperTest;

  enum class Status { SWEEPING, DONE_REACHED_MAX, DONE_ERROR, DONE_COMPLETE };

  // Contains the current sweeping state and position for the sweeper.
  struct SweepState {
    SweepState();
    ~SweepState();

    // Stores the random starting database seed. Not bounded.
    size_t start_database_seed = 0;
    base::Optional<WrappingIterator<DatabaseMetadataVector>> database_it;

    // Stores the random starting object store seed. Not bounded.
    size_t start_object_store_seed = 0;
    base::Optional<WrappingIterator<ObjectStoreMetadataMap>> object_store_it;

    // Stores the random starting object store seed. Not bounded.
    size_t start_index_seed = 0;
    base::Optional<WrappingIterator<IndexMetadataMap>> index_it;
    base::Optional<IndexDataKey> index_it_key;
  };

  // Accumulated metrics that are reported at the end of sweeping.
  struct SweepMetrics {
    int num_invalid_index_values = 0;
    int num_errors_reading_exists_table = 0;
    int num_invalid_exists_values = 0;

    int seen_tombstones = 0;
    uint64_t seen_tombstones_size = 0;
  };

  void SetStartSeedsForTesting(size_t database_seed,
                               size_t object_store_seed,
                               size_t index_seed) {
    sweep_state_.start_database_seed = database_seed;
    sweep_state_.start_object_store_seed = object_store_seed;
    sweep_state_.start_index_seed = index_seed;
  }

  void SetClockForTesting(const base::TickClock* clock) {
    clock_for_testing_ = clock;
  }

  // Records UMA stats based on stop or completion status, as well as the mode
  // of the sweeper.
  // Exactly one optional argument must be populated.
  void RecordUMAStats(
      base::Optional<IndexedDBPreCloseTaskQueue::StopReason> stop_reason,
      base::Optional<Status> status,
      const leveldb::Status& leveldb_error);

  leveldb::Status FlushDeletions();

  bool ShouldContinueIteration(Status* sweep_status,
                               leveldb::Status* leveldb_status,
                               int* round_iters);

  Status DoSweep(leveldb::Status* status);

  // Returns true if sweeper can continue iterating.
  bool IterateIndex(int64_t database_id,
                    int64_t object_store_id,
                    const blink::IndexedDBIndexMetadata& index,
                    Status* sweep_status,
                    leveldb::Status* leveldb_status,
                    int* round_iterations);

  int num_iterations_ = 0;
  const int max_round_iterations_;
  const int max_iterations_;

  int indices_scanned_ = 0;
  int total_indices_ = 0;

  // Used to measure total time of the task.
  const base::TickClock* clock_for_testing_ = nullptr;
  base::Optional<base::TimeTicks> start_time_;

  leveldb::DB* database_ = nullptr;
  bool has_writes_ = false;
  leveldb::WriteBatch round_deletion_batch_;
  base::TimeDelta total_deletion_time_;

  std::vector<blink::IndexedDBDatabaseMetadata> const* database_metadata_ =
      nullptr;
  std::unique_ptr<leveldb::Iterator> iterator_;

  SweepState sweep_state_;
  SweepMetrics metrics_;

  base::WeakPtrFactory<IndexedDBTombstoneSweeper> ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(IndexedDBTombstoneSweeper);
};

}  // namespace content
#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TOMBSTONE_SWEEPER_H_
