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
#include "base/test/bind_test_util.h"
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
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::StorageType;

// Declared to shorten the line lengths.
static const StorageType kTemp = StorageType::kTemporary;

namespace content {

// Base class for our test fixtures.
class IndexedDBQuotaClientTest : public testing::Test {
 public:
  const url::Origin kOriginA;
  const url::Origin kOriginB;
  const url::Origin kOriginOther;

  IndexedDBQuotaClientTest()
      : kOriginA(url::Origin::Create(GURL("http://host"))),
        kOriginB(url::Origin::Create(GURL("http://host:8000"))),
        kOriginOther(url::Origin::Create(GURL("http://other"))) {
    CreateTempDir();
    auto quota_manager = base::MakeRefCounted<storage::MockQuotaManager>(
        /*in_memory=*/false, temp_dir_.GetPath(),
        base::ThreadTaskRunnerHandle::Get(), nullptr);

    idb_context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir_.GetPath(), quota_manager->proxy(),
        base::DefaultClock::GetInstance(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*native_file_system_context=*/mojo::NullRemote(),
        base::SequencedTaskRunnerHandle::Get(),
        base::SequencedTaskRunnerHandle::Get());
    base::RunLoop().RunUntilIdle();
    SetupTempDir();
  }

  void CreateTempDir() { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void SetupTempDir() {
    base::FilePath indexeddb_dir =
        temp_dir_.GetPath().Append(IndexedDBContextImpl::kIndexedDBDirectory);
    ASSERT_TRUE(base::CreateDirectory(indexeddb_dir));
    idb_context()->set_data_path_for_testing(indexeddb_dir);
  }

  ~IndexedDBQuotaClientTest() override {
    base::RunLoop().RunUntilIdle();
    idb_context_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  static int64_t GetOriginUsage(scoped_refptr<storage::QuotaClient> client,
                                const url::Origin& origin,
                                StorageType type) {
    int result = -1;
    base::RunLoop loop;
    client->GetOriginUsage(origin, type,
                           base::BindLambdaForTesting([&](int64_t usage) {
                             result = usage;
                             loop.Quit();
                           }));
    loop.Run();
    EXPECT_GT(result, -1);
    return result;
  }

  static std::vector<url::Origin> GetOriginsForType(
      scoped_refptr<storage::QuotaClient> client,
      StorageType type) {
    std::vector<url::Origin> result;
    base::RunLoop loop;
    client->GetOriginsForType(type,
                              base::BindLambdaForTesting(
                                  [&](const std::vector<url::Origin>& origins) {
                                    result = origins;
                                    loop.Quit();
                                  }));
    loop.Run();
    return result;
  }

  static std::vector<url::Origin> GetOriginsForHost(
      scoped_refptr<storage::QuotaClient> client,
      StorageType type,
      const std::string& host) {
    std::vector<url::Origin> result;
    base::RunLoop loop;
    client->GetOriginsForHost(type, host,
                              base::BindLambdaForTesting(
                                  [&](const std::vector<url::Origin>& origins) {
                                    result = origins;
                                    loop.Quit();
                                  }));
    loop.Run();
    return result;
  }

  static blink::mojom::QuotaStatusCode DeleteOriginData(
      scoped_refptr<storage::QuotaClient> client,
      const url::Origin& origin,
      StorageType type) {
    blink::mojom::QuotaStatusCode result =
        blink::mojom::QuotaStatusCode::kUnknown;
    base::RunLoop loop;
    client->DeleteOriginData(
        origin, type,
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

  void AddFakeIndexedDB(const url::Origin& origin, int size) {
    base::FilePath file_path_origin;
    {
      base::RunLoop run_loop;
      idb_context()->GetFilePathForTesting(
          origin, base::BindLambdaForTesting([&](const base::FilePath& path) {
            file_path_origin = path;
            run_loop.Quit();
          }));
      run_loop.Run();
    }
    if (!base::CreateDirectory(file_path_origin)) {
      LOG(ERROR) << "failed to base::CreateDirectory "
                 << file_path_origin.value();
    }
    file_path_origin = file_path_origin.Append(FILE_PATH_LITERAL("fake_file"));
    SetFileSizeTo(file_path_origin, size);

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

  DISALLOW_COPY_AND_ASSIGN(IndexedDBQuotaClientTest);
};

TEST_F(IndexedDBQuotaClientTest, GetOriginUsage) {
  auto client = base::MakeRefCounted<IndexedDBQuotaClient>(idb_context());

  AddFakeIndexedDB(kOriginA, 6);
  AddFakeIndexedDB(kOriginB, 3);
  EXPECT_EQ(6, GetOriginUsage(client, kOriginA, kTemp));
  EXPECT_EQ(3, GetOriginUsage(client, kOriginB, kTemp));

  AddFakeIndexedDB(kOriginA, 1000);
  EXPECT_EQ(1000, GetOriginUsage(client, kOriginA, kTemp));
  EXPECT_EQ(3, GetOriginUsage(client, kOriginB, kTemp));
}

TEST_F(IndexedDBQuotaClientTest, GetOriginsForHost) {
  auto client = base::MakeRefCounted<IndexedDBQuotaClient>(idb_context());

  EXPECT_EQ(kOriginA.host(), kOriginB.host());
  EXPECT_NE(kOriginA.host(), kOriginOther.host());

  std::vector<url::Origin> origins =
      GetOriginsForHost(client, kTemp, kOriginA.host());
  EXPECT_TRUE(origins.empty());

  AddFakeIndexedDB(kOriginA, 1000);
  origins = GetOriginsForHost(client, kTemp, kOriginA.host());
  EXPECT_EQ(origins.size(), 1ul);
  EXPECT_THAT(origins, testing::Contains(kOriginA));

  AddFakeIndexedDB(kOriginB, 1000);
  origins = GetOriginsForHost(client, kTemp, kOriginA.host());
  EXPECT_EQ(origins.size(), 2ul);
  EXPECT_THAT(origins, testing::Contains(kOriginA));
  EXPECT_THAT(origins, testing::Contains(kOriginB));

  EXPECT_TRUE(GetOriginsForHost(client, kTemp, kOriginOther.host()).empty());
}

TEST_F(IndexedDBQuotaClientTest, GetOriginsForType) {
  auto client = base::MakeRefCounted<IndexedDBQuotaClient>(idb_context());

  EXPECT_TRUE(GetOriginsForType(client, kTemp).empty());

  AddFakeIndexedDB(kOriginA, 1000);
  std::vector<url::Origin> origins = GetOriginsForType(client, kTemp);
  EXPECT_EQ(origins.size(), 1ul);
  EXPECT_THAT(origins, testing::Contains(kOriginA));
}

TEST_F(IndexedDBQuotaClientTest, DeleteOrigin) {
  auto client = base::MakeRefCounted<IndexedDBQuotaClient>(idb_context());

  AddFakeIndexedDB(kOriginA, 1000);
  AddFakeIndexedDB(kOriginB, 50);
  EXPECT_EQ(1000, GetOriginUsage(client, kOriginA, kTemp));
  EXPECT_EQ(50, GetOriginUsage(client, kOriginB, kTemp));

  blink::mojom::QuotaStatusCode delete_status =
      DeleteOriginData(client, kOriginA, kTemp);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, delete_status);
  EXPECT_EQ(0, GetOriginUsage(client, kOriginA, kTemp));
  EXPECT_EQ(50, GetOriginUsage(client, kOriginB, kTemp));
}

}  // namespace content
