// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "storage/browser/database/database_quota_client.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/database/database_util.h"
#include "storage/common/database/database_identifier.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::DatabaseQuotaClient;
using storage::DatabaseTracker;
using storage::OriginInfo;

namespace content {

// Declared to shorten the line lengths.
static const blink::mojom::StorageType kTemp =
    blink::mojom::StorageType::kTemporary;
static const blink::mojom::StorageType kPerm =
    blink::mojom::StorageType::kPersistent;

// Mock tracker class the mocks up those methods of the tracker
// that are used by the QuotaClient.
class MockDatabaseTracker : public DatabaseTracker {
 public:
  MockDatabaseTracker()
      : DatabaseTracker(base::FilePath(), false, nullptr, nullptr),
        delete_called_count_(0),
        async_delete_(false) {
    set_task_runner_for_testing(base::ThreadTaskRunnerHandle::Get());
  }

  bool GetOriginInfo(const std::string& origin_identifier,
                     OriginInfo* info) override {
    auto found = mock_origin_infos_.find(
        storage::GetOriginFromIdentifier(origin_identifier));
    if (found == mock_origin_infos_.end())
      return false;
    *info = OriginInfo(found->second);
    return true;
  }

  bool GetAllOriginIdentifiers(
      std::vector<std::string>* origins_identifiers) override {
    for (const auto& origin_info : mock_origin_infos_)
      origins_identifiers->push_back(origin_info.second.GetOriginIdentifier());
    return true;
  }

  bool GetAllOriginsInfo(std::vector<OriginInfo>* origins_info) override {
    for (const auto& origin_info : mock_origin_infos_)
      origins_info->push_back(OriginInfo(origin_info.second));
    return true;
  }

  int DeleteDataForOrigin(const url::Origin& origin,
                          net::CompletionOnceCallback callback) override {
    ++delete_called_count_;
    if (async_delete()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&MockDatabaseTracker::AsyncDeleteDataForOrigin, this,
                         std::move(callback)));
      return net::ERR_IO_PENDING;
    }
    return net::OK;
  }

  void AsyncDeleteDataForOrigin(net::CompletionOnceCallback callback) {
    std::move(callback).Run(net::OK);
  }

  void AddMockDatabase(const url::Origin& origin, const char* name, int size) {
    MockOriginInfo& info = mock_origin_infos_[origin];
    info.set_origin(storage::GetIdentifierFromOrigin(origin));
    info.AddMockDatabase(base::ASCIIToUTF16(name), size);
  }

  int delete_called_count() { return delete_called_count_; }
  bool async_delete() { return async_delete_; }
  void set_async_delete(bool async) { async_delete_ = async; }

 protected:
  ~MockDatabaseTracker() override = default;

 private:
  class MockOriginInfo : public OriginInfo {
   public:
    void set_origin(const std::string& origin_identifier) {
      origin_identifier_ = origin_identifier;
    }

    void AddMockDatabase(const base::string16& name, int size) {
      EXPECT_TRUE(database_info_.find(name) == database_info_.end());
      database_info_[name].size = size;
      total_size_ += size;
    }
  };

  int delete_called_count_;
  bool async_delete_;
  std::map<url::Origin, MockOriginInfo> mock_origin_infos_;
};

// Base class for our test fixtures.
class DatabaseQuotaClientTest : public testing::Test {
 public:
  const url::Origin kOriginA;
  const url::Origin kOriginB;
  const url::Origin kOriginOther;

  DatabaseQuotaClientTest()
      : kOriginA(url::Origin::Create(GURL("http://host"))),
        kOriginB(url::Origin::Create(GURL("http://host:8000"))),
        kOriginOther(url::Origin::Create(GURL("http://other"))),
        usage_(0),
        mock_tracker_(new MockDatabaseTracker) {}

  int64_t GetOriginUsage(storage::QuotaClient* client,
                         const url::Origin& origin,
                         blink::mojom::StorageType type) {
    usage_ = 0;
    client->GetOriginUsage(
        origin, type,
        base::AdaptCallbackForRepeating(
            base::BindOnce(&DatabaseQuotaClientTest::OnGetOriginUsageComplete,
                           weak_factory_.GetWeakPtr())));
    base::RunLoop().RunUntilIdle();
    return usage_;
  }

  const std::set<url::Origin>& GetOriginsForType(
      storage::QuotaClient* client,
      blink::mojom::StorageType type) {
    origins_.clear();
    client->GetOriginsForType(
        type, base::AdaptCallbackForRepeating(
                  base::BindOnce(&DatabaseQuotaClientTest::OnGetOriginsComplete,
                                 weak_factory_.GetWeakPtr())));
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  const std::set<url::Origin>& GetOriginsForHost(storage::QuotaClient* client,
                                                 blink::mojom::StorageType type,
                                                 const std::string& host) {
    origins_.clear();
    client->GetOriginsForHost(
        type, host,
        base::AdaptCallbackForRepeating(
            base::BindOnce(&DatabaseQuotaClientTest::OnGetOriginsComplete,
                           weak_factory_.GetWeakPtr())));
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  bool DeleteOriginData(storage::QuotaClient* client,
                        blink::mojom::StorageType type,
                        const url::Origin& origin) {
    delete_status_ = blink::mojom::QuotaStatusCode::kUnknown;
    client->DeleteOriginData(
        origin, type,
        base::AdaptCallbackForRepeating(
            base::BindOnce(&DatabaseQuotaClientTest::OnDeleteOriginDataComplete,
                           weak_factory_.GetWeakPtr())));
    base::RunLoop().RunUntilIdle();
    return delete_status_ == blink::mojom::QuotaStatusCode::kOk;
  }

  MockDatabaseTracker* mock_tracker() { return mock_tracker_.get(); }

 private:
  void OnGetOriginUsageComplete(int64_t usage) { usage_ = usage; }

  void OnGetOriginsComplete(const std::set<url::Origin>& origins) {
    origins_ = origins;
  }

  void OnDeleteOriginDataComplete(blink::mojom::QuotaStatusCode status) {
    delete_status_ = status;
  }

  base::test::TaskEnvironment task_environment_;
  int64_t usage_;
  std::set<url::Origin> origins_;
  blink::mojom::QuotaStatusCode delete_status_;
  scoped_refptr<MockDatabaseTracker> mock_tracker_;
  base::WeakPtrFactory<DatabaseQuotaClientTest> weak_factory_{this};
};

TEST_F(DatabaseQuotaClientTest, GetOriginUsage) {
  DatabaseQuotaClient client(mock_tracker());

  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kPerm));

  mock_tracker()->AddMockDatabase(kOriginA, "fooDB", 1000);
  EXPECT_EQ(1000, GetOriginUsage(&client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginA, kPerm));

  EXPECT_EQ(0, GetOriginUsage(&client, kOriginB, kPerm));
  EXPECT_EQ(0, GetOriginUsage(&client, kOriginB, kTemp));
}

TEST_F(DatabaseQuotaClientTest, GetOriginsForHost) {
  DatabaseQuotaClient client(mock_tracker());

  EXPECT_EQ(kOriginA.host(), kOriginB.host());
  EXPECT_NE(kOriginA.host(), kOriginOther.host());

  std::set<url::Origin> origins =
      GetOriginsForHost(&client, kTemp, kOriginA.host());
  EXPECT_TRUE(origins.empty());

  mock_tracker()->AddMockDatabase(kOriginA, "fooDB", 1000);
  origins = GetOriginsForHost(&client, kTemp, kOriginA.host());
  EXPECT_EQ(origins.size(), 1ul);
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());

  mock_tracker()->AddMockDatabase(kOriginB, "barDB", 1000);
  origins = GetOriginsForHost(&client, kTemp, kOriginA.host());
  EXPECT_EQ(origins.size(), 2ul);
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());
  EXPECT_TRUE(origins.find(kOriginB) != origins.end());

  EXPECT_TRUE(GetOriginsForHost(&client, kPerm, kOriginA.host()).empty());
  EXPECT_TRUE(GetOriginsForHost(&client, kTemp, kOriginOther.host()).empty());
}

TEST_F(DatabaseQuotaClientTest, GetOriginsForType) {
  DatabaseQuotaClient client(mock_tracker());

  EXPECT_TRUE(GetOriginsForType(&client, kTemp).empty());
  EXPECT_TRUE(GetOriginsForType(&client, kPerm).empty());

  mock_tracker()->AddMockDatabase(kOriginA, "fooDB", 1000);
  std::set<url::Origin> origins = GetOriginsForType(&client, kTemp);
  EXPECT_EQ(origins.size(), 1ul);
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());

  EXPECT_TRUE(GetOriginsForType(&client, kPerm).empty());
}

TEST_F(DatabaseQuotaClientTest, DeleteOriginData) {
  DatabaseQuotaClient client(mock_tracker());

  // Perm deletions are short circuited in the Client and
  // should not reach the DatabaseTracker.
  EXPECT_TRUE(DeleteOriginData(&client, kPerm, kOriginA));
  EXPECT_EQ(0, mock_tracker()->delete_called_count());

  mock_tracker()->set_async_delete(false);
  EXPECT_TRUE(DeleteOriginData(&client, kTemp, kOriginA));
  EXPECT_EQ(1, mock_tracker()->delete_called_count());

  mock_tracker()->set_async_delete(true);
  EXPECT_TRUE(DeleteOriginData(&client, kTemp, kOriginA));
  EXPECT_EQ(2, mock_tracker()->delete_called_count());
}

}  // namespace content
