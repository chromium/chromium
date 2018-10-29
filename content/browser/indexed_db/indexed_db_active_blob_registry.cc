// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "content/browser/indexed_db/indexed_db_active_blob_registry.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"

namespace content {

IndexedDBActiveBlobRegistry::IndexedDBActiveBlobRegistry(
    IndexedDBBackingStore* backing_store)
    : backing_store_(backing_store), weak_factory_(this) {}

IndexedDBActiveBlobRegistry::~IndexedDBActiveBlobRegistry() {
}

void IndexedDBActiveBlobRegistry::AddBlobRef(int64_t database_id,
                                             int64_t blob_key) {
  DCHECK(backing_store_);
  DCHECK(backing_store_->task_runner()->RunsTasksInCurrentSequence());
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  DCHECK(DatabaseMetaDataKey::IsValidBlobKey(blob_key));
  DCHECK(!base::ContainsKey(deleted_dbs_, database_id));
  bool need_ref = use_tracker_.empty();
  SingleDBMap& single_db_map = use_tracker_[database_id];
  auto iter = single_db_map.find(blob_key);
  if (iter == single_db_map.end()) {
    single_db_map[blob_key] = false;
    if (need_ref) {
      backing_store_->factory()->ReportOutstandingBlobs(
          backing_store_->origin(), true);
    }
  } else {
    DCHECK(!need_ref);
    DCHECK(!iter->second);  // You can't add a reference once it's been deleted.
  }
}

void IndexedDBActiveBlobRegistry::ReleaseBlobRef(int64_t database_id,
                                                 int64_t blob_key) {
  DCHECK(backing_store_);
  DCHECK(backing_store_->task_runner()->RunsTasksInCurrentSequence());
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  DCHECK(DatabaseMetaDataKey::IsValidBlobKey(blob_key));
  const auto& db_pair = use_tracker_.find(database_id);
  if (db_pair == use_tracker_.end()) {
    NOTREACHED();
    return;
  }
  SingleDBMap& single_db = db_pair->second;
  auto blob_pair = single_db.find(blob_key);
  if (blob_pair == single_db.end()) {
    NOTREACHED();
    return;
  }
  bool delete_in_backend = false;
  const auto& db_to_delete = deleted_dbs_.find(database_id);
  bool db_marked_for_deletion = db_to_delete != deleted_dbs_.end();
  // Don't bother deleting the file if we're going to delete its whole
  // database directory soon.
  delete_in_backend = blob_pair->second && !db_marked_for_deletion;
  single_db.erase(blob_pair);
  if (single_db.empty()) {
    use_tracker_.erase(db_pair);
    if (db_marked_for_deletion) {
      delete_in_backend = true;
      blob_key = DatabaseMetaDataKey::kAllBlobsKey;
      deleted_dbs_.erase(db_to_delete);
    }
  }
  if (delete_in_backend)
    backing_store_->ReportBlobUnused(database_id, blob_key);
  if (use_tracker_.empty()) {
    backing_store_->factory()->ReportOutstandingBlobs(backing_store_->origin(),
                                                      false);
  }
}

bool IndexedDBActiveBlobRegistry::MarkDeletedCheckIfUsed(int64_t database_id,
                                                         int64_t blob_key) {
  DCHECK(backing_store_);
  DCHECK(backing_store_->task_runner()->RunsTasksInCurrentSequence());
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  const auto& db_pair = use_tracker_.find(database_id);
  if (db_pair == use_tracker_.end())
    return false;

  if (blob_key == DatabaseMetaDataKey::kAllBlobsKey) {
    deleted_dbs_.insert(database_id);
    return true;
  }

  SingleDBMap& single_db = db_pair->second;
  auto iter = single_db.find(blob_key);
  if (iter == single_db.end())
    return false;

  iter->second = true;
  return true;
}

void IndexedDBActiveBlobRegistry::ReleaseBlobRefThreadSafe(
    scoped_refptr<base::TaskRunner> task_runner,
    base::WeakPtr<IndexedDBActiveBlobRegistry> weak_ptr,
    int64_t database_id,
    int64_t blob_key,
    const base::FilePath& unused) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&IndexedDBActiveBlobRegistry::ReleaseBlobRef,
                                weak_ptr, database_id, blob_key));
}

IndexedDBBlobInfo::ReleaseCallback
IndexedDBActiveBlobRegistry::GetFinalReleaseCallback(int64_t database_id,
                                                     int64_t blob_key) {
  return base::Bind(
      &IndexedDBActiveBlobRegistry::ReleaseBlobRefThreadSafe,
      scoped_refptr<base::TaskRunner>(backing_store_->task_runner()),
      weak_factory_.GetWeakPtr(), database_id, blob_key);
}

base::Closure IndexedDBActiveBlobRegistry::GetAddBlobRefCallback(
    int64_t database_id,
    int64_t blob_key) {
  return base::Bind(&IndexedDBActiveBlobRegistry::AddBlobRef,
                    weak_factory_.GetWeakPtr(), database_id, blob_key);
}

void IndexedDBActiveBlobRegistry::ForceShutdown() {
  weak_factory_.InvalidateWeakPtrs();
  use_tracker_.clear();
  backing_store_ = nullptr;
}

}  // namespace content
