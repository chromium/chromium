// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/browser/file_system/sandbox_file_system_backend.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/cpp/constants.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_features.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/obfuscated_file_util.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_options.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "url/gurl.h"

// PS stands for path separator.
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
#define PS "\\"
#else
#define PS "/"
#endif

namespace storage {

namespace {

const struct RootPathTest {
  FileSystemType type;
  const char* origin_url;
  const char* expected_path;
} kRootPathTestCases[] = {
    {kFileSystemTypeTemporary, "http://foo:1/", "000" PS "t"},
    {kFileSystemTypePersistent, "http://foo:1/", "000" PS "p"},
    {kFileSystemTypeTemporary, "http://bar.com/", "001" PS "t"},
    {kFileSystemTypePersistent, "http://bar.com/", "001" PS "p"},
    {kFileSystemTypeTemporary, "https://foo:2/", "002" PS "t"},
    {kFileSystemTypePersistent, "https://foo:2/", "002" PS "p"},
    {kFileSystemTypeTemporary, "https://bar.com/", "003" PS "t"},
    {kFileSystemTypePersistent, "https://bar.com/", "003" PS "p"},
};

const struct RootPathFileURITest {
  FileSystemType type;
  const char* origin_url;
  const char* expected_path;
} kRootPathFileURITestCases[] = {
    {kFileSystemTypeTemporary, "file:///", "000" PS "t"},
    {kFileSystemTypePersistent, "file:///", "000" PS "p"}};

const struct RootPathFileURINonDefaulBucketTest {
  FileSystemType type;
  const char* origin_url;
  const char* expected_path;
} kRootPathFileURIAndBucketTestCases[] = {
    {kFileSystemTypeTemporary, "file:///", "1" PS "FileSystem" PS "t"},
    {kFileSystemTypePersistent, "file:///", "1" PS "FileSystem" PS "p"}};

void DidOpenFileSystem(base::File::Error* error_out,
                       const GURL& origin_url,
                       const std::string& name,
                       base::File::Error error) {
  *error_out = error;
}

}  // namespace

class SandboxFileSystemBackendTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    SetUpToCreateBuckets();
    SetUpNewDelegate(CreateAllowFileAccessOptions());
  }

  void SetUpNewDelegate(const FileSystemOptions& options) {
    incognito_env_override_ = leveldb_chrome::NewMemEnv("FileSystem");
    delegate_ = std::make_unique<SandboxFileSystemBackendDelegate>(
        quota_manager_->proxy(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), data_dir_.GetPath(),
        /*special_storage_policy=*/nullptr, options,
        options.is_in_memory() ? incognito_env_override_.get() : nullptr);
  }

  void SetUpToCreateBuckets() {
    scoped_refptr<MockSpecialStoragePolicy> storage_policy =
        base::MakeRefCounted<MockSpecialStoragePolicy>();

    scoped_refptr<base::SingleThreadTaskRunner> quota_manager_task_runner =
        base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()});

    quota_manager_ = base::MakeRefCounted<QuotaManager>(
        /*is_incognito=*/true, data_dir_.GetPath(), quota_manager_task_runner,
        /*quota_change_callback=*/base::DoNothing(), storage_policy,
        GetQuotaSettingsFunc());

    quota_manager_task_runner->PostTask(
        FROM_HERE, base::BindOnce(
                       [](const scoped_refptr<QuotaManager>& quota_manager) {
                         QuotaSettings settings;
                         settings.per_storage_key_quota = 25 * 1024 * 1024;
                         settings.pool_size =
                             settings.per_storage_key_quota * 5;
                         settings.must_remain_available = 10 * 1024 * 1024;
                         settings.refresh_interval = base::TimeDelta::Max();
                         quota_manager->SetQuotaSettings(settings);
                       },
                       quota_manager_));
  }

  BucketLocator CreateNonDefaultBucket(const blink::StorageKey& storage_key) {
    // Create the non-default bucket member corresponding to the StorageKey
    // member we created.
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> custom_future;
    BucketInitParams params = BucketInitParams::ForDefaultBucket(storage_key);
    params.name = "non-default bucket";
    quota_manager_->proxy()->UpdateOrCreateBucket(
        params, base::SequencedTaskRunner::GetCurrentDefault(),
        custom_future.GetCallback());
    QuotaErrorOr<BucketInfo> custom_bucket = custom_future.Take();
    CHECK(custom_bucket.has_value());
    return custom_bucket.value().ToBucketLocator();
  }

  void SetUpNewBackend(const FileSystemOptions& options) {
    SetUpNewDelegate(options);
    backend_ = std::make_unique<SandboxFileSystemBackend>(delegate_.get());
  }

  SandboxFileSystemBackendDelegate::StorageKeyEnumerator*
  CreateStorageKeyEnumerator() const {
    return backend_->CreateStorageKeyEnumerator();
  }

  void CreateOriginTypeDirectory(const char* origin_url, FileSystemType type) {
    base::FilePath target = delegate_->GetBaseDirectoryForStorageKeyAndType(
        blink::StorageKey::CreateFromStringForTesting(origin_url), type, true);
    ASSERT_TRUE(!target.empty());
    ASSERT_TRUE(base::DirectoryExists(target));
  }

  bool GetRootPath(const char* origin_url,
                   FileSystemType type,
                   const BucketLocator* bucket_locator,
                   OpenFileSystemMode mode,
                   base::FilePath* root_path) {
    base::File::Error error = base::File::FILE_OK;
    FileSystemURL test_url = FileSystemURL::CreateForTest(
        blink::StorageKey::CreateFromStringForTesting(origin_url), type,
        base::FilePath());
    // TODO(crbug.com/40227222):
    // SandboxFileSystemBackendDelegate::OpenFileSystem() needs to be refactored
    // to take bucket information into account. Remove this if statement once
    // this refactor is complete - the ResolveURL() call should setup the
    // FileSystem correctly for both buckets and non-buckets paths.
    if (bucket_locator)
      test_url.SetBucket(*bucket_locator);
    else
      backend_->ResolveURL(test_url, mode,
                           base::BindOnce(&DidOpenFileSystem, &error));
    base::RunLoop().RunUntilIdle();
    if (error != base::File::FILE_OK)
      return false;
    base::FilePath returned_root_path;
    if (bucket_locator)
      returned_root_path = delegate_->GetBaseDirectoryForBucketAndType(
          *bucket_locator, type, /*create=*/true);
    else
      returned_root_path = delegate_->GetBaseDirectoryForStorageKeyAndType(
          blink::StorageKey::CreateFromStringForTesting(origin_url), type,
          /*create=*/false);
    if (root_path)
      *root_path = returned_root_path;
    return !returned_root_path.empty();
  }

  base::FilePath file_system_path() const {
    return data_dir_.GetPath().Append(ObfuscatedFileUtil::kFileSystemDirectory);
  }

  base::FilePath file_system_path_for_buckets() const {
    return data_dir_.GetPath().Append(kWebStorageDirectory);
  }

  std::unique_ptr<leveldb::Env> incognito_env_override_;
  base::ScopedTempDir data_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SandboxFileSystemBackendDelegate> delegate_;
  std::unique_ptr<SandboxFileSystemBackend> backend_;
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<QuotaManager> quota_manager_;
};

TEST_F(SandboxFileSystemBackendTest, Empty) {
  SetUpNewBackend(CreateAllowFileAccessOptions());
  std::unique_ptr<SandboxFileSystemBackendDelegate::StorageKeyEnumerator>
      enumerator(CreateStorageKeyEnumerator());
  ASSERT_FALSE(enumerator->Next());
}

TEST_F(SandboxFileSystemBackendTest, EnumerateOrigins) {
  SetUpNewBackend(CreateAllowFileAccessOptions());
  const char* temporary_origins[] = {
      "http://www.bar.com/",       "http://www.foo.com/",
      "http://www.foo.com:1/",     "http://www.example.com:8080/",
      "http://www.google.com:80/",
  };
  const char* persistent_origins[] = {
      "http://www.bar.com/",
      "http://www.foo.com:8080/",
      "http://www.foo.com:80/",
  };
  size_t temporary_size = std::size(temporary_origins);
  size_t persistent_size = std::size(persistent_origins);
  std::set<blink::StorageKey> temporary_set, persistent_set;
  for (size_t i = 0; i < temporary_size; ++i) {
    CreateOriginTypeDirectory(temporary_origins[i], kFileSystemTypeTemporary);
    temporary_set.insert(
        blink::StorageKey::CreateFromStringForTesting(temporary_origins[i]));
  }
  for (size_t i = 0; i < persistent_size; ++i) {
    CreateOriginTypeDirectory(persistent_origins[i], kFileSystemTypePersistent);
    persistent_set.insert(
        blink::StorageKey::CreateFromStringForTesting(persistent_origins[i]));
  }

  std::unique_ptr<SandboxFileSystemBackendDelegate::StorageKeyEnumerator>
      enumerator(CreateStorageKeyEnumerator());
  size_t temporary_actual_size = 0;
  size_t persistent_actual_size = 0;

  std::optional<blink::StorageKey> current;
  while ((current = enumerator->Next()).has_value()) {
    SCOPED_TRACE(testing::Message()
                 << "EnumerateOrigin " << current->origin().Serialize());
    if (enumerator->HasFileSystemType(kFileSystemTypeTemporary)) {
      EXPECT_TRUE(base::Contains(temporary_set, current.value()));
      ++temporary_actual_size;
    }
    if (enumerator->HasFileSystemType(kFileSystemTypePersistent)) {
      EXPECT_TRUE(base::Contains(persistent_set, current.value()));
      ++persistent_actual_size;
    }
  }

  EXPECT_EQ(temporary_size, temporary_actual_size);
  EXPECT_EQ(persistent_size, persistent_actual_size);
}

TEST_F(SandboxFileSystemBackendTest, GetRootPathCreateAndExamine) {
  std::vector<base::FilePath> returned_root_path(std::size(kRootPathTestCases));
  SetUpNewBackend(CreateAllowFileAccessOptions());

  // Create a new root directory.
  for (size_t i = 0; i < std::size(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (create) #" << i << " "
                                    << kRootPathTestCases[i].expected_path);

    base::FilePath root_path;
    EXPECT_TRUE(
        GetRootPath(kRootPathTestCases[i].origin_url,
                    kRootPathTestCases[i].type, /*bucket_locator=*/nullptr,
                    OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, &root_path));

    base::FilePath expected =
        file_system_path().AppendASCII(kRootPathTestCases[i].expected_path);
    EXPECT_EQ(expected.value(), root_path.value());
    EXPECT_TRUE(base::DirectoryExists(root_path));
    ASSERT_TRUE(returned_root_path.size() > i);
    returned_root_path[i] = root_path;
  }

  // Get the root directory with create=false and see if we get the
  // same directory.
  for (size_t i = 0; i < std::size(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (get) #" << i << " "
                                    << kRootPathTestCases[i].expected_path);

    base::FilePath root_path;
    EXPECT_TRUE(GetRootPath(kRootPathTestCases[i].origin_url,
                            kRootPathTestCases[i].type,
                            /*bucket_locator=*/nullptr,
                            OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT, &root_path));
    ASSERT_TRUE(returned_root_path.size() > i);
    EXPECT_EQ(returned_root_path[i].value(), root_path.value());
  }
}

TEST_F(SandboxFileSystemBackendTest,
       GetRootPathCreateAndExamineWithNewBackend) {
  std::vector<base::FilePath> returned_root_path(std::size(kRootPathTestCases));
  SetUpNewBackend(CreateAllowFileAccessOptions());

  base::FilePath root_path1;
  EXPECT_TRUE(GetRootPath("http://foo.com:1/", kFileSystemTypeTemporary,
                          /*bucket_locator=*/nullptr,
                          OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, &root_path1));

  SetUpNewBackend(CreateDisallowFileAccessOptions());
  base::FilePath root_path2;
  EXPECT_TRUE(GetRootPath("http://foo.com:1/", kFileSystemTypeTemporary,
                          /*bucket_locator=*/nullptr,
                          OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT, &root_path2));

  EXPECT_EQ(root_path1.value(), root_path2.value());
}

TEST_F(SandboxFileSystemBackendTest, GetRootPathGetWithoutCreate) {
  SetUpNewBackend(CreateDisallowFileAccessOptions());

  // Try to get a root directory without creating.
  for (size_t i = 0; i < std::size(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (create=false) #" << i << " "
                                    << kRootPathTestCases[i].expected_path);
    EXPECT_FALSE(GetRootPath(kRootPathTestCases[i].origin_url,
                             kRootPathTestCases[i].type,
                             /*bucket_locator=*/nullptr,
                             OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT, nullptr));
  }
}

TEST_F(SandboxFileSystemBackendTest, GetRootPathInIncognito) {
  SetUpNewBackend(CreateIncognitoFileSystemOptions());

  // Try to get a root directory.
  for (size_t i = 0; i < std::size(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (incognito) #" << i << " "
                                    << kRootPathTestCases[i].expected_path);
    EXPECT_TRUE(GetRootPath(kRootPathTestCases[i].origin_url,
                            kRootPathTestCases[i].type,
                            /*bucket_locator=*/nullptr,
                            OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, nullptr));
  }
}

TEST_F(SandboxFileSystemBackendTest, GetRootPathFileURI) {
  SetUpNewBackend(CreateDisallowFileAccessOptions());
  for (size_t i = 0; i < std::size(kRootPathFileURITestCases); ++i) {
    SCOPED_TRACE(testing::Message()
                 << "RootPathFileURI (disallow) #" << i << " "
                 << kRootPathFileURITestCases[i].expected_path);
    EXPECT_FALSE(GetRootPath(kRootPathFileURITestCases[i].origin_url,
                             kRootPathFileURITestCases[i].type,
                             /*bucket_locator=*/nullptr,
                             OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, nullptr));
  }
}

TEST_F(SandboxFileSystemBackendTest, GetRootPathFileURIWithAllowFlag) {
  SetUpNewBackend(CreateAllowFileAccessOptions());
  for (size_t i = 0; i < std::size(kRootPathFileURITestCases); ++i) {
    SCOPED_TRACE(testing::Message()
                 << "RootPathFileURI (allow) #" << i << " "
                 << kRootPathFileURITestCases[i].expected_path);
    base::FilePath root_path;
    EXPECT_TRUE(GetRootPath(kRootPathFileURITestCases[i].origin_url,
                            kRootPathFileURITestCases[i].type,
                            /*bucket_locator=*/nullptr,
                            OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                            &root_path));
    base::FilePath expected = file_system_path().AppendASCII(
        kRootPathFileURITestCases[i].expected_path);
    EXPECT_EQ(expected.value(), root_path.value());
    EXPECT_TRUE(base::DirectoryExists(root_path));
  }
}

TEST_F(SandboxFileSystemBackendTest, GetRootPathFileURIWithAllowFlagAndBucket) {
  SetUpNewBackend(CreateAllowFileAccessOptions());
  for (size_t i = 0; i < std::size(kRootPathFileURIAndBucketTestCases); ++i) {
    SCOPED_TRACE(testing::Message()
                 << "RootPathFileURIAndBucket (allow) #" << i << " "
                 << kRootPathFileURIAndBucketTestCases[i].expected_path);
    BucketLocator bucket_locator =
        CreateNonDefaultBucket(blink::StorageKey::CreateFromStringForTesting(
            kRootPathFileURIAndBucketTestCases[i].origin_url));
    base::FilePath root_path;
    EXPECT_TRUE(
        GetRootPath(kRootPathFileURIAndBucketTestCases[i].origin_url,
                    kRootPathFileURIAndBucketTestCases[i].type, &bucket_locator,
                    OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, &root_path));
    base::FilePath expected = file_system_path_for_buckets().AppendASCII(
        kRootPathFileURIAndBucketTestCases[i].expected_path);
    EXPECT_EQ(expected.value(), root_path.value());
    EXPECT_TRUE(base::DirectoryExists(root_path));
  }
}

}  // namespace storage
