// Copyright 2014 The Chromium Authors
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
                    const storage::BucketLocator& bucket_locator,
                    const base::FilePath& data_directory));
  MOCK_METHOD4(OpenProxy,
               void(const std::u16string& name,
                    IndexedDBPendingConnection* connection,
                    const blink::StorageKey& storage_key,
                    const base::FilePath& data_directory));
  // Googlemock can't deal with move-only types, so *Proxy() is a workaround.
  void Open(const std::u16string& name,
            std::unique_ptr<IndexedDBPendingConnection> connection,
            const storage::BucketLocator& bucket_locator,
            const base::FilePath& data_directory) override {
    OpenProxy(name, connection.get(), bucket_locator.storage_key,
              data_directory);
  }
  MOCK_METHOD5(DeleteDatabase,
               void(const std::u16string& name,
                    scoped_refptr<IndexedDBCallbacks> callbacks,
                    const storage::BucketLocator& bucket_locator,
                    const base::FilePath& data_directory,
                    bool force_close));

  MOCK_METHOD1(HandleBackingStoreFailure,
               void(const storage::BucketLocator& bucket_locator));
  MOCK_METHOD2(HandleBackingStoreCorruption,
               void(const storage::BucketLocator& bucket_locator,
                    const IndexedDBDatabaseError& error));
  // The Android NDK implements a subset of STL, and the gtest templates can't
  // deal with std::pair's. This means we can't use GoogleMock for this method
  std::vector<IndexedDBDatabase*> GetOpenDatabasesForBucket(
      const storage::BucketLocator& bucket_locator) const override;
  MOCK_METHOD2(ForceClose,
               void(storage::BucketId bucket_locator,
                    bool delete_in_memory_store));
  MOCK_METHOD1(ForceSchemaDowngrade,
               void(const storage::BucketLocator& bucket_locator));
  MOCK_METHOD1(
      HasV2SchemaCorruption,
      V2SchemaCorruptionStatus(const storage::BucketLocator& bucket_locator));
  MOCK_METHOD0(ContextDestroyed, void());

  MOCK_METHOD1(BlobFilesCleaned,
               void(const storage::BucketLocator& bucket_locator));

  MOCK_CONST_METHOD1(GetConnectionCount,
                     size_t(storage::BucketId bucket_locator));

  MOCK_CONST_METHOD1(GetInMemoryDBSize,
                     int64_t(const storage::BucketLocator& bucket_locator));

  MOCK_CONST_METHOD1(GetLastModified,
                     base::Time(const storage::BucketLocator& bucket_locator));

  MOCK_METHOD2(ReportOutstandingBlobs,
               void(const storage::BucketLocator& bucket_locator,
                    bool blobs_outstanding));

  MOCK_METHOD1(NotifyIndexedDBListChanged,
               void(const blink::StorageKey& storage_key));
  MOCK_METHOD3(NotifyIndexedDBContentChanged,
               void(const storage::BucketLocator& bucket_locator,
                    const std::u16string& database_name,
                    const std::u16string& object_store_name));
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_FACTORY_H_
