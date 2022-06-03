// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_FACTORY_H_

#include <stddef.h>

#include <memory>

#include "content/browser/indexed_db/indexed_db_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

// TODO(dmurph) Remove this by making classes more testable.
class MockIndexedDBFactory : public IndexedDBFactory {
 public:
  MockIndexedDBFactory();

  MockIndexedDBFactory(const MockIndexedDBFactory&) = delete;
  MockIndexedDBFactory& operator=(const MockIndexedDBFactory&) = delete;

  ~MockIndexedDBFactory() override;
  MOCK_METHOD3(GetDatabaseNames,
               void(scoped_refptr<IndexedDBCallbacks> callbacks,
                    const blink::StorageKey& storage_key,
                    const base::FilePath& data_directory));
  MOCK_METHOD3(GetDatabaseInfo,
               void(scoped_refptr<IndexedDBCallbacks> callbacks,
                    const blink::StorageKey& storage_key,
                    const base::FilePath& data_directory));
  MOCK_METHOD4(OpenProxy,
               void(const std::u16string& name,
                    IndexedDBPendingConnection* connection,
                    const blink::StorageKey& storage_key,
                    const base::FilePath& data_directory));
  // Googlemock can't deal with move-only types, so *Proxy() is a workaround.
  void Open(const std::u16string& name,
            std::unique_ptr<IndexedDBPendingConnection> connection,
            const blink::StorageKey& storage_key,
            const base::FilePath& data_directory) override {
    OpenProxy(name, connection.get(), storage_key, data_directory);
  }
  MOCK_METHOD5(DeleteDatabase,
               void(const std::u16string& name,
                    scoped_refptr<IndexedDBCallbacks> callbacks,
                    const blink::StorageKey& storage_key,
                    const base::FilePath& data_directory,
                    bool force_close));
  MOCK_METHOD2(AbortTransactionsAndCompactDatabaseProxy,
               void(base::OnceCallback<void(leveldb::Status)>* callback,
                    const blink::StorageKey& storage_key));
  void AbortTransactionsAndCompactDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const blink::StorageKey& storage_key) override {
    base::OnceCallback<void(leveldb::Status)>* callback_ref = &callback;
    AbortTransactionsAndCompactDatabaseProxy(callback_ref, storage_key);
  }
  MOCK_METHOD2(AbortTransactionsForDatabaseProxy,
               void(base::OnceCallback<void(leveldb::Status)>* callback,
                    const blink::StorageKey& storage_key));
  void AbortTransactionsForDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const blink::StorageKey& storage_key) override {
    base::OnceCallback<void(leveldb::Status)>* callback_ref = &callback;
    AbortTransactionsForDatabaseProxy(callback_ref, storage_key);
  }

  MOCK_METHOD1(HandleBackingStoreFailure,
               void(const blink::StorageKey& storage_key));
  MOCK_METHOD2(HandleBackingStoreCorruption,
               void(const blink::StorageKey& storage_key,
                    const IndexedDBDatabaseError& error));
  // The Android NDK implements a subset of STL, and the gtest templates can't
  // deal with std::pair's. This means we can't use GoogleMock for this method
  std::vector<IndexedDBDatabase*> GetOpenDatabasesForStorageKey(
      const blink::StorageKey& storage_key) const override;
  MOCK_METHOD2(ForceClose,
               void(const blink::StorageKey& storage_key,
                    bool delete_in_memory_store));
  MOCK_METHOD1(ForceSchemaDowngrade,
               void(const blink::StorageKey& storage_key));
  MOCK_METHOD1(HasV2SchemaCorruption,
               V2SchemaCorruptionStatus(const blink::StorageKey& storage_key));
  MOCK_METHOD0(ContextDestroyed, void());

  MOCK_METHOD1(BlobFilesCleaned, void(const blink::StorageKey& storage_key));

  MOCK_CONST_METHOD1(GetConnectionCount,
                     size_t(const blink::StorageKey& storage_key));

  MOCK_CONST_METHOD1(GetInMemoryDBSize,
                     int64_t(const blink::StorageKey& storage_key));

  MOCK_CONST_METHOD1(GetLastModified,
                     base::Time(const blink::StorageKey& storage_key));

  MOCK_METHOD2(ReportOutstandingBlobs,
               void(const blink::StorageKey& storage_key,
                    bool blobs_outstanding));

  MOCK_METHOD1(NotifyIndexedDBListChanged,
               void(const blink::StorageKey& storage_key));
  MOCK_METHOD3(NotifyIndexedDBContentChanged,
               void(const blink::StorageKey& storage_key,
                    const std::u16string& database_name,
                    const std::u16string& object_store_name));
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_FACTORY_H_
