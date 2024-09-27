// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXTERNAL_OBJECT_STORAGE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXTERNAL_OBJECT_STORAGE_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/status.h"
#include "storage/common/file_system/file_system_mount_option.h"

namespace content::indexed_db {

// This file contains all of the classes & types used to store external objects
// (such as blobs) in IndexedDB. Currently it is messy because this is
// mid-refactor, but it will be cleaned up over time.

enum class BlobWriteResult {
  // There was an error writing the blobs.
  kFailure,
  // The blobs were written, and phase two should be scheduled asynchronously.
  // The returned status will be ignored.
  kRunPhaseTwoAsync,
  // The blobs were written, and phase two should be run now. The returned
  // status will be correctly propagated.
  kRunPhaseTwoAndReturnResult,
};

// This callback is used to signify that writing blobs is complete. The
// BlobWriteResult signifies if the operation succeeded or not, and the returned
// status is used to handle errors in the next part of the transcation commit
// lifecycle. Note: The returned status can only be used when the result is
// |kRunPhaseTwoAndReturnResult|.  The WriteBlobToFileResult is a more granular
// error in the case something goes wrong.
using BlobWriteCallback =
    base::OnceCallback<Status(BlobWriteResult,
                              storage::mojom::WriteBlobToFileResult)>;

// This object represents a change in the database involving adding or removing
// external objects. if external_objects() is empty, then objects are to be
// deleted, and if external_objects() is populated, then objects are two be
// written (and also possibly deleted if there were already objects).
class IndexedDBExternalObjectChangeRecord {
 public:
  IndexedDBExternalObjectChangeRecord(const std::string& object_store_data_key);

  IndexedDBExternalObjectChangeRecord(
      const IndexedDBExternalObjectChangeRecord&) = delete;
  IndexedDBExternalObjectChangeRecord& operator=(
      const IndexedDBExternalObjectChangeRecord&) = delete;

  ~IndexedDBExternalObjectChangeRecord();

  const std::string& object_store_data_key() const {
    return object_store_data_key_;
  }
  void SetExternalObjects(
      std::vector<IndexedDBExternalObject>* external_objects);
  std::vector<IndexedDBExternalObject>& mutable_external_objects() {
    return external_objects_;
  }
  const std::vector<IndexedDBExternalObject>& external_objects() const {
    return external_objects_;
  }
  std::unique_ptr<IndexedDBExternalObjectChangeRecord> Clone() const;

 private:
  std::string object_store_data_key_;
  std::vector<IndexedDBExternalObject> external_objects_;
};

// Reports that the recovery and/or active journals have been processed, and
// blob files have been deleted.
using BlobFilesCleanedCallback = base::RepeatingClosure;

// Reports that there are (or are not) active blobs.
using ReportOutstandingBlobsCallback =
    base::RepeatingCallback<void(/*outstanding_blobs=*/bool)>;

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXTERNAL_OBJECT_STORAGE_H_
