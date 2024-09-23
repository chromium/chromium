// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_ACTIVE_BLOB_REGISTRY_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_ACTIVE_BLOB_REGISTRY_H_

#include <stdint.h>

#include <map>
#include <set>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"

namespace content::indexed_db {

// Keeps track of blobs that have been sent to clients as database responses,
// and determines when Blob files can be deleted. The database entry that links
// to the blob file can be deleted in the database, but the blob file still
// needs to stay alive while it is still active (i.e. referenced by a client).
// This class must be used on a single sequence, and will call the given
// callbacks back on the same sequence it is constructed on.
class CONTENT_EXPORT ActiveBlobRegistry {
 public:
  using ReportOutstandingBlobsCallback = base::RepeatingCallback<void(bool)>;
  using ReportUnusedBlobCallback =
      base::RepeatingCallback<void(int64_t /*database_id*/,
                                   int64_t /*blob_number*/)>;

  explicit ActiveBlobRegistry(
      ReportOutstandingBlobsCallback report_outstanding_blobs,
      ReportUnusedBlobCallback report_unused_blob);

  ActiveBlobRegistry(const ActiveBlobRegistry&) = delete;
  ActiveBlobRegistry& operator=(const ActiveBlobRegistry&) = delete;

  ~ActiveBlobRegistry();

  // Most methods of this class, and the closure returned by
  // GetMarkBlobActiveCallback, should only be called on the backing_store's
  // task runner.  The exception is the closure returned by
  // GetFinalReleaseCallback, which just calls MarkBlobInactiveThreadSafe.

  // Called when the given database is deleted (and all blob infos inside of
  // it). Returns true if any of the deleted blobs are active (i.e. referenced
  // by a client).
  bool MarkDatabaseDeletedAndCheckIfReferenced(int64_t database_id);

  // Called when the given blob handle is deleted by a transaction, and returns
  // if the blob file has one or more external references.
  bool MarkBlobInfoDeletedAndCheckIfReferenced(int64_t database_id,
                                               int64_t blob_number);

  // When called, the returned callback will mark the given blob entry as
  // inactive (i.e. no longer referenced by a client).
  // This closure must be called on the same sequence as this registry.
  base::RepeatingClosure GetFinalReleaseCallback(int64_t database_id,
                                                 int64_t blob_number);

  // When called, the returned closure will mark the given blob entry as active
  // (i.e. referenced by the client). Calling this multiple times does nothing.
  // This closure holds a raw pointer to the ActiveBlobRegistry, and may not be
  // called after it is deleted.
  base::RepeatingClosure GetMarkBlobActiveCallback(int64_t database_id,
                                                   int64_t blob_number);
  // Call this to force the registry to drop its use counts, permitting the
  // factory to drop any blob-related refcount for the backing store.
  // This will also turn any outstanding callbacks into no-ops.
  void ForceShutdown();

 private:
  enum class BlobState { kLinked, kUnlinked };
  // Maps blob_number -> BlobState; if the record's absent, it's not in active
  // use and we don't care if it's deleted.
  typedef std::map<int64_t, BlobState> SingleDBMap;

  void MarkBlobActive(int64_t database_id, int64_t blob_number);

  // Removes a reference to the given blob.
  void MarkBlobInactive(int64_t database_id, int64_t blob_number);

  SEQUENCE_CHECKER(sequence_checker_);

  // This stores, for every database, the blobs that are currently being used in
  // that database.
  std::map<int64_t, SingleDBMap> blob_reference_tracker_;
  // Databases that have been marked as deleted by
  // MarkDatabaseDeletedAndCheckIfReferenced.
  std::set<int64_t> deleted_dbs_;
  ReportOutstandingBlobsCallback report_outstanding_blobs_;
  ReportUnusedBlobCallback report_unused_blob_;
  base::WeakPtrFactory<ActiveBlobRegistry> weak_factory_{this};
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_ACTIVE_BLOB_REGISTRY_H_
