// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_ACTIVE_BLOB_REGISTRY_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_ACTIVE_BLOB_REGISTRY_H_

#include <stdint.h>

#include <map>
#include <set>
#include <utility>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/indexed_db/indexed_db_blob_info.h"
#include "content/common/content_export.h"

namespace content {

// Keeps track of blobs that have been sent to the renderer as database
// responses, and determines when the file should be deleted. The database entry
// that the blob files live in could be deleted in the database, but the file
// would need to stay alive while it is still used in the renderer.
// This class must be used on a single sequence, and will call the given
// callbacks back on the same sequence it is constructed on.
class CONTENT_EXPORT IndexedDBActiveBlobRegistry {
 public:
  using ReportOutstandingBlobsCallback = base::RepeatingCallback<void(bool)>;
  using ReportUnusedBlobCallback =
      base::RepeatingCallback<void(int64_t /*database_id*/,
                                   int64_t /*blob_key*/)>;

  explicit IndexedDBActiveBlobRegistry(
      ReportOutstandingBlobsCallback report_outstanding_blobs,
      ReportUnusedBlobCallback report_unused_blob);
  ~IndexedDBActiveBlobRegistry();

  // Most methods of this class, and the closure returned by
  // GetAddBlobRefCallback, should only be called on the backing_store's task
  // runner.  The exception is the closure returned by GetFinalReleaseCallback,
  // which just calls ReleaseBlobRefThreadSafe.

  // Marks a given blob or a while database as deleted by a transaction, and
  // returns if the given blob key (or database) was previously used.
  // Use DatabaseMetaDataKey::AllBlobsKey for "the whole database".
  bool MarkDeletedCheckIfUsed(int64_t database_id, int64_t blob_key);

  IndexedDBBlobInfo::ReleaseCallback GetFinalReleaseCallback(
      int64_t database_id,
      int64_t blob_key);
  // This closure holds a raw pointer to the IndexedDBActiveBlobRegistry,
  // and may not be called after it is deleted.
  base::RepeatingClosure GetAddBlobRefCallback(int64_t database_id,
                                               int64_t blob_key);
  // Call this to force the registry to drop its use counts, permitting the
  // factory to drop any blob-related refcount for the backing store.
  // This will also turn any outstanding callbacks into no-ops.
  void ForceShutdown();

 private:
  enum class BlobState { kAlive, kDeleted };
  // Maps blob_key -> BlobState; if the record's absent, it's not in active use
  // and we don't care if it's deleted.
  typedef std::map<int64_t, BlobState> SingleDBMap;

  void AddBlobRef(int64_t database_id, int64_t blob_key);

  // Removes a reference to the given blob.
  void ReleaseBlobRef(int64_t database_id, int64_t blob_key);
  static void ReleaseBlobRefThreadSafe(
      scoped_refptr<base::TaskRunner> task_runner,
      base::WeakPtr<IndexedDBActiveBlobRegistry> weak_ptr,
      int64_t database_id,
      int64_t blob_key,
      const base::FilePath& unused);

  SEQUENCE_CHECKER(sequence_checker_);

  std::map<int64_t, SingleDBMap> use_tracker_;
  // Databases that have been marked as deleted by MarkDeletedCheckIfUsed.
  std::set<int64_t> deleted_dbs_;
  ReportOutstandingBlobsCallback report_outstanding_blobs_;
  ReportUnusedBlobCallback report_unused_blob_;
  base::WeakPtrFactory<IndexedDBActiveBlobRegistry> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IndexedDBActiveBlobRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_ACTIVE_BLOB_REGISTRY_H_
