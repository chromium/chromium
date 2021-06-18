// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_manager.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {

namespace {

constexpr StorageType kTemporary = StorageType::kTemporary;
constexpr StorageType kPersistent = StorageType::kPersistent;

constexpr QuotaClientType kClientFile = QuotaClientType::kFileSystem;
constexpr QuotaClientType kClientDB = QuotaClientType::kIndexedDatabase;

}  // namespace

class MockQuotaManagerTest : public testing::Test {
 public:
  MockQuotaManagerTest() : deletion_callback_count_(0) {}

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    policy_ = new MockSpecialStoragePolicy;
    manager_ = new MockQuotaManager(
        false /* is_incognito */, data_dir_.GetPath(),
        base::ThreadTaskRunnerHandle::Get().get(), policy_.get());
  }

  void TearDown() override {
    // Make sure the quota manager cleans up correctly.
    manager_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  void GetModifiedStorageKeys(StorageType type,
                              base::Time begin,
                              base::Time end) {
    manager_->GetStorageKeysModifiedBetween(
        type, begin, end,
        base::BindOnce(&MockQuotaManagerTest::GotModifiedStorageKeys,
                       weak_factory_.GetWeakPtr()));
  }

  void GotModifiedStorageKeys(const std::set<StorageKey>& storage_keys,
                              StorageType type) {
    storage_keys_ = storage_keys;
    type_ = type;
  }

  void DeleteStorageKeyData(const StorageKey& storage_key,
                            StorageType type,
                            QuotaClientTypes quota_client_types) {
    manager_->DeleteStorageKeyData(
        storage_key, type, std::move(quota_client_types),
        base::BindOnce(&MockQuotaManagerTest::DeletedStorageKeyData,
                       weak_factory_.GetWeakPtr()));
  }

  void DeletedStorageKeyData(blink::mojom::QuotaStatusCode status) {
    ++deletion_callback_count_;
    EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, status);
  }

  int deletion_callback_count() const {
    return deletion_callback_count_;
  }

  MockQuotaManager* manager() const {
    return manager_.get();
  }

  const std::set<StorageKey>& storage_keys() const { return storage_keys_; }

  const StorageType& type() const {
    return type_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<MockQuotaManager> manager_;
  scoped_refptr<MockSpecialStoragePolicy> policy_;

  int deletion_callback_count_;

  std::set<StorageKey> storage_keys_;
  StorageType type_;

  base::WeakPtrFactory<MockQuotaManagerTest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockQuotaManagerTest);
};

TEST_F(MockQuotaManagerTest, BasicStorageKeyManipulation) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://host1:1/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://host2:1/");

  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey1, kTemporary, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey1, kTemporary, kClientDB));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey1, kPersistent, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey1, kPersistent, kClientDB));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kTemporary, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kTemporary, kClientDB));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kPersistent, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kPersistent, kClientDB));

  manager()->AddStorageKey(kStorageKey1, kTemporary, {kClientFile},
                           base::Time::Now());
  EXPECT_TRUE(
      manager()->StorageKeyHasData(kStorageKey1, kTemporary, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey1, kTemporary, kClientDB));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey1, kPersistent, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey1, kPersistent, kClientDB));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kTemporary, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kTemporary, kClientDB));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kPersistent, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kPersistent, kClientDB));

  manager()->AddStorageKey(kStorageKey1, kPersistent, {kClientFile},
                           base::Time::Now());
  EXPECT_TRUE(
      manager()->StorageKeyHasData(kStorageKey1, kTemporary, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey1, kTemporary, kClientDB));
  EXPECT_TRUE(
      manager()->StorageKeyHasData(kStorageKey1, kPersistent, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey1, kPersistent, kClientDB));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kTemporary, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kTemporary, kClientDB));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kPersistent, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kPersistent, kClientDB));

  manager()->AddStorageKey(kStorageKey2, kTemporary, {kClientFile, kClientDB},
                           base::Time::Now());
  EXPECT_TRUE(
      manager()->StorageKeyHasData(kStorageKey1, kTemporary, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey1, kTemporary, kClientDB));
  EXPECT_TRUE(
      manager()->StorageKeyHasData(kStorageKey1, kPersistent, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey1, kPersistent, kClientDB));
  EXPECT_TRUE(
      manager()->StorageKeyHasData(kStorageKey2, kTemporary, kClientFile));
  EXPECT_TRUE(
      manager()->StorageKeyHasData(kStorageKey2, kTemporary, kClientDB));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kPersistent, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kPersistent, kClientDB));
}

TEST_F(MockQuotaManagerTest, StorageKeyDeletion) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://host1:1/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://host2:1/");
  const StorageKey kStorageKey3 =
      StorageKey::CreateFromStringForTesting("http://host3:1/");

  manager()->AddStorageKey(kStorageKey1, kTemporary, {kClientFile},
                           base::Time::Now());
  manager()->AddStorageKey(kStorageKey2, kTemporary, {kClientFile, kClientDB},
                           base::Time::Now());
  manager()->AddStorageKey(kStorageKey3, kTemporary, {kClientFile, kClientDB},
                           base::Time::Now());

  DeleteStorageKeyData(kStorageKey2, kTemporary, {kClientFile});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, deletion_callback_count());
  EXPECT_TRUE(
      manager()->StorageKeyHasData(kStorageKey1, kTemporary, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kTemporary, kClientFile));
  EXPECT_TRUE(
      manager()->StorageKeyHasData(kStorageKey2, kTemporary, kClientDB));
  EXPECT_TRUE(
      manager()->StorageKeyHasData(kStorageKey3, kTemporary, kClientFile));
  EXPECT_TRUE(
      manager()->StorageKeyHasData(kStorageKey3, kTemporary, kClientDB));

  DeleteStorageKeyData(kStorageKey3, kTemporary, {kClientFile, kClientDB});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, deletion_callback_count());
  EXPECT_TRUE(
      manager()->StorageKeyHasData(kStorageKey1, kTemporary, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey2, kTemporary, kClientFile));
  EXPECT_TRUE(
      manager()->StorageKeyHasData(kStorageKey2, kTemporary, kClientDB));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey3, kTemporary, kClientFile));
  EXPECT_FALSE(
      manager()->StorageKeyHasData(kStorageKey3, kTemporary, kClientDB));
}

TEST_F(MockQuotaManagerTest, ModifiedStorageKeys) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://host1:1/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://host2:1/");

  base::Time now = base::Time::Now();
  base::Time then = base::Time();
  base::TimeDelta an_hour = base::TimeDelta::FromMilliseconds(3600000);
  base::TimeDelta a_minute = base::TimeDelta::FromMilliseconds(60000);

  GetModifiedStorageKeys(kTemporary, then, base::Time::Max());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(storage_keys().empty());

  manager()->AddStorageKey(kStorageKey1, kTemporary, {kClientFile},
                           now - an_hour);

  GetModifiedStorageKeys(kTemporary, then, base::Time::Max());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(1UL, storage_keys().size());
  EXPECT_EQ(1UL, storage_keys().count(kStorageKey1));
  EXPECT_EQ(0UL, storage_keys().count(kStorageKey2));

  manager()->AddStorageKey(kStorageKey2, kTemporary, {kClientFile}, now);

  GetModifiedStorageKeys(kTemporary, then, base::Time::Max());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(2UL, storage_keys().size());
  EXPECT_EQ(1UL, storage_keys().count(kStorageKey1));
  EXPECT_EQ(1UL, storage_keys().count(kStorageKey2));

  GetModifiedStorageKeys(kTemporary, then, now);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(1UL, storage_keys().size());
  EXPECT_EQ(1UL, storage_keys().count(kStorageKey1));
  EXPECT_EQ(0UL, storage_keys().count(kStorageKey2));

  GetModifiedStorageKeys(kTemporary, now - a_minute, now + a_minute);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(1UL, storage_keys().size());
  EXPECT_EQ(0UL, storage_keys().count(kStorageKey1));
  EXPECT_EQ(1UL, storage_keys().count(kStorageKey2));
}
}  // namespace storage
