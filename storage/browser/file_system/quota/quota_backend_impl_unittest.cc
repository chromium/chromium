// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/quota/quota_backend_impl.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_error_or.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/obfuscated_file_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

namespace storage {

namespace {

bool DidReserveQuota(bool accepted,
                     base::File::Error* error_out,
                     int64_t* delta_out,
                     base::File::Error error,
                     int64_t delta) {
  DCHECK(error_out);
  DCHECK(delta_out);
  *error_out = error;
  *delta_out = delta;
  return accepted;
}

class MockQuotaManagerProxy : public QuotaManagerProxy {
 public:
  MockQuotaManagerProxy()
      : QuotaManagerProxy(/*quota_manager_impl=*/nullptr,
                          base::SingleThreadTaskRunner::GetCurrentDefault(),
                          /*profile_path=*/base::FilePath()) {}

  MockQuotaManagerProxy(const MockQuotaManagerProxy&) = delete;
  MockQuotaManagerProxy& operator=(const MockQuotaManagerProxy&) = delete;

  // We don't mock them.
  void SetUsageCacheEnabled(QuotaClientType client_id,
                            const blink::StorageKey& storage_key,
                            blink::mojom::StorageType type,
                            bool enabled) override {}

  void NotifyBucketModified(
      QuotaClientType client_id,
      const BucketLocator& bucket,
      std::optional<int64_t> delta,
      base::Time modification_time,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceClosure callback) override {
    ++storage_modified_count_;
    if (delta) {
      usage_ += *delta;
    } else {
      usage_ = 0;
    }
    ASSERT_LE(usage_, quota_);
    if (callback)
      callback_task_runner->PostTask(FROM_HERE, std::move(callback));
  }

  void GetUsageAndQuota(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      UsageAndQuotaCallback callback) override {
    DCHECK(callback_task_runner);
    DCHECK(callback_task_runner->RunsTasksInCurrentSequence());
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, usage_, quota_);
  }

  int storage_modified_count() { return storage_modified_count_; }
  int64_t usage() { return usage_; }
  void set_usage(int64_t usage) { usage_ = usage; }
  void set_quota(int64_t quota) { quota_ = quota; }

 protected:
  ~MockQuotaManagerProxy() override = default;

 private:
  int storage_modified_count_ = 0;
  int64_t usage_ = 0;
  int64_t quota_ = 0;
};

}  // namespace

class QuotaBackendImplTest : public testing::Test,
                             public ::testing::WithParamInterface<bool> {
 public:
  QuotaBackendImplTest()
      : file_system_usage_cache_(is_incognito()),
        quota_manager_proxy_(base::MakeRefCounted<MockQuotaManagerProxy>()) {}

  QuotaBackendImplTest(const QuotaBackendImplTest&) = delete;
  QuotaBackendImplTest& operator=(const QuotaBackendImplTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    in_memory_env_ = leveldb_chrome::NewMemEnv("quota");
    file_util_ = ObfuscatedFileUtil::CreateForTesting(
        /*special_storage_policy=*/nullptr, data_dir_.GetPath(),
        in_memory_env_.get(), is_incognito());
    backend_ = std::make_unique<QuotaBackendImpl>(
        file_task_runner(), file_util_.get(), &file_system_usage_cache_,
        quota_manager_proxy_.get());
  }

  void TearDown() override {
    backend_.reset();
    quota_manager_proxy_ = nullptr;
    file_util_.reset();
    base::RunLoop().RunUntilIdle();
  }

  bool is_incognito() { return GetParam(); }

 protected:
  void InitializeForOriginAndType(const url::Origin& origin,
                                  FileSystemType type) {
    ASSERT_TRUE(file_util_->InitOriginDatabase(origin, true /* create */));
    ASSERT_TRUE(file_util_->origin_database_ != nullptr);

    ASSERT_TRUE(file_util_
                    ->GetDirectoryForStorageKeyAndType(
                        blink::StorageKey::CreateFirstParty(origin), type,
                        true /* create */)
                    .has_value());

    ASSERT_OK_AND_ASSIGN(base::FilePath path, GetUsageCachePath(origin, type));
    ASSERT_TRUE(file_system_usage_cache_.UpdateUsage(path, 0));
  }

  base::SequencedTaskRunner* file_task_runner() {
    return base::SingleThreadTaskRunner::GetCurrentDefault().get();
  }

  base::FileErrorOr<base::FilePath> GetUsageCachePath(const url::Origin& origin,
                                                      FileSystemType type) {
    return backend_->GetUsageCachePath(origin, type)
        .transform([](base::FilePath path) {
          EXPECT_FALSE(path.empty());
          return path;
        });
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<leveldb::Env> in_memory_env_;
  std::unique_ptr<ObfuscatedFileUtil> file_util_;
  FileSystemUsageCache file_system_usage_cache_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  std::unique_ptr<QuotaBackendImpl> backend_;
};

INSTANTIATE_TEST_SUITE_P(All, QuotaBackendImplTest, testing::Bool());

TEST_P(QuotaBackendImplTest, ReserveQuota_Basic) {
  const url::Origin kOrigin = url::Origin::Create(GURL("http://example.com"));

  FileSystemType type = kFileSystemTypeTemporary;
  InitializeForOriginAndType(kOrigin, type);
  quota_manager_proxy_->set_quota(10000);

  int64_t delta = 0;

  const int64_t kDelta1 = 1000;
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend_->ReserveQuota(
      kOrigin, type, kDelta1,
      base::BindOnce(&DidReserveQuota, true, &error, &delta));
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(kDelta1, delta);
  EXPECT_EQ(kDelta1, quota_manager_proxy_->usage());

  const int64_t kDelta2 = -300;
  error = base::File::FILE_ERROR_FAILED;
  backend_->ReserveQuota(
      kOrigin, type, kDelta2,
      base::BindOnce(&DidReserveQuota, true, &error, &delta));
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(kDelta2, delta);
  EXPECT_EQ(kDelta1 + kDelta2, quota_manager_proxy_->usage());

  EXPECT_EQ(2, quota_manager_proxy_->storage_modified_count());
}

TEST_P(QuotaBackendImplTest, ReserveQuota_NoSpace) {
  const url::Origin kOrigin = url::Origin::Create(GURL("http://example.com"));

  FileSystemType type = kFileSystemTypeTemporary;
  InitializeForOriginAndType(kOrigin, type);
  quota_manager_proxy_->set_quota(100);

  int64_t delta = 0;

  const int64_t kDelta = 1000;
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend_->ReserveQuota(
      kOrigin, type, kDelta,
      base::BindOnce(&DidReserveQuota, true, &error, &delta));
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(100, delta);
  EXPECT_EQ(100, quota_manager_proxy_->usage());

  EXPECT_EQ(1, quota_manager_proxy_->storage_modified_count());
}

TEST_P(QuotaBackendImplTest, ReserveQuota_Revert) {
  const url::Origin kOrigin = url::Origin::Create(GURL("http://example.com"));

  FileSystemType type = kFileSystemTypeTemporary;
  InitializeForOriginAndType(kOrigin, type);
  quota_manager_proxy_->set_quota(10000);

  int64_t delta = 0;

  const int64_t kDelta = 1000;
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend_->ReserveQuota(
      kOrigin, type, kDelta,
      base::BindOnce(&DidReserveQuota, false, &error, &delta));
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(kDelta, delta);
  EXPECT_EQ(0, quota_manager_proxy_->usage());

  EXPECT_EQ(2, quota_manager_proxy_->storage_modified_count());
}

TEST_P(QuotaBackendImplTest, ReleaseReservedQuota) {
  const url::Origin kOrigin = url::Origin::Create(GURL("http://example.com"));

  FileSystemType type = kFileSystemTypeTemporary;
  InitializeForOriginAndType(kOrigin, type);
  const int64_t kInitialUsage = 2000;
  quota_manager_proxy_->set_usage(kInitialUsage);
  quota_manager_proxy_->set_quota(10000);

  const int64_t kSize = 1000;
  backend_->ReleaseReservedQuota(kOrigin, type, kSize);
  EXPECT_EQ(kInitialUsage - kSize, quota_manager_proxy_->usage());

  EXPECT_EQ(1, quota_manager_proxy_->storage_modified_count());
}

TEST_P(QuotaBackendImplTest, CommitQuotaUsage) {
  const url::Origin kOrigin = url::Origin::Create(GURL("http://example.com"));

  FileSystemType type = kFileSystemTypeTemporary;
  InitializeForOriginAndType(kOrigin, type);
  quota_manager_proxy_->set_quota(10000);
  ASSERT_OK_AND_ASSIGN(base::FilePath path, GetUsageCachePath(kOrigin, type));

  const int64_t kDelta1 = 1000;
  backend_->CommitQuotaUsage(kOrigin, type, kDelta1);
  EXPECT_EQ(kDelta1, quota_manager_proxy_->usage());
  int64_t usage = 0;
  EXPECT_TRUE(file_system_usage_cache_.GetUsage(path, &usage));
  EXPECT_EQ(kDelta1, usage);

  const int64_t kDelta2 = -300;
  backend_->CommitQuotaUsage(kOrigin, type, kDelta2);
  EXPECT_EQ(kDelta1 + kDelta2, quota_manager_proxy_->usage());
  usage = 0;
  EXPECT_TRUE(file_system_usage_cache_.GetUsage(path, &usage));
  EXPECT_EQ(kDelta1 + kDelta2, usage);

  EXPECT_EQ(2, quota_manager_proxy_->storage_modified_count());
}

TEST_P(QuotaBackendImplTest, DirtyCount) {
  const url::Origin kOrigin = url::Origin::Create(GURL("http://example.com"));

  FileSystemType type = kFileSystemTypeTemporary;
  InitializeForOriginAndType(kOrigin, type);
  ASSERT_OK_AND_ASSIGN(base::FilePath path, GetUsageCachePath(kOrigin, type));

  backend_->IncrementDirtyCount(kOrigin, type);
  uint32_t dirty = 0;
  ASSERT_TRUE(file_system_usage_cache_.GetDirty(path, &dirty));
  EXPECT_EQ(1u, dirty);

  backend_->DecrementDirtyCount(kOrigin, type);
  ASSERT_TRUE(file_system_usage_cache_.GetDirty(path, &dirty));
  EXPECT_EQ(0u, dirty);
}

}  // namespace storage
