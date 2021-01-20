// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "storage/browser/database/database_quota_client.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/common/database/database_identifier.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

// Declared to shorten the line lengths.
static const blink::mojom::StorageType kTemp =
    blink::mojom::StorageType::kTemporary;

// Mock tracker class the mocks up those methods of the tracker
// that are used by the QuotaClient.
class MockDatabaseTracker : public DatabaseTracker {
 public:
  MockDatabaseTracker()
      : DatabaseTracker(base::FilePath(), false, nullptr, nullptr) {}

  bool GetOriginInfo(const std::string& origin_identifier,
                     OriginInfo* info) override {
    auto found =
        mock_origin_infos_.find(GetOriginFromIdentifier(origin_identifier));
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

  void DeleteDataForOrigin(const url::Origin& origin,
                           net::CompletionOnceCallback callback) override {
    ++delete_called_count_;
    if (async_delete()) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&MockDatabaseTracker::AsyncDeleteDataForOrigin, this,
                         std::move(callback)));
      return;
    }
    std::move(callback).Run(net::OK);
  }

  void AsyncDeleteDataForOrigin(net::CompletionOnceCallback callback) {
    std::move(callback).Run(net::OK);
  }

  void AddMockDatabase(const url::Origin& origin, const char* name, int size) {
    MockOriginInfo& info = mock_origin_infos_[origin];
    info.set_origin(GetIdentifierFromOrigin(origin));
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
      EXPECT_FALSE(base::Contains(database_sizes_, name));
      database_sizes_[name] = size;
      total_size_ += size;
    }
  };

  int delete_called_count_ = 0;
  bool async_delete_ = false;
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
        mock_tracker_(base::MakeRefCounted<MockDatabaseTracker>()) {}

  static int64_t GetOriginUsage(QuotaClient& client,
                                const url::Origin& origin,
                                blink::mojom::StorageType type) {
    int result = -1;
    base::RunLoop loop;
    client.GetOriginUsage(origin, type,
                          base::BindLambdaForTesting([&](int64_t usage) {
                            result = usage;
                            loop.Quit();
                          }));
    loop.Run();
    EXPECT_GT(result, -1);
    return result;
  }

  static std::vector<url::Origin> GetOriginsForType(
      QuotaClient& client,
      blink::mojom::StorageType type) {
    std::vector<url::Origin> result;
    base::RunLoop loop;
    client.GetOriginsForType(type,
                             base::BindLambdaForTesting(
                                 [&](const std::vector<url::Origin>& origins) {
                                   result = origins;
                                   loop.Quit();
                                 }));
    loop.Run();
    return result;
  }

  static std::vector<url::Origin> GetOriginsForHost(
      QuotaClient& client,
      blink::mojom::StorageType type,
      const std::string& host) {
    std::vector<url::Origin> result;
    base::RunLoop loop;
    client.GetOriginsForHost(type, host,
                             base::BindLambdaForTesting(
                                 [&](const std::vector<url::Origin>& origins) {
                                   result = origins;
                                   loop.Quit();
                                 }));
    loop.Run();
    return result;
  }

  static blink::mojom::QuotaStatusCode DeleteOriginData(
      QuotaClient& client,
      blink::mojom::StorageType type,
      const url::Origin& origin) {
    blink::mojom::QuotaStatusCode result =
        blink::mojom::QuotaStatusCode::kUnknown;
    base::RunLoop loop;
    client.DeleteOriginData(
        origin, type,
        base::BindLambdaForTesting([&](blink::mojom::QuotaStatusCode code) {
          result = code;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockDatabaseTracker> mock_tracker_;
  base::WeakPtrFactory<DatabaseQuotaClientTest> weak_factory_{this};
};

TEST_F(DatabaseQuotaClientTest, GetOriginUsage) {
  auto client = base::MakeRefCounted<DatabaseQuotaClient>(mock_tracker_);

  EXPECT_EQ(0, GetOriginUsage(*client, kOriginA, kTemp));

  mock_tracker_->AddMockDatabase(kOriginA, "fooDB", 1000);
  EXPECT_EQ(1000, GetOriginUsage(*client, kOriginA, kTemp));

  EXPECT_EQ(0, GetOriginUsage(*client, kOriginB, kTemp));
}

TEST_F(DatabaseQuotaClientTest, GetOriginsForHost) {
  auto client = base::MakeRefCounted<DatabaseQuotaClient>(mock_tracker_);

  EXPECT_EQ(kOriginA.host(), kOriginB.host());
  EXPECT_NE(kOriginA.host(), kOriginOther.host());

  std::vector<url::Origin> origins =
      GetOriginsForHost(*client, kTemp, kOriginA.host());
  EXPECT_TRUE(origins.empty());

  mock_tracker_->AddMockDatabase(kOriginA, "fooDB", 1000);
  origins = GetOriginsForHost(*client, kTemp, kOriginA.host());
  EXPECT_EQ(origins.size(), 1ul);
  EXPECT_THAT(origins, testing::Contains(kOriginA));

  mock_tracker_->AddMockDatabase(kOriginB, "barDB", 1000);
  origins = GetOriginsForHost(*client, kTemp, kOriginA.host());
  EXPECT_EQ(origins.size(), 2ul);
  EXPECT_THAT(origins, testing::Contains(kOriginA));
  EXPECT_THAT(origins, testing::Contains(kOriginB));

  EXPECT_TRUE(GetOriginsForHost(*client, kTemp, kOriginOther.host()).empty());
}

TEST_F(DatabaseQuotaClientTest, GetOriginsForType) {
  auto client = base::MakeRefCounted<DatabaseQuotaClient>(mock_tracker_);

  EXPECT_TRUE(GetOriginsForType(*client, kTemp).empty());

  mock_tracker_->AddMockDatabase(kOriginA, "fooDB", 1000);
  std::vector<url::Origin> origins = GetOriginsForType(*client, kTemp);
  EXPECT_EQ(origins.size(), 1ul);
  EXPECT_THAT(origins, testing::Contains(kOriginA));
}

TEST_F(DatabaseQuotaClientTest, DeleteOriginData) {
  auto client = base::MakeRefCounted<DatabaseQuotaClient>(mock_tracker_);

  mock_tracker_->set_async_delete(false);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            DeleteOriginData(*client, kTemp, kOriginA));
  EXPECT_EQ(1, mock_tracker_->delete_called_count());

  mock_tracker_->set_async_delete(true);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            DeleteOriginData(*client, kTemp, kOriginA));
  EXPECT_EQ(2, mock_tracker_->delete_called_count());
}

}  // namespace storage
