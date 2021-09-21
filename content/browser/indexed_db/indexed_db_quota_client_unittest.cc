// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_quota_client.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

// Declared to shorten the line lengths.
static const StorageType kTemp = StorageType::kTemporary;

namespace content {

// Base class for our test fixtures.
class IndexedDBQuotaClientTest : public testing::Test {
 public:
  const StorageKey kStorageKeyA;
  const StorageKey kStorageKeyB;
  const StorageKey kStorageKeyOther;

  IndexedDBQuotaClientTest()
      : kStorageKeyA(StorageKey::CreateFromStringForTesting("http://host")),
        kStorageKeyB(
            StorageKey::CreateFromStringForTesting("http://host:8000")),
        kStorageKeyOther(
            StorageKey::CreateFromStringForTesting("http://other")) {
    CreateTempDir();
    auto quota_manager = base::MakeRefCounted<storage::MockQuotaManager>(
        /*in_memory=*/false, temp_dir_.GetPath(),
        base::ThreadTaskRunnerHandle::Get(), nullptr);

    idb_context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir_.GetPath(), quota_manager->proxy(),
        base::DefaultClock::GetInstance(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunnerHandle::Get(),
        base::SequencedTaskRunnerHandle::Get());
    base::RunLoop().RunUntilIdle();
    SetupTempDir();
  }

  void CreateTempDir() { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void SetupTempDir() {
    ASSERT_TRUE(base::CreateDirectory(idb_context_->data_path()));
  }

  IndexedDBQuotaClientTest(const IndexedDBQuotaClientTest&) = delete;
  IndexedDBQuotaClientTest& operator=(const IndexedDBQuotaClientTest&) = delete;

  ~IndexedDBQuotaClientTest() override {
    base::RunLoop().RunUntilIdle();
    idb_context_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  static int64_t GetStorageKeyUsage(storage::mojom::QuotaClient& client,
                                    const StorageKey& storage_key,
                                    StorageType type) {
    int result = -1;
    base::RunLoop loop;
    client.GetStorageKeyUsage(storage_key, type,
                              base::BindLambdaForTesting([&](int64_t usage) {
                                result = usage;
                                loop.Quit();
                              }));
    loop.Run();
    EXPECT_GT(result, -1);
    return result;
  }

  static std::vector<StorageKey> GetStorageKeysForType(
      storage::mojom::QuotaClient& client,
      StorageType type) {
    std::vector<StorageKey> result;
    base::RunLoop loop;
    client.GetStorageKeysForType(
        type, base::BindLambdaForTesting(
                  [&](const std::vector<StorageKey>& storage_keys) {
                    result = storage_keys;
                    loop.Quit();
                  }));
    loop.Run();
    return result;
  }

  static std::vector<StorageKey> GetStorageKeysForHost(
      storage::mojom::QuotaClient& client,
      StorageType type,
      const std::string& host) {
    std::vector<StorageKey> result;
    base::RunLoop loop;
    client.GetStorageKeysForHost(
        type, host,
        base::BindLambdaForTesting(
            [&](const std::vector<StorageKey>& storage_keys) {
              result = storage_keys;
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  static blink::mojom::QuotaStatusCode DeleteStorageKeyData(
      storage::mojom::QuotaClient& client,
      const StorageKey& storage_key,
      StorageType type) {
    blink::mojom::QuotaStatusCode result =
        blink::mojom::QuotaStatusCode::kUnknown;
    base::RunLoop loop;
    client.DeleteStorageKeyData(
        storage_key, type,
        base::BindLambdaForTesting([&](blink::mojom::QuotaStatusCode code) {
          result = code;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  IndexedDBContextImpl* idb_context() { return idb_context_.get(); }

  void SetFileSizeTo(const base::FilePath& path, int size) {
    std::string junk(size, 'a');
    ASSERT_TRUE(base::WriteFile(path, junk));
  }

  void AddFakeIndexedDB(const StorageKey& storage_key, int size) {
    base::FilePath file_path_storage_key;
    {
      base::RunLoop run_loop;
      idb_context()->GetFilePathForTesting(
          blink::StorageKey(storage_key),
          base::BindLambdaForTesting([&](const base::FilePath& path) {
            file_path_storage_key = path;
            run_loop.Quit();
          }));
      run_loop.Run();
    }
    if (!base::CreateDirectory(file_path_storage_key)) {
      LOG(ERROR) << "failed to base::CreateDirectory "
                 << file_path_storage_key.value();
    }
    file_path_storage_key =
        file_path_storage_key.Append(FILE_PATH_LITERAL("fake_file"));
    SetFileSizeTo(file_path_storage_key, size);

    {
      base::RunLoop run_loop;
      idb_context()->ResetCachesForTesting(run_loop.QuitClosure());
      run_loop.Run();
    }
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<IndexedDBContextImpl> idb_context_;
  base::WeakPtrFactory<IndexedDBQuotaClientTest> weak_factory_{this};
};

TEST_F(IndexedDBQuotaClientTest, GetStorageKeyUsage) {
  IndexedDBQuotaClient client(*idb_context());

  AddFakeIndexedDB(kStorageKeyA, 6);
  AddFakeIndexedDB(kStorageKeyB, 3);
  EXPECT_EQ(6, GetStorageKeyUsage(client, kStorageKeyA, kTemp));
  EXPECT_EQ(3, GetStorageKeyUsage(client, kStorageKeyB, kTemp));

  AddFakeIndexedDB(kStorageKeyA, 1000);
  EXPECT_EQ(1000, GetStorageKeyUsage(client, kStorageKeyA, kTemp));
  EXPECT_EQ(3, GetStorageKeyUsage(client, kStorageKeyB, kTemp));
}

TEST_F(IndexedDBQuotaClientTest, GetStorageKeysForHost) {
  IndexedDBQuotaClient client(*idb_context());

  EXPECT_EQ(kStorageKeyA.origin().host(), kStorageKeyB.origin().host());
  EXPECT_NE(kStorageKeyA.origin().host(), kStorageKeyOther.origin().host());

  std::vector<StorageKey> storage_keys =
      GetStorageKeysForHost(client, kTemp, kStorageKeyA.origin().host());
  EXPECT_TRUE(storage_keys.empty());

  AddFakeIndexedDB(kStorageKeyA, 1000);
  storage_keys =
      GetStorageKeysForHost(client, kTemp, kStorageKeyA.origin().host());
  EXPECT_EQ(storage_keys.size(), 1ul);
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyA));

  AddFakeIndexedDB(kStorageKeyB, 1000);
  storage_keys =
      GetStorageKeysForHost(client, kTemp, kStorageKeyA.origin().host());
  EXPECT_EQ(storage_keys.size(), 2ul);
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyA));
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyB));

  EXPECT_TRUE(
      GetStorageKeysForHost(client, kTemp, kStorageKeyOther.origin().host())
          .empty());
}

TEST_F(IndexedDBQuotaClientTest, GetStorageKeysForType) {
  IndexedDBQuotaClient client(*idb_context());

  EXPECT_TRUE(GetStorageKeysForType(client, kTemp).empty());

  AddFakeIndexedDB(kStorageKeyA, 1000);
  std::vector<StorageKey> storage_keys = GetStorageKeysForType(client, kTemp);
  EXPECT_EQ(storage_keys.size(), 1ul);
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyA));
}

TEST_F(IndexedDBQuotaClientTest, DeleteStorageKey) {
  IndexedDBQuotaClient client(*idb_context());

  AddFakeIndexedDB(kStorageKeyA, 1000);
  AddFakeIndexedDB(kStorageKeyB, 50);
  EXPECT_EQ(1000, GetStorageKeyUsage(client, kStorageKeyA, kTemp));
  EXPECT_EQ(50, GetStorageKeyUsage(client, kStorageKeyB, kTemp));

  blink::mojom::QuotaStatusCode delete_status =
      DeleteStorageKeyData(client, kStorageKeyA, kTemp);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, delete_status);
  EXPECT_EQ(0, GetStorageKeyUsage(client, kStorageKeyA, kTemp));
  EXPECT_EQ(50, GetStorageKeyUsage(client, kStorageKeyB, kTemp));
}

}  // namespace content
