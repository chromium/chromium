// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXTERNAL_OBJECT_STORAGE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXTERNAL_OBJECT_STORAGE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/status.h"

namespace content::indexed_db {

// This file contains all of the classes & types used to store external objects
// (such as blobs) in IndexedDB. Currently it is messy because this is
// mid-refactor, but it will be cleaned up over time.

enum class BlobWriteResult {
  // The blobs were written, and phase two should be scheduled asynchronously.
  // The returned status will be ignored.
  kRunPhaseTwoAsync,
  // The blobs were written, and phase two should be run now. The returned
  // status will be correctly propagated.
  kRunPhaseTwoAndReturnResult,
};

// This callback is used to signify that writing blobs is complete. The
// `StatusOr<BlobWriteResult>` signifies if the operation succeeded or not, and
// the returned status is used to handle errors in the next part of the
// transaction commit lifecycle. Note: The returned status can only be used when
// the result is `kRunPhaseTwoAndReturnResult`.
using BlobWriteCallback = base::OnceCallback<Status(StatusOr<BlobWriteResult>)>;

// This callback will serialize a single object that represents an FSA handle
// and return the serialized token asynchronously via the inner callback.
using SerializeFsaCallback = base::RepeatingCallback<void(
    blink::mojom::FileSystemAccessTransferToken&,
    base::OnceCallback<void(
        const std::vector<uint8_t>& /*serialized_token*/)>)>;

// This callback will rehydrate a serialized FSA handle token into a mojo
// endpoint which is returned asynchronously via the inner callback.
using DeserializeFsaCallback = base::RepeatingCallback<void(
    const std::vector<uint8_t>& /*serialized_token*/,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken>)>;

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
