// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_quota_client.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/file_system/obfuscated_file_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {
namespace {

const char kDummyURL1[] = "http://www.dummy.org";
const char kDummyURL2[] = "http://www.example.com";
const char kDummyURL3[] = "http://www.bleh";

// Declared to shorten the variable names.
const StorageType kTemporary = StorageType::kTemporary;

}  // namespace

class FileSystemQuotaClientTest : public testing::Test {
 public:
  FileSystemQuotaClientTest()
      : special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}
  ~FileSystemQuotaClientTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    quota_manager_ = base::MakeRefCounted<MockQuotaManager>(
        /*is_incognito_=*/false, data_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    file_system_context_ = CreateFileSystemContextForTesting(
        quota_manager_proxy_, data_dir_.GetPath());
  }

  struct TestFile {
    bool isDirectory;
    const char* name;
    int64_t size;
    const char* origin_url;
    FileSystemType type;
  };

  storage::FileSystemContext* GetFileSystemContext() {
    return file_system_context_.get();
  }

  void GetBucketUsageAsync(storage::mojom::QuotaClient& quota_client,
                           const BucketLocator& bucket) {
    quota_client.GetBucketUsage(
        bucket, base::BindOnce(&FileSystemQuotaClientTest::OnGetUsage,
                               weak_factory_.GetWeakPtr()));
  }

  int64_t GetBucketUsage(storage::mojom::QuotaClient& quota_client,
                         const BucketLocator& bucket) {
    GetBucketUsageAsync(quota_client, bucket);
    base::RunLoop().RunUntilIdle();
    return usage_;
  }

  const std::vector<StorageKey>& GetStorageKeysForType(
      storage::mojom::QuotaClient& quota_client,
      StorageType type) {
    storage_keys_.clear();
    quota_client.GetStorageKeysForType(
        type, base::BindOnce(&FileSystemQuotaClientTest::OnGetStorageKeys,
                             weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();
    return storage_keys_;
  }

  void RunAdditionalBucketUsageTask(storage::mojom::QuotaClient& quota_client,
                                    const BucketLocator& bucket) {
    quota_client.GetBucketUsage(
        bucket, base::BindOnce(&FileSystemQuotaClientTest::OnGetAdditionalUsage,
                               weak_factory_.GetWeakPtr()));
  }

  bool CreateFileSystemDirectory(const base::FilePath& file_path,
                                 const std::string& origin_url,
                                 FileSystemType type) {
    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting(origin_url), type,
        file_path);

    base::File::Error result =
        AsyncFileTestHelper::CreateDirectory(file_system_context_.get(), url);
    return result == base::File::FILE_OK;
  }

  bool CreateFileSystemFile(const base::FilePath& file_path,
                            int64_t file_size,
                            const std::string& origin_url,
                            FileSystemType type) {
    if (file_path.empty()) {
      return false;
    }

    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting(origin_url), type,
        file_path);

    base::File::Error result =
        AsyncFileTestHelper::CreateFile(file_system_context_.get(), url);
    if (result != base::File::FILE_OK) {
      return false;
    }

    result = AsyncFileTestHelper::TruncateFile(file_system_context_.get(), url,
                                               file_size);
    return result == base::File::FILE_OK;
  }

  void InitializeOriginFiles(storage::mojom::QuotaClient& quota_client,
                             const std::vector<TestFile>& files) {
    for (const TestFile& file : files) {
      base::FilePath path = base::FilePath().AppendASCII(file.name);
      if (file.isDirectory) {
        ASSERT_OK_AND_ASSIGN(
            auto bucket,
            GetOrCreateBucket(file.origin_url, kDefaultBucketName,
                              FileSystemTypeToQuotaStorageType(file.type)));
        quota_manager_->SetQuota(bucket.storage_key, bucket.type,
                                 1024 * 1024 * 100);
        ASSERT_TRUE(
            CreateFileSystemDirectory(path, file.origin_url, file.type));
        if (path.empty()) {
          // Create the usage cache.
          // HACK--we always create the root [an empty path] first.  If we
          // create it later, this will fail due to a quota mismatch.  If we
          // call this before we create the root, it succeeds, but hasn't
          // actually created the cache.
          GetBucketUsage(quota_client, bucket);
        }
      } else {
        ASSERT_TRUE(
            CreateFileSystemFile(path, file.size, file.origin_url, file.type));
      }
    }
  }

  // This is a bit fragile--it depends on the test data always creating a
  // directory before adding a file or directory to it, so that we can just
  // count the basename of each addition.  A recursive creation of a path, which
  // created more than one directory in a single shot, would break this.
  int64_t ComputeFilePathsCostForOriginAndType(
      const base::span<const TestFile> files,
      const std::string& origin_url,
      FileSystemType type) {
    int64_t file_paths_cost = 0;
    for (const TestFile& file : files) {
      if (file.type == type && GURL(file.origin_url) == GURL(origin_url)) {
        base::FilePath path = base::FilePath().AppendASCII(file.name);
        if (!path.empty()) {
          file_paths_cost += ObfuscatedFileUtil::ComputeFilePathCost(path);
        }
      }
    }
    return file_paths_cost;
  }

  void DeleteBucketData(FileSystemQuotaClient* quota_client,
                        const BucketLocator& bucket) {
    base::test::TestFuture<blink::mojom::QuotaStatusCode> future;
    quota_client->DeleteBucketData(bucket, future.GetCallback());
    ASSERT_EQ(future.Get(), blink::mojom::QuotaStatusCode::kOk);
  }

  storage::QuotaErrorOr<BucketLocator> GetOrCreateBucket(
      const std::string& origin,
      const std::string& name,
      StorageType type) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    quota_manager_->GetOrCreateBucketDeprecated(
        {blink::StorageKey::CreateFromStringForTesting(origin), name}, type,
        future.GetCallback());
    return future.Take().transform(&storage::BucketInfo::ToBucketLocator);
  }

  storage::QuotaErrorOr<BucketLocator> GetBucket(const std::string& origin,
                                                 const std::string& name,
                                                 StorageType type) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    quota_manager_->GetBucketByNameUnsafe(
        blink::StorageKey::CreateFromStringForTesting(origin), name, type,
        future.GetCallback());
    return future.Take().transform(&storage::BucketInfo::ToBucketLocator);
  }

  int64_t usage() const { return usage_; }
  int additional_callback_count() const { return additional_callback_count_; }
  void set_additional_callback_count(int count) {
    additional_callback_count_ = count;
  }

 private:
  void OnGetUsage(int64_t usage) { usage_ = usage; }

  void OnGetStorageKeys(const std::vector<StorageKey>& storage_keys) {
    storage_keys_ = storage_keys;
  }

  void OnGetAdditionalUsage(int64_t usage_unused) {
    ++additional_callback_count_;
  }

 protected:
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;

  base::ScopedTempDir data_dir_;
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;

  int64_t usage_ = 0;
  int additional_callback_count_ = 0;
  std::vector<StorageKey> storage_keys_;
  base::WeakPtrFactory<FileSystemQuotaClientTest> weak_factory_{this};
};

TEST_F(FileSystemQuotaClientTest, NoFileSystemTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());

  ASSERT_OK_AND_ASSIGN(
      auto bucket,
      GetOrCreateBucket(kDummyURL1, kDefaultBucketName, kTemporary));
  EXPECT_EQ(0, GetBucketUsage(quota_client, bucket));
}

TEST_F(FileSystemQuotaClientTest, NoFileTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());

  InitializeOriginFiles(quota_client,
                        {{true, "", 0, kDummyURL1, kFileSystemTypeTemporary}});

  ASSERT_OK_AND_ASSIGN(
      auto bucket,
      GetOrCreateBucket(kDummyURL1, kDefaultBucketName, kTemporary));
  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(0, GetBucketUsage(quota_client, bucket));
  }
}

TEST_F(FileSystemQuotaClientTest, NonDefaultBucket) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  ASSERT_OK_AND_ASSIGN(
      auto bucket, GetOrCreateBucket(kDummyURL1, "logs_bucket", kTemporary));
  ASSERT_FALSE(bucket.is_default);
  EXPECT_EQ(0, GetBucketUsage(quota_client, bucket));
  DeleteBucketData(&quota_client, bucket);
}

TEST_F(FileSystemQuotaClientTest, OneFileTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());

  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "foo", 4921, kDummyURL1, kFileSystemTypeTemporary},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, kDummyURL1, kFileSystemTypeTemporary);

  ASSERT_OK_AND_ASSIGN(auto bucket,
                       GetBucket(kDummyURL1, kDefaultBucketName, kTemporary));
  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(4921 + file_paths_cost, GetBucketUsage(quota_client, bucket));
  }
}

TEST_F(FileSystemQuotaClientTest, TwoFilesTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "foo", 10310, kDummyURL1, kFileSystemTypeTemporary},
      {false, "bar", 41, kDummyURL1, kFileSystemTypeTemporary},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, kDummyURL1, kFileSystemTypeTemporary);

  ASSERT_OK_AND_ASSIGN(auto bucket,
                       GetBucket(kDummyURL1, kDefaultBucketName, kTemporary));
  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(10310 + 41 + file_paths_cost,
              GetBucketUsage(quota_client, bucket));
  }
}

TEST_F(FileSystemQuotaClientTest, EmptyFilesTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "foo", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "bar", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "baz", 0, kDummyURL1, kFileSystemTypeTemporary},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, kDummyURL1, kFileSystemTypeTemporary);

  ASSERT_OK_AND_ASSIGN(auto bucket,
                       GetBucket(kDummyURL1, kDefaultBucketName, kTemporary));
  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(file_paths_cost, GetBucketUsage(quota_client, bucket));
  }
}

TEST_F(FileSystemQuotaClientTest, SubDirectoryTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {true, "dirtest", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "dirtest/foo", 11921, kDummyURL1, kFileSystemTypeTemporary},
      {false, "bar", 4814, kDummyURL1, kFileSystemTypeTemporary},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, kDummyURL1, kFileSystemTypeTemporary);

  ASSERT_OK_AND_ASSIGN(auto bucket,
                       GetBucket(kDummyURL1, kDefaultBucketName, kTemporary));
  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(11921 + 4814 + file_paths_cost,
              GetBucketUsage(quota_client, bucket));
  }
}

TEST_F(FileSystemQuotaClientTest, MultiTypeTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {true, "dirtest", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "dirtest/foo", 133, kDummyURL1, kFileSystemTypeTemporary},
      {false, "bar", 14, kDummyURL1, kFileSystemTypeTemporary},
      {true, "", 0, kDummyURL1, kFileSystemTypePersistent},
      {true, "dirtest", 0, kDummyURL1, kFileSystemTypePersistent},
      {false, "dirtest/foo", 193, kDummyURL1, kFileSystemTypePersistent},
      {false, "bar", 9, kDummyURL1, kFileSystemTypePersistent},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost_temporary =
      ComputeFilePathsCostForOriginAndType(kFiles, kDummyURL1,
                                           kFileSystemTypeTemporary);
  const int64_t file_paths_cost_persistent =
      ComputeFilePathsCostForOriginAndType(kFiles, kDummyURL1,
                                           kFileSystemTypePersistent);

  for (int i = 0; i < 2; i++) {
    ASSERT_OK_AND_ASSIGN(auto bucket,
                         GetBucket(kDummyURL1, kDefaultBucketName, kTemporary));
    EXPECT_EQ(133 + 14 + file_paths_cost_temporary + 193 + 9 +
                  file_paths_cost_persistent,
              GetBucketUsage(quota_client, bucket));
  }
}

TEST_F(FileSystemQuotaClientTest, MultiDomainTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {true, "dir1", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "dir1/foo", 1331, kDummyURL1, kFileSystemTypeTemporary},
      {false, "bar", 134, kDummyURL1, kFileSystemTypeTemporary},
      {true, "", 0, kDummyURL1, kFileSystemTypePersistent},
      {true, "dir2", 0, kDummyURL1, kFileSystemTypePersistent},
      {false, "dir2/foo", 1903, kDummyURL1, kFileSystemTypePersistent},
      {false, "bar", 19, kDummyURL1, kFileSystemTypePersistent},
      {true, "", 0, kDummyURL2, kFileSystemTypeTemporary},
      {true, "dom", 0, kDummyURL2, kFileSystemTypeTemporary},
      {false, "dom/fan", 1319, kDummyURL2, kFileSystemTypeTemporary},
      {false, "bar", 113, kDummyURL2, kFileSystemTypeTemporary},
      {true, "", 0, kDummyURL2, kFileSystemTypePersistent},
      {true, "dom", 0, kDummyURL2, kFileSystemTypePersistent},
      {false, "dom/fan", 2013, kDummyURL2, kFileSystemTypePersistent},
      {false, "baz", 18, kDummyURL2, kFileSystemTypePersistent},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost_temporary1 =
      ComputeFilePathsCostForOriginAndType(kFiles, kDummyURL1,
                                           kFileSystemTypeTemporary);
  const int64_t file_paths_cost_persistent1 =
      ComputeFilePathsCostForOriginAndType(kFiles, kDummyURL1,
                                           kFileSystemTypePersistent);
  const int64_t file_paths_cost_temporary2 =
      ComputeFilePathsCostForOriginAndType(kFiles, kDummyURL2,
                                           kFileSystemTypeTemporary);
  const int64_t file_paths_cost_persistent2 =
      ComputeFilePathsCostForOriginAndType(kFiles, kDummyURL2,
                                           kFileSystemTypePersistent);

  for (int i = 0; i < 2; i++) {
    ASSERT_OK_AND_ASSIGN(auto bucket1,
                         GetBucket(kDummyURL1, kDefaultBucketName, kTemporary));
    ASSERT_OK_AND_ASSIGN(auto bucket2,
                         GetBucket(kDummyURL2, kDefaultBucketName, kTemporary));
    EXPECT_EQ(1331 + 134 + file_paths_cost_temporary1 + 1903 + 19 +
                  file_paths_cost_persistent1,
              GetBucketUsage(quota_client, bucket1));
    EXPECT_EQ(1319 + 113 + file_paths_cost_temporary2 + 2013 + 18 +
                  file_paths_cost_persistent2,
              GetBucketUsage(quota_client, bucket2));
  }
}

TEST_F(FileSystemQuotaClientTest, GetUsage_MultipleTasks) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "foo", 11, kDummyURL1, kFileSystemTypeTemporary},
      {false, "bar", 22, kDummyURL1, kFileSystemTypeTemporary},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, kDummyURL1, kFileSystemTypeTemporary);

  // Dispatching three GetUsage tasks.
  set_additional_callback_count(0);
  ASSERT_OK_AND_ASSIGN(auto bucket,
                       GetBucket(kDummyURL1, kDefaultBucketName, kTemporary));
  GetBucketUsageAsync(quota_client, bucket);
  RunAdditionalBucketUsageTask(quota_client, bucket);
  RunAdditionalBucketUsageTask(quota_client, bucket);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(11 + 22 + file_paths_cost, usage());
  EXPECT_EQ(2, additional_callback_count());

  // Once more, in a different order.
  set_additional_callback_count(0);
  RunAdditionalBucketUsageTask(quota_client, bucket);
  GetBucketUsageAsync(quota_client, bucket);
  RunAdditionalBucketUsageTask(quota_client, bucket);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(11 + 22 + file_paths_cost, usage());
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(FileSystemQuotaClientTest, GetStorageKeysForType) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  InitializeOriginFiles(
      quota_client, {
                        {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
                        {true, "", 0, kDummyURL2, kFileSystemTypeTemporary},
                        {true, "", 0, kDummyURL3, kFileSystemTypePersistent},
                    });

  EXPECT_THAT(GetStorageKeysForType(quota_client, kTemporary),
              testing::UnorderedElementsAre(
                  StorageKey::CreateFromStringForTesting(kDummyURL1),
                  StorageKey::CreateFromStringForTesting(kDummyURL2),
                  StorageKey::CreateFromStringForTesting(kDummyURL3)));
}

TEST_F(FileSystemQuotaClientTest, DeleteOriginTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, "http://foo.com/", kFileSystemTypeTemporary},
      {false, "a", 1, "http://foo.com/", kFileSystemTypeTemporary},
      {true, "", 0, "https://foo.com/", kFileSystemTypeTemporary},
      {false, "b", 2, "https://foo.com/", kFileSystemTypeTemporary},
      {true, "", 0, "http://foo.com/", kFileSystemTypePersistent},
      {false, "c", 4, "http://foo.com/", kFileSystemTypePersistent},
      {true, "", 0, "http://bar.com/", kFileSystemTypeTemporary},
      {false, "d", 8, "http://bar.com/", kFileSystemTypeTemporary},
      {true, "", 0, "http://bar.com/", kFileSystemTypePersistent},
      {false, "e", 16, "http://bar.com/", kFileSystemTypePersistent},
      {true, "", 0, "https://bar.com/", kFileSystemTypePersistent},
      {false, "f", 32, "https://bar.com/", kFileSystemTypePersistent},
      {true, "", 0, "https://bar.com/", kFileSystemTypeTemporary},
      {false, "g", 64, "https://bar.com/", kFileSystemTypeTemporary},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost_temporary_foo_https =
      ComputeFilePathsCostForOriginAndType(kFiles, "https://foo.com/",
                                           kFileSystemTypeTemporary);
  const int64_t file_paths_cost_temporary_bar =
      ComputeFilePathsCostForOriginAndType(kFiles, "http://bar.com/",
                                           kFileSystemTypeTemporary);
  const int64_t file_paths_cost_persistent_bar =
      ComputeFilePathsCostForOriginAndType(kFiles, "http://bar.com/",
                                           kFileSystemTypePersistent);
  const int64_t file_paths_cost_temporary_bar_https =
      ComputeFilePathsCostForOriginAndType(kFiles, "https://bar.com/",
                                           kFileSystemTypeTemporary);
  const int64_t file_paths_cost_persistent_bar_https =
      ComputeFilePathsCostForOriginAndType(kFiles, "https://bar.com/",
                                           kFileSystemTypePersistent);

  ASSERT_OK_AND_ASSIGN(
      auto foo_temp_bucket,
      GetBucket("http://foo.com/", kDefaultBucketName, kTemporary));
  DeleteBucketData(&quota_client, foo_temp_bucket);

  ASSERT_OK_AND_ASSIGN(
      auto buz_temp_bucket,
      GetOrCreateBucket("http://buz.com/", kDefaultBucketName, kTemporary));
  DeleteBucketData(&quota_client, buz_temp_bucket);

  EXPECT_EQ(0, GetBucketUsage(quota_client, foo_temp_bucket));
  EXPECT_EQ(0, GetBucketUsage(quota_client, buz_temp_bucket));

  ASSERT_OK_AND_ASSIGN(
      auto foo_https_temp_bucket,
      GetBucket("https://foo.com/", kDefaultBucketName, kTemporary));
  EXPECT_EQ(2 + file_paths_cost_temporary_foo_https,
            GetBucketUsage(quota_client, foo_https_temp_bucket));

  ASSERT_OK_AND_ASSIGN(
      auto bar_temp_bucket,
      GetBucket("http://bar.com/", kDefaultBucketName, kTemporary));
  EXPECT_EQ(
      8 + file_paths_cost_temporary_bar + 16 + file_paths_cost_persistent_bar,
      GetBucketUsage(quota_client, bar_temp_bucket));

  ASSERT_OK_AND_ASSIGN(
      auto bar_https_temp_bucket,
      GetBucket("https://bar.com/", kDefaultBucketName, kTemporary));
  EXPECT_EQ(64 + file_paths_cost_temporary_bar_https + 32 +
                file_paths_cost_persistent_bar_https,
            GetBucketUsage(quota_client, bar_https_temp_bucket));
}

}  // namespace storage
