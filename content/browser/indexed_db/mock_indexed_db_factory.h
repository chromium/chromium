// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_FACTORY_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

// TODO(dmurph) Remove this by making classes more testable.
class MockIndexedDBFactory : public IndexedDBFactory {
 public:
  MockIndexedDBFactory();
  ~MockIndexedDBFactory() override;
  MOCK_METHOD3(GetDatabaseNames,
               void(scoped_refptr<IndexedDBCallbacks> callbacks,
                    const url::Origin& origin,
                    const base::FilePath& data_directory));
  MOCK_METHOD3(GetDatabaseInfo,
               void(scoped_refptr<IndexedDBCallbacks> callbacks,
                    const url::Origin& origin,
                    const base::FilePath& data_directory));
  MOCK_METHOD4(
      OpenProxy,
      void(const base::string16& name,
           IndexedDBPendingConnection* connection,
           const url::Origin& origin,
           const base::FilePath& data_directory));
  // Googlemock can't deal with move-only types, so *Proxy() is a workaround.
  void Open(const base::string16& name,
            std::unique_ptr<IndexedDBPendingConnection> connection,
            const url::Origin& origin,
            const base::FilePath& data_directory) override {
    OpenProxy(name, connection.get(), origin, data_directory);
  }
  MOCK_METHOD5(DeleteDatabase,
               void(const base::string16& name,
                    scoped_refptr<IndexedDBCallbacks> callbacks,
                    const url::Origin& origin,
                    const base::FilePath& data_directory,
                    bool force_close));
  MOCK_METHOD2(AbortTransactionsAndCompactDatabaseProxy,
               void(base::OnceCallback<void(leveldb::Status)>* callback,
                    const url::Origin& origin));
  void AbortTransactionsAndCompactDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const url::Origin& origin) override {
    base::OnceCallback<void(leveldb::Status)>* callback_ref = &callback;
    AbortTransactionsAndCompactDatabaseProxy(callback_ref, origin);
  }
  MOCK_METHOD2(AbortTransactionsForDatabaseProxy,
               void(base::OnceCallback<void(leveldb::Status)>* callback,
                    const url::Origin& origin));
  void AbortTransactionsForDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const url::Origin& origin) override {
    base::OnceCallback<void(leveldb::Status)>* callback_ref = &callback;
    AbortTransactionsForDatabaseProxy(callback_ref, origin);
  }

  MOCK_METHOD1(HandleBackingStoreFailure, void(const url::Origin& origin));
  MOCK_METHOD2(HandleBackingStoreCorruption,
               void(const url::Origin& origin,
                    const IndexedDBDatabaseError& error));
  // The Android NDK implements a subset of STL, and the gtest templates can't
  // deal with std::pair's. This means we can't use GoogleMock for this method
  std::vector<IndexedDBDatabase*> GetOpenDatabasesForOrigin(
      const url::Origin& origin) const override;
  MOCK_METHOD2(ForceClose,
               void(const url::Origin& origin, bool delete_in_memory_store));
  MOCK_METHOD1(ForceSchemaDowngrade, void(const url::Origin& origin));
  MOCK_METHOD1(HasV2SchemaCorruption,
               V2SchemaCorruptionStatus(const url::Origin& origin));
  MOCK_METHOD0(ContextDestroyed, void());

  MOCK_METHOD1(BlobFilesCleaned, void(const url::Origin& origin));

  MOCK_CONST_METHOD1(GetConnectionCount, size_t(const url::Origin& origin));

  MOCK_CONST_METHOD1(GetInMemoryDBSize, int64_t(const url::Origin& origin));

  MOCK_CONST_METHOD1(GetLastModified, base::Time(const url::Origin& origin));

  MOCK_METHOD2(ReportOutstandingBlobs,
               void(const url::Origin& origin, bool blobs_outstanding));

  MOCK_METHOD1(NotifyIndexedDBListChanged, void(const url::Origin& origin));
  MOCK_METHOD3(NotifyIndexedDBContentChanged,
               void(const url::Origin& origin,
                    const base::string16& database_name,
                    const base::string16& object_store_name));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIndexedDBFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_FACTORY_H_
