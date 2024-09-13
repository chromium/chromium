// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/active_blob_registry.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/task_runner.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/instance/backing_store.h"

namespace content::indexed_db {

ActiveBlobRegistry::ActiveBlobRegistry(
    ReportOutstandingBlobsCallback report_outstanding_blobs,
    ReportUnusedBlobCallback report_unused_blob)
    : report_outstanding_blobs_(std::move(report_outstanding_blobs)),
      report_unused_blob_(std::move(report_unused_blob)) {}

ActiveBlobRegistry::~ActiveBlobRegistry() {}

bool ActiveBlobRegistry::MarkDatabaseDeletedAndCheckIfReferenced(
    int64_t database_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  auto db_pair = blob_reference_tracker_.find(database_id);
  if (db_pair == blob_reference_tracker_.end()) {
    return false;
  }

  deleted_dbs_.insert(database_id);
  return true;
}

bool ActiveBlobRegistry::MarkBlobInfoDeletedAndCheckIfReferenced(
    int64_t database_id,
    int64_t blob_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(blob_number, DatabaseMetaDataKey::kAllBlobsNumber);
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  auto db_pair = blob_reference_tracker_.find(database_id);
  if (db_pair == blob_reference_tracker_.end()) {
    return false;
  }

  SingleDBMap& single_db = db_pair->second;
  auto iter = single_db.find(blob_number);
  if (iter == single_db.end()) {
    return false;
  }

  iter->second = BlobState::kUnlinked;
  return true;
}

base::RepeatingClosure ActiveBlobRegistry::GetFinalReleaseCallback(
    int64_t database_id,
    int64_t blob_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindRepeating(&ActiveBlobRegistry::MarkBlobInactive,
                             weak_factory_.GetWeakPtr(), database_id,
                             blob_number);
}

base::RepeatingClosure ActiveBlobRegistry::GetMarkBlobActiveCallback(
    int64_t database_id,
    int64_t blob_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindRepeating(&ActiveBlobRegistry::MarkBlobActive,
                             weak_factory_.GetWeakPtr(), database_id,
                             blob_number);
}

void ActiveBlobRegistry::ForceShutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  blob_reference_tracker_.clear();
  report_outstanding_blobs_.Reset();
  report_unused_blob_.Reset();
}

void ActiveBlobRegistry::MarkBlobActive(int64_t database_id,
                                        int64_t blob_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(report_outstanding_blobs_);
  DCHECK(report_unused_blob_);

  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  DCHECK(DatabaseMetaDataKey::IsValidBlobNumber(blob_number));
  DCHECK(!base::Contains(deleted_dbs_, database_id));
  bool outstanding_blobs_in_backing_store = !blob_reference_tracker_.empty();
  SingleDBMap& blobs_in_db = blob_reference_tracker_[database_id];
  auto iter = blobs_in_db.find(blob_number);
  if (iter == blobs_in_db.end()) {
    blobs_in_db[blob_number] = BlobState::kLinked;
    if (!outstanding_blobs_in_backing_store) {
      report_outstanding_blobs_.Run(true);
    }
  } else {
    DCHECK(outstanding_blobs_in_backing_store);
    // You can't add a reference once it's been deleted.
    DCHECK(iter->second == BlobState::kLinked);
  }
}

void ActiveBlobRegistry::MarkBlobInactive(int64_t database_id,
                                          int64_t blob_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(report_outstanding_blobs_);
  DCHECK(report_unused_blob_);

  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  DCHECK(DatabaseMetaDataKey::IsValidBlobNumber(blob_number));
  auto db_pair = blob_reference_tracker_.find(database_id);
  if (db_pair == blob_reference_tracker_.end()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  SingleDBMap& blobs_in_db = db_pair->second;
  auto blob_in_db_it = blobs_in_db.find(blob_number);
  if (blob_in_db_it == blobs_in_db.end()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  bool delete_blob_in_backend = false;
  auto deleted_database_it = deleted_dbs_.find(database_id);
  bool db_marked_for_deletion = deleted_database_it != deleted_dbs_.end();
  // Don't bother deleting the file if we're going to delete its whole
  // database directory soon.
  delete_blob_in_backend =
      blob_in_db_it->second == BlobState::kUnlinked && !db_marked_for_deletion;
  blobs_in_db.erase(blob_in_db_it);
  if (blobs_in_db.empty()) {
    blob_reference_tracker_.erase(db_pair);
    if (db_marked_for_deletion) {
      delete_blob_in_backend = true;
      blob_number = DatabaseMetaDataKey::kAllBlobsNumber;
      deleted_dbs_.erase(deleted_database_it);
    }
  }
  if (delete_blob_in_backend) {
    report_unused_blob_.Run(database_id, blob_number);
  }
  if (blob_reference_tracker_.empty()) {
    report_outstanding_blobs_.Run(false);
  }
}

}  // namespace content::indexed_db
