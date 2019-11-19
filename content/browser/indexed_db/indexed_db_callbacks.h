// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}

namespace blink {
struct IndexedDBDatabaseMetadata;
}

namespace content {
class IndexedDBBlobInfo;
class IndexedDBConnection;
class IndexedDBCursor;
class IndexedDBDatabase;
struct IndexedDBDataLossInfo;
struct IndexedDBValue;

class CONTENT_EXPORT IndexedDBCallbacks
    : public base::RefCounted<IndexedDBCallbacks> {
 public:
  // IndexedDBValueBlob stores information about a given IndexedDBValue's
  // blobs so they can be created on the IO thread.
  class IndexedDBValueBlob {
   public:
    // IndexedDBValueBlob() takes a std::vector<IDBBlobInfoPtr>* which it
    // accesses during its invocation but doesn't keep a copy of it.  The
    // std::vector<IDBBlobInfoPtr>* must only be alive for the duration of the
    // invocation.
    IndexedDBValueBlob(const IndexedDBBlobInfo& blob_info,
                       blink::mojom::IDBBlobInfoPtr* blob_or_file_info);
    IndexedDBValueBlob(IndexedDBValueBlob&& other);
    ~IndexedDBValueBlob();

    // GetIndexedDBValueBlobs() takes a std::vector<IDBBlobInfoPtr>* which it
    // passes to IndexedDBValueBlob().  Neither of them hold the pointer after
    // the call.
    static void GetIndexedDBValueBlobs(
        std::vector<IndexedDBValueBlob>* value_blobs,
        const std::vector<IndexedDBBlobInfo>& blob_info,
        std::vector<blink::mojom::IDBBlobInfoPtr>* blob_or_file_info);
    // GetIndexedDBValueBlobs() takes a std::vector<IDBBlobInfoPtr>* which it
    // passes to IndexedDBValueBlob().  Neither of them hold the pointer after
    // the call.
    static std::vector<IndexedDBValueBlob> GetIndexedDBValueBlobs(
        const std::vector<IndexedDBBlobInfo>& blob_info,
        std::vector<blink::mojom::IDBBlobInfoPtr>* blob_or_file_info);

   private:
    friend class IndexedDBCallbacks;
    friend class IndexedDBCursor;

    const IndexedDBBlobInfo& blob_info_;
    std::string uuid_;
    mojo::PendingReceiver<blink::mojom::Blob> receiver_;
  };

  static bool CreateAllBlobs(
      scoped_refptr<ChromeBlobStorageContext> blob_context,
      std::vector<IndexedDBValueBlob> value_blobs);

  IndexedDBCallbacks(base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
                     const url::Origin& origin,
                     mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
                         pending_callbacks,
                     scoped_refptr<base::SequencedTaskRunner> idb_runner);

  virtual void OnError(const IndexedDBDatabaseError& error);

  // IndexedDBFactory::databases
  virtual void OnSuccess(
      std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions);

  // IndexedDBFactory::GetDatabaseNames
  virtual void OnSuccess(const std::vector<base::string16>& string);

  // IndexedDBFactory::Open / DeleteDatabase
  virtual void OnBlocked(int64_t existing_version);

  // IndexedDBFactory::Open
  virtual void OnUpgradeNeeded(int64_t old_version,
                               std::unique_ptr<IndexedDBConnection> connection,
                               const blink::IndexedDBDatabaseMetadata& metadata,
                               const IndexedDBDataLossInfo& data_loss_info);
  virtual void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                         const blink::IndexedDBDatabaseMetadata& metadata);

  // IndexedDBDatabase::Count
  // IndexedDBFactory::DeleteDatabase
  // IndexedDBDatabase::DeleteRange
  // IndexedDBDatabase::GetKeyGeneratorCurrentNumber
  virtual void OnSuccess(int64_t value);

  // IndexedDBCursor::Continue / Advance (when complete)
  virtual void OnSuccess();

  void OnConnectionError();

  bool is_complete() const { return complete_; }

 protected:
  virtual ~IndexedDBCallbacks();

 private:
  friend class base::RefCounted<IndexedDBCallbacks>;

  // Stores if this callbacks object is complete and should not be called again.
  bool complete_ = false;

  // Depending on whether the database needs upgrading, we create connections in
  // different spots. This stores if we've already created the connection so
  // OnSuccess(Connection) doesn't create an extra one.
  bool connection_created_ = false;

  // Used to assert that OnSuccess is only called if there was no data loss.
  blink::mojom::IDBDataLoss data_loss_;

  // The "blocked" event should be sent at most once per request.
  bool sent_blocked_ = false;

  base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host_;
  url::Origin origin_;
  scoped_refptr<base::SequencedTaskRunner> idb_runner_;
  mojo::AssociatedRemote<blink::mojom::IDBCallbacks> callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(IndexedDBCallbacks);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_H_
