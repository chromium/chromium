// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/browser/file_system/obfuscated_file_util.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "storage/browser/file_system/sandbox_directory_database.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "storage/browser/file_system/sandbox_origin_database.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/file_system_test_file_set.h"
#include "storage/browser/test/mock_file_change_observer.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/sandbox_file_system_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/database/database_identifier.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "url/gurl.h"

using url::Origin;

namespace storage {

namespace {

enum TestMode {
  kRegularFirstParty,
  kRegularFirstPartyNonDefaultBucket,
  kRegularThirdParty,
  kRegularThirdPartyNonDefaultBucket,
  kIncognitoFirstParty,
  kIncognitoFirstPartyNonDefaultBucket,
  kIncognitoThirdParty,
  kIncognitoThirdPartyNonDefaultBucket
};

bool FileExists(const base::FilePath& path) {
  return base::PathExists(path) && !base::DirectoryExists(path);
}

int64_t GetLocalFileSize(const base::FilePath& path) {
  int64_t size;
  EXPECT_TRUE(base::GetFileSize(path, &size));
  return size;
}

// After a move, the dest exists and the source doesn't.
// After a copy, both source and dest exist.
struct CopyMoveTestCaseRecord {
  bool is_copy_not_move;
  const char source_path[64];
  const char dest_path[64];
  bool cause_overwrite;
};

const CopyMoveTestCaseRecord kCopyMoveTestCases[] = {
    // This is the combinatoric set of:
    //  rename vs. same-name
    //  different directory vs. same directory
    //  overwrite vs. no-overwrite
    //  copy vs. move
    //  We can never be called with source and destination paths identical, so
    //  those cases are omitted.
    {true, "dir0/file0", "dir0/file1", false},
    {false, "dir0/file0", "dir0/file1", false},
    {true, "dir0/file0", "dir0/file1", true},
    {false, "dir0/file0", "dir0/file1", true},

    {true, "dir0/file0", "dir1/file0", false},
    {false, "dir0/file0", "dir1/file0", false},
    {true, "dir0/file0", "dir1/file0", true},
    {false, "dir0/file0", "dir1/file0", true},
    {true, "dir0/file0", "dir1/file1", false},
    {false, "dir0/file0", "dir1/file1", false},
    {true, "dir0/file0", "dir1/file1", true},
    {false, "dir0/file0", "dir1/file1", true},
};

struct OriginEnumerationTestRecord {
  std::string origin_url;
  bool has_temporary;
  bool has_persistent;
};

const OriginEnumerationTestRecord kOriginEnumerationTestRecords[] = {
    {"http://example.com/", false, true},
    {"http://example1.com/", true, false},
    {"https://example1.com/", true, true},
    {"file:///", false, true},
    {"http://example.com:8000/", false, true},
};

FileSystemURL FileSystemURLAppend(const FileSystemURL& url,
                                  const base::FilePath::StringType& child) {
  FileSystemURL new_url = FileSystemURL::CreateForTest(
      url.storage_key(), url.mount_type(), url.virtual_path().Append(child));
  if (url.bucket().has_value())
    new_url.SetBucket(url.bucket().value());
  return new_url;
}

FileSystemURL FileSystemURLAppendUTF8(const FileSystemURL& url,
                                      const std::string& child) {
  FileSystemURL new_url = FileSystemURL::CreateForTest(
      url.storage_key(), url.mount_type(),
      url.virtual_path().Append(base::FilePath::FromUTF8Unsafe(child)));
  if (url.bucket().has_value())
    new_url.SetBucket(url.bucket().value());
  return new_url;
}

FileSystemURL FileSystemURLDirName(const FileSystemURL& url) {
  FileSystemURL new_url =
      FileSystemURL::CreateForTest(url.storage_key(), url.mount_type(),
                                   VirtualPath::DirName(url.virtual_path()));
  if (url.bucket().has_value())
    new_url.SetBucket(url.bucket().value());
  return new_url;
}

std::string GetTypeString(FileSystemType type) {
  return SandboxFileSystemBackendDelegate::GetTypeString(type);
}

bool HasFileSystemType(
    ObfuscatedFileUtil::AbstractStorageKeyEnumerator* enumerator,
    FileSystemType type) {
  return enumerator->HasTypeDirectory(GetTypeString(type));
}

}  // namespace

// TODO(ericu): The vast majority of this and the other FSFU subclass tests
// could theoretically be shared.  It would basically be a FSFU interface
// compliance test, and only the subclass-specific bits that look into the
// implementation would need to be written per-subclass.
class ObfuscatedFileUtilTest : public testing::Test,
                               public ::testing::WithParamInterface<TestMode> {
 public:
  ObfuscatedFileUtilTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        storage_key_(blink::StorageKey::CreateFromStringForTesting(
            "http://www.example.com")),
        type_(kFileSystemTypeTemporary),
        sandbox_file_system_(storage_key_, type_),
        quota_status_(blink::mojom::QuotaStatusCode::kUnknown),
        usage_(-1) {
    if (is_third_party_context()) {
      scoped_feature_list_.InitAndEnableFeature(
          net::features::kThirdPartyStoragePartitioning);
      // Once we enable third-party storage partitioning, we can create a
      // third-party StorageKey and re-assign the StorageKey value in the
      // SandboxFileSystem with this value in SetUp for default buckets.
      storage_key_ = blink::StorageKey::Create(
          storage_key_.origin(), storage_key_.top_level_site(),
          blink::mojom::AncestorChainBit::kCrossSite);
    }
  }

  ObfuscatedFileUtilTest(const ObfuscatedFileUtilTest&) = delete;
  ObfuscatedFileUtilTest& operator=(const ObfuscatedFileUtilTest&) = delete;

  ~ObfuscatedFileUtilTest() override = default;

  bool is_incognito() {
    return GetParam() == TestMode::kIncognitoFirstParty ||
           (GetParam() == TestMode::kIncognitoFirstPartyNonDefaultBucket) ||
           (GetParam() == TestMode::kIncognitoThirdParty) ||
           (GetParam() == TestMode::kIncognitoThirdPartyNonDefaultBucket);
  }
  bool is_third_party_context() {
    return GetParam() == TestMode::kRegularThirdParty ||
           (GetParam() == TestMode::kRegularThirdPartyNonDefaultBucket) ||
           (GetParam() == TestMode::kIncognitoThirdParty) ||
           (GetParam() == TestMode::kIncognitoThirdPartyNonDefaultBucket);
  }
  bool is_non_default_bucket() {
    return GetParam() == TestMode::kRegularFirstPartyNonDefaultBucket ||
           (GetParam() == TestMode::kRegularThirdPartyNonDefaultBucket) ||
           (GetParam() == TestMode::kIncognitoFirstPartyNonDefaultBucket) ||
           (GetParam() == TestMode::kIncognitoThirdPartyNonDefaultBucket);
  }

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    storage_policy_ = base::MakeRefCounted<MockSpecialStoragePolicy>();

    quota_manager_task_runner_ =
        base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()});

    quota_manager_ = base::MakeRefCounted<QuotaManager>(
        is_incognito(), data_dir_.GetPath(), quota_manager_task_runner_,
        /*quota_change_callback=*/base::DoNothing(), storage_policy_,
        GetQuotaSettingsFunc());

    quota_manager_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](const scoped_refptr<QuotaManager>& quota_manager) {
                         QuotaSettings settings;
                         settings.per_storage_key_quota = 250 * 1024 * 1024;
                         settings.pool_size =
                             settings.per_storage_key_quota * 5;
                         settings.must_remain_available = 10 * 1024 * 1024;
                         settings.refresh_interval = base::TimeDelta::Max();
                         quota_manager->SetQuotaSettings(settings);
                       },
                       quota_manager_));

    // Every time we create a new sandbox_file_system helper,
    // it creates another context, which creates another path manager,
    // another sandbox_backend, and another OFU.
    // We need to pass in the context to skip all that.
    file_system_context_ =
        is_incognito() ? CreateIncognitoFileSystemContextForTesting(
                             base::SingleThreadTaskRunner::GetCurrentDefault(),
                             base::SingleThreadTaskRunner::GetCurrentDefault(),
                             quota_manager_->proxy(), data_dir_.GetPath())
                       : CreateFileSystemContextForTesting(
                             quota_manager_->proxy(), data_dir_.GetPath());

    // Create the default bucket member corresponding to the StorageKey member
    // we created.
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> default_future;
    quota_manager_->proxy()->UpdateOrCreateBucket(
        BucketInitParams::ForDefaultBucket(storage_key()),
        base::SequencedTaskRunner::GetCurrentDefault(),
        default_future.GetCallback());
    QuotaErrorOr<BucketInfo> default_bucket = default_future.Take();
    CHECK(default_bucket.has_value());
    default_bucket_ = default_bucket.value().ToBucketLocator();

    // Create a non-default bucket member corresponding to the StorageKey
    // member we created.
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> custom_future;
    BucketInitParams params = BucketInitParams::ForDefaultBucket(storage_key());
    params.name = "non-default bucket";
    quota_manager_->proxy()->UpdateOrCreateBucket(
        params, base::SequencedTaskRunner::GetCurrentDefault(),
        custom_future.GetCallback());
    QuotaErrorOr<BucketInfo> custom_bucket = custom_future.Take();
    CHECK(custom_bucket.has_value());
    custom_bucket_ = custom_bucket.value().ToBucketLocator();

    // Create an alternate non-default bucket member corresponding to the
    // StorageKey member we created.
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> alternate_future;
    params.name = "alternate non-default bucket";
    quota_manager_->proxy()->UpdateOrCreateBucket(
        params, base::SequencedTaskRunner::GetCurrentDefault(),
        alternate_future.GetCallback());
    QuotaErrorOr<BucketInfo> alternate_bucket = alternate_future.Take();
    CHECK(alternate_bucket.has_value());
    alternate_custom_bucket_ = alternate_bucket.value().ToBucketLocator();

    is_non_default_bucket()
        ? sandbox_file_system_.SetUp(file_system_context_, custom_bucket_)
        : sandbox_file_system_.SetUp(file_system_context_, default_bucket_);

    change_observers_ = MockFileChangeObserver::CreateList(&change_observer_);

    if (is_incognito())
      incognito_leveldb_environment_ = leveldb_chrome::NewMemEnv("FileSystem");
  }

  void TearDown() override {
    if (is_incognito())
      ASSERT_TRUE(IsDirectoryEmpty(data_dir_.GetPath()));

    quota_manager_ = nullptr;
    task_environment_.RunUntilIdle();
    sandbox_file_system_.TearDown();
  }

  std::unique_ptr<FileSystemOperationContext> LimitedContext(
      int64_t allowed_bytes_growth) {
    std::unique_ptr<FileSystemOperationContext> context(
        sandbox_file_system_.NewOperationContext());
    context->set_allowed_bytes_growth(allowed_bytes_growth);
    return context;
  }

  std::unique_ptr<FileSystemOperationContext> UnlimitedContext() {
    return LimitedContext(std::numeric_limits<int64_t>::max());
  }

  std::unique_ptr<FileSystemOperationContext> NewContext(
      SandboxFileSystemTestHelper* file_system) {
    change_observer()->ResetCount();
    std::unique_ptr<FileSystemOperationContext> context;
    if (file_system)
      context = file_system->NewOperationContext();
    else
      context = sandbox_file_system_.NewOperationContext();
    // Setting allowed_bytes_growth big enough for all tests.
    context->set_allowed_bytes_growth(1024 * 1024);
    context->set_change_observers(change_observers());
    return context;
  }

  const ChangeObserverList& change_observers() const {
    return change_observers_;
  }

  MockFileChangeObserver* change_observer() { return &change_observer_; }

  // This can only be used after SetUp has run and created file_system_context_
  // and obfuscated_file_util_.
  // Use this for tests which need to run in multiple StorageKeys; we need a
  // test helper per StorageKey.
  std::unique_ptr<SandboxFileSystemTestHelper> NewFileSystem(
      const blink::StorageKey& storage_key,
      FileSystemType type) {
    auto file_system =
        std::make_unique<SandboxFileSystemTestHelper>(storage_key, type);
    file_system->SetUp(file_system_context_);
    return file_system;
  }

  // This can only be used after SetUp has run and created file_system_context_
  // and obfuscated_file_util_.
  // Use this for tests which need to run in multiple BucketLocators; we need a
  // test helper per BucketLocator.
  std::unique_ptr<SandboxFileSystemTestHelper> NewFileSystem(
      const BucketLocator& bucket_locator,
      FileSystemType type) {
    auto file_system = std::make_unique<SandboxFileSystemTestHelper>(
        bucket_locator.storage_key, type);

    file_system->SetUp(file_system_context_, bucket_locator);
    return file_system;
  }

  std::unique_ptr<ObfuscatedFileUtil> CreateObfuscatedFileUtil(
      scoped_refptr<SpecialStoragePolicy> storage_policy) {
    return ObfuscatedFileUtil::CreateForTesting(
        std::move(storage_policy), data_dir_path(),
        is_incognito() ? incognito_leveldb_environment_.get() : nullptr,
        is_incognito());
  }

  ObfuscatedFileUtil* ofu() {
    return static_cast<ObfuscatedFileUtil*>(sandbox_file_system_.file_util());
  }

  const base::FilePath& test_directory() const { return data_dir_.GetPath(); }

  const blink::StorageKey& storage_key() const { return storage_key_; }

  const Origin& origin() const { return storage_key_.origin(); }

  FileSystemType type() const { return type_; }

  std::string type_string() const { return GetTypeString(type_); }

  int64_t ComputeTotalFileSize() {
    return sandbox_file_system_.ComputeCurrentStorageKeyUsage() -
           sandbox_file_system_.ComputeCurrentDirectoryDatabaseUsage();
  }

  void GetUsageFromQuotaManager() {
    int64_t quota = -1;
    quota_status_ = AsyncFileTestHelper::GetUsageAndQuota(
        quota_manager_->proxy(), storage_key(), sandbox_file_system_.type(),
        &usage_, &quota);
    EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, quota_status_);
  }

  void RevokeUsageCache() {
    quota_manager_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](const scoped_refptr<QuotaManager>& quota_manager,
                          SandboxFileSystemTestHelper* sandbox_file_system) {
                         quota_manager->ResetUsageTracker(
                             sandbox_file_system->storage_type());
                       },
                       quota_manager_, &sandbox_file_system_));
    base::FileErrorOr<base::FilePath> path =
        sandbox_file_system_.GetUsageCachePath();
    if (path.has_value())
      usage_cache()->Delete(path.value());
  }

  int64_t SizeByQuotaUtil() { return sandbox_file_system_.GetCachedUsage(); }

  int64_t SizeInUsageFile() {
    task_environment_.RunUntilIdle();
    int64_t usage = 0;
    base::FileErrorOr<base::FilePath> path =
        sandbox_file_system_.GetUsageCachePath();
    return (path.has_value() && usage_cache()->GetUsage(path.value(), &usage))
               ? usage
               : -1;
  }

  bool PathExists(const FileSystemURL& url) {
    std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
    base::File::Info file_info;
    base::FilePath platform_path;
    base::File::Error error =
        ofu()->GetFileInfo(context.get(), url, &file_info, &platform_path);
    return error == base::File::FILE_OK;
  }

  int64_t GetPathSize(const FileSystemURL& url) {
    std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
    base::File::Info file_info;
    base::FilePath platform_path;
    EXPECT_EQ(
        base::File::FILE_OK,
        ofu()->GetFileInfo(context.get(), url, &file_info, &platform_path));
    return file_info.size;
  }

  bool DirectoryExists(const FileSystemURL& url) {
    return AsyncFileTestHelper::DirectoryExists(file_system_context(), url);
  }

  int64_t usage() const { return usage_; }
  FileSystemUsageCache* usage_cache() {
    return sandbox_file_system_.usage_cache();
  }

  FileSystemURL CreateURL(const base::FilePath& path) {
    FileSystemURL test_url = sandbox_file_system_.CreateURL(path);
    if (is_non_default_bucket())
      test_url.SetBucket(custom_bucket_);
    return test_url;
  }

  FileSystemURL CreateURLFromUTF8(const std::string& path) {
    FileSystemURL test_url = sandbox_file_system_.CreateURLFromUTF8(path);
    if (is_non_default_bucket())
      test_url.SetBucket(custom_bucket_);
    return test_url;
  }

  int64_t PathCost(const FileSystemURL& url) {
    return ObfuscatedFileUtil::ComputeFilePathCost(url.path());
  }

  void CheckFile(const FileSystemURL& url) {
    std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
    base::FilePath local_path;
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->GetLocalFilePath(context.get(), url, &local_path));
    EXPECT_EQ(0, GetPathSize(url));

    base::File::Info file_info0;
    base::FilePath data_path;
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->GetFileInfo(context.get(), url, &file_info0, &data_path));
    EXPECT_EQ(data_path, local_path);
    EXPECT_EQ(!is_incognito(), FileExists(data_path));

    const char data[] = "test data";
    const int length = std::size(data) - 1;

    base::File file = ofu()->CreateOrOpen(
        context.get(), url, base::File::FLAG_WRITE | base::File::FLAG_OPEN);
    if (is_incognito()) {
      ASSERT_FALSE(file.IsValid());
      auto* memory_delegate =
          static_cast<ObfuscatedFileUtilMemoryDelegate*>(ofu()->delegate());
      ASSERT_EQ(
          length,
          memory_delegate->WriteFile(
              local_path, 0,
              base::MakeRefCounted<net::StringIOBuffer>(data).get(), length));
    } else {
      ASSERT_TRUE(file.IsValid());
      ASSERT_EQ(length, file.Write(0, data, length));
      file.Close();
    }

    base::File::Info file_info1;
    if (!is_incognito())
      EXPECT_EQ(length, GetLocalFileSize(data_path));
    EXPECT_EQ(length, GetPathSize(url));
    context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->GetFileInfo(context.get(), url, &file_info1, &data_path));
    EXPECT_EQ(data_path, local_path);

    EXPECT_FALSE(file_info0.is_directory);
    EXPECT_FALSE(file_info1.is_directory);
    EXPECT_FALSE(file_info0.is_symbolic_link);
    EXPECT_FALSE(file_info1.is_symbolic_link);
    EXPECT_EQ(0, file_info0.size);
    EXPECT_EQ(length, file_info1.size);
    EXPECT_LE(file_info0.last_modified, file_info1.last_modified);

    context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->Truncate(context.get(), url, length * 2));
    EXPECT_EQ(length * 2, GetPathSize(url));

    context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK, ofu()->Truncate(context.get(), url, 0));
    EXPECT_EQ(0, GetPathSize(url));
  }

  void ValidateTestDirectory(
      const FileSystemURL& root_url,
      const std::set<base::FilePath::StringType>& files,
      const std::set<base::FilePath::StringType>& directories) {
    std::unique_ptr<FileSystemOperationContext> context;
    for (const auto& file : files) {
      bool created = true;
      context = NewContext(nullptr);
      ASSERT_EQ(
          base::File::FILE_OK,
          ofu()->EnsureFileExists(
              context.get(), FileSystemURLAppend(root_url, file), &created));
      ASSERT_FALSE(created);
    }
    for (const auto& directory : directories) {
      context = NewContext(nullptr);
      EXPECT_TRUE(DirectoryExists(FileSystemURLAppend(root_url, directory)));
    }
  }

  class UsageVerifyHelper {
   public:
    UsageVerifyHelper(std::unique_ptr<FileSystemOperationContext> context,
                      SandboxFileSystemTestHelper* file_system,
                      int64_t expected_usage,
                      ObfuscatedFileUtilTest* test)
        : context_(std::move(context)),
          sandbox_file_system_(file_system),
          expected_usage_(expected_usage),
          test_(test) {}

    ~UsageVerifyHelper() {
      test_->task_environment_.RunUntilIdle();
      Check();
    }

    FileSystemOperationContext* context() { return context_.get(); }

   private:
    void Check() {
      ASSERT_EQ(expected_usage_, sandbox_file_system_->GetCachedUsage());
    }

    std::unique_ptr<FileSystemOperationContext> context_;
    raw_ptr<SandboxFileSystemTestHelper> sandbox_file_system_;
    int64_t expected_usage_;
    const raw_ptr<ObfuscatedFileUtilTest> test_;
  };

  std::unique_ptr<UsageVerifyHelper> AllowUsageIncrease(
      int64_t requested_growth) {
    int64_t usage = sandbox_file_system_.GetCachedUsage();
    return std::make_unique<UsageVerifyHelper>(LimitedContext(requested_growth),
                                               &sandbox_file_system_,
                                               usage + requested_growth, this);
  }

  std::unique_ptr<UsageVerifyHelper> DisallowUsageIncrease(
      int64_t requested_growth) {
    int64_t usage = sandbox_file_system_.GetCachedUsage();
    return std::make_unique<UsageVerifyHelper>(
        LimitedContext(requested_growth - 1), &sandbox_file_system_, usage,
        this);
  }

  void FillTestDirectory(const FileSystemURL& root_url,
                         std::set<base::FilePath::StringType>* files,
                         std::set<base::FilePath::StringType>* directories) {
    std::unique_ptr<FileSystemOperationContext> context;
    std::vector<filesystem::mojom::DirectoryEntry> entries;
    EXPECT_EQ(base::File::FILE_OK,
              AsyncFileTestHelper::ReadDirectory(file_system_context(),
                                                 root_url, &entries));
    EXPECT_EQ(0UL, entries.size());

    files->clear();
    files->insert(FILE_PATH_LITERAL("first"));
    files->insert(FILE_PATH_LITERAL("second"));
    files->insert(FILE_PATH_LITERAL("third"));
    directories->clear();
    directories->insert(FILE_PATH_LITERAL("fourth"));
    directories->insert(FILE_PATH_LITERAL("fifth"));
    directories->insert(FILE_PATH_LITERAL("sixth"));
    for (const auto& file : *files) {
      bool created = false;
      context = NewContext(nullptr);
      ASSERT_EQ(
          base::File::FILE_OK,
          ofu()->EnsureFileExists(
              context.get(), FileSystemURLAppend(root_url, file), &created));
      ASSERT_TRUE(created);
    }
    for (const auto& directory : *directories) {
      bool exclusive = true;
      bool recursive = false;
      context = NewContext(nullptr);
      EXPECT_EQ(base::File::FILE_OK,
                ofu()->CreateDirectory(context.get(),
                                       FileSystemURLAppend(root_url, directory),
                                       exclusive, recursive));
    }
    ValidateTestDirectory(root_url, *files, *directories);
  }

  void TestReadDirectoryHelper(const FileSystemURL& root_url) {
    std::set<base::FilePath::StringType> files;
    std::set<base::FilePath::StringType> directories;
    FillTestDirectory(root_url, &files, &directories);

    std::unique_ptr<FileSystemOperationContext> context;
    std::vector<filesystem::mojom::DirectoryEntry> entries;
    context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK,
              AsyncFileTestHelper::ReadDirectory(file_system_context(),
                                                 root_url, &entries));
    EXPECT_EQ(files.size() + directories.size(), entries.size());
    EXPECT_TRUE(change_observer()->HasNoChange());
    for (const filesystem::mojom::DirectoryEntry& entry : entries) {
      auto iter = files.find(entry.name.value());
      if (iter != files.end()) {
        EXPECT_EQ(entry.type, filesystem::mojom::FsFileType::REGULAR_FILE);
        files.erase(iter);
        continue;
      }
      iter = directories.find(entry.name.value());
      EXPECT_FALSE(directories.end() == iter);
      EXPECT_EQ(entry.type, filesystem::mojom::FsFileType::DIRECTORY);
      directories.erase(iter);
    }
  }

  void TestTouchHelper(const FileSystemURL& url, bool is_file) {
    base::Time last_access_time = base::Time::Now();
    base::Time last_modified_time = base::Time::Now();

    std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
    EXPECT_EQ(
        base::File::FILE_OK,
        ofu()->Touch(context.get(), url, last_access_time, last_modified_time));
    // Currently we fire no change notifications for Touch.
    EXPECT_TRUE(change_observer()->HasNoChange());
    base::FilePath local_path;
    base::File::Info file_info;
    context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->GetFileInfo(context.get(), url, &file_info, &local_path));
    // We compare as time_t here to lower our resolution, to avoid false
    // negatives caused by conversion to the local filesystem's native
    // representation and back.
    EXPECT_EQ(file_info.last_modified.ToTimeT(), last_modified_time.ToTimeT());

    context = NewContext(nullptr);
    last_modified_time += base::Hours(1);
    last_access_time += base::Hours(14);
    EXPECT_EQ(
        base::File::FILE_OK,
        ofu()->Touch(context.get(), url, last_access_time, last_modified_time));
    EXPECT_TRUE(change_observer()->HasNoChange());
    context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->GetFileInfo(context.get(), url, &file_info, &local_path));
    EXPECT_EQ(file_info.last_modified.ToTimeT(), last_modified_time.ToTimeT());
    if (is_file)  // Directories in OFU don't support atime.
      EXPECT_EQ(file_info.last_accessed.ToTimeT(), last_access_time.ToTimeT());
  }

  void TestCopyInForeignFileHelper(bool overwrite) {
    base::ScopedTempDir source_dir;
    ASSERT_TRUE(source_dir.CreateUniqueTempDir());
    base::FilePath root_file_path = source_dir.GetPath();
    base::FilePath src_file_path = root_file_path.AppendASCII("file_name");
    FileSystemURL dest_url = CreateURLFromUTF8("new file");
    int64_t src_file_length = 87;

    base::File file(src_file_path,
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());
    EXPECT_TRUE(file.created());
    ASSERT_TRUE(file.SetLength(src_file_length));
    file.Close();

    std::unique_ptr<FileSystemOperationContext> context;

    if (overwrite) {
      context = NewContext(nullptr);
      bool created = false;
      EXPECT_EQ(base::File::FILE_OK,
                ofu()->EnsureFileExists(context.get(), dest_url, &created));
      EXPECT_TRUE(created);

      // We must have observed one (and only one) create_file_count.
      EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
      EXPECT_TRUE(change_observer()->HasNoChange());
    }

    const int64_t path_cost =
        ObfuscatedFileUtil::ComputeFilePathCost(dest_url.path());
    if (!overwrite) {
      // Verify that file creation requires sufficient quota for the path.
      context = NewContext(nullptr);
      context->set_allowed_bytes_growth(path_cost + src_file_length - 1);
      EXPECT_EQ(
          base::File::FILE_ERROR_NO_SPACE,
          ofu()->CopyInForeignFile(context.get(), src_file_path, dest_url));
    }

    context = NewContext(nullptr);
    context->set_allowed_bytes_growth(path_cost + src_file_length);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->CopyInForeignFile(context.get(), src_file_path, dest_url));

    EXPECT_TRUE(PathExists(dest_url));
    EXPECT_FALSE(DirectoryExists(dest_url));

    context = NewContext(nullptr);
    base::File::Info file_info;
    base::FilePath data_path;
    EXPECT_EQ(base::File::FILE_OK, ofu()->GetFileInfo(context.get(), dest_url,
                                                      &file_info, &data_path));
    EXPECT_NE(data_path, src_file_path);
    EXPECT_TRUE(PathExists(dest_url));
    EXPECT_EQ(src_file_length, GetPathSize(dest_url));

    EXPECT_EQ(base::File::FILE_OK, ofu()->DeleteFile(context.get(), dest_url));
  }

  void ClearTimestamp(const FileSystemURL& url) {
    std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->Touch(context.get(), url, base::Time(), base::Time()));
    EXPECT_EQ(base::Time(), GetModifiedTime(url));
  }

  base::Time GetModifiedTime(const FileSystemURL& url) {
    std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
    base::FilePath data_path;
    base::File::Info file_info;
    context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->GetFileInfo(context.get(), url, &file_info, &data_path));
    EXPECT_TRUE(change_observer()->HasNoChange());
    return file_info.last_modified;
  }

  void TestDirectoryTimestampHelper(const FileSystemURL& base_dir,
                                    bool copy,
                                    bool overwrite) {
    std::unique_ptr<FileSystemOperationContext> context;
    const FileSystemURL src_dir_url(
        FileSystemURLAppendUTF8(base_dir, "foo_dir"));
    const FileSystemURL dest_dir_url(
        FileSystemURLAppendUTF8(base_dir, "bar_dir"));

    const FileSystemURL src_file_url(
        FileSystemURLAppendUTF8(src_dir_url, "hoge"));
    const FileSystemURL dest_file_url(
        FileSystemURLAppendUTF8(dest_dir_url, "fuga"));

    context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->CreateDirectory(context.get(), src_dir_url, true, true));
    context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->CreateDirectory(context.get(), dest_dir_url, true, true));

    bool created = false;
    context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->EnsureFileExists(context.get(), src_file_url, &created));
    if (overwrite) {
      context = NewContext(nullptr);
      EXPECT_EQ(
          base::File::FILE_OK,
          ofu()->EnsureFileExists(context.get(), dest_file_url, &created));
    }

    ClearTimestamp(src_dir_url);
    ClearTimestamp(dest_dir_url);
    context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->CopyOrMoveFile(context.get(), src_file_url, dest_file_url,
                                    FileSystemOperation::CopyOrMoveOptionSet(),
                                    copy));
    if (copy)
      EXPECT_EQ(base::Time(), GetModifiedTime(src_dir_url));
    else
      EXPECT_NE(base::Time(), GetModifiedTime(src_dir_url));
    EXPECT_NE(base::Time(), GetModifiedTime(dest_dir_url));
  }

  void MaybeDropDatabasesAliveCaseTestBody() {
    std::unique_ptr<ObfuscatedFileUtil> file_util =
        CreateObfuscatedFileUtil(/*storage_policy=*/nullptr);
    file_util->InitOriginDatabase(url::Origin(), true /*create*/);
    ASSERT_TRUE(file_util->origin_database_ != nullptr);

    // Callback to Drop DB is called while ObfuscatedFileUtilTest is
    // still alive.
    file_util->db_flush_delay_seconds_ = 0;
    file_util->MarkUsed();
    task_environment_.RunUntilIdle();

    ASSERT_TRUE(file_util->origin_database_ == nullptr);
  }

  void MaybeDropDatabasesAlreadyDeletedCaseTestBody() {
    // Run message loop after OFU is already deleted to make sure callback
    // doesn't cause a crash for use after free.
    {
      std::unique_ptr<ObfuscatedFileUtil> file_util =
          CreateObfuscatedFileUtil(/*storage_policy=*/nullptr);
      file_util->InitOriginDatabase(url::Origin(), true /*create*/);
      file_util->db_flush_delay_seconds_ = 0;
      file_util->MarkUsed();
    }

    // At this point the callback is still in the message queue but OFU is gone.
    task_environment_.RunUntilIdle();
  }

  void DestroyDirectoryDatabase_IsolatedTestBody() {
    storage_policy_->AddIsolated(origin().GetURL());
    FileSystemURL url = FileSystemURL::CreateForTest(
        storage_key(), kFileSystemTypePersistent, base::FilePath());
    if (is_non_default_bucket())
      url.SetBucket(custom_bucket_);

    // Create DirectoryDatabase for isolated origin.
    SandboxDirectoryDatabase* db =
        ofu()->GetDirectoryDatabase(url, true /* create */);
    ASSERT_TRUE(db != nullptr);

    // Destroy it.
    ofu()->DestroyDirectoryDatabaseForBucket(
        is_non_default_bucket() ? url.bucket().value() : default_bucket_,
        url.type());
    ASSERT_TRUE(ofu()->directories_.empty());
  }

  void GetDirectoryDatabase_IsolatedTestBody() {
    storage_policy_->AddIsolated(origin().GetURL());
    FileSystemURL url = FileSystemURL::CreateForTest(
        blink::StorageKey::CreateFirstParty(origin()),
        kFileSystemTypePersistent, base::FilePath());
    if (is_non_default_bucket())
      url.SetBucket(custom_bucket_);

    // Create DirectoryDatabase for isolated origin.
    SandboxDirectoryDatabase* db =
        ofu()->GetDirectoryDatabase(url, true /* create */);
    ASSERT_TRUE(db != nullptr);
    ASSERT_EQ(1U, ofu()->directories_.size());

    // Remove isolated.
    storage_policy_->RemoveIsolated(url.origin().GetURL());

    // This should still get the same database.
    SandboxDirectoryDatabase* db2 =
        ofu()->GetDirectoryDatabase(url, false /* create */);
    ASSERT_EQ(db, db2);
  }

  int64_t ComputeCurrentUsage() {
    return sandbox_file_system_.ComputeCurrentStorageKeyUsage() -
           sandbox_file_system_.ComputeCurrentDirectoryDatabaseUsage();
  }

  FileSystemContext* file_system_context() {
    return sandbox_file_system_.file_system_context();
  }

  const base::FilePath& data_dir_path() const { return data_dir_.GetPath(); }

  void CheckFileSize(FileSystemURL& url,
                     base::FilePath& local_path,
                     int64_t expected_size) {
    if (!is_incognito())
      EXPECT_EQ(expected_size, GetLocalFileSize(local_path));
    EXPECT_EQ(expected_size, GetPathSize(url));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<leveldb::Env> incognito_leveldb_environment_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  scoped_refptr<QuotaManager> quota_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> quota_manager_task_runner_;
  scoped_refptr<FileSystemContext> file_system_context_;
  blink::StorageKey storage_key_;
  BucketLocator default_bucket_;
  BucketLocator custom_bucket_;
  BucketLocator alternate_custom_bucket_;
  FileSystemType type_;
  SandboxFileSystemTestHelper sandbox_file_system_;
  blink::mojom::QuotaStatusCode quota_status_;
  int64_t usage_;
  MockFileChangeObserver change_observer_;
  ChangeObserverList change_observers_;
  base::WeakPtrFactory<ObfuscatedFileUtilTest> weak_factory_{this};
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ObfuscatedFileUtilTest,
    testing::Values(TestMode::kRegularFirstParty,
                    TestMode::kRegularFirstPartyNonDefaultBucket,
                    TestMode::kRegularThirdParty,
                    TestMode::kRegularThirdPartyNonDefaultBucket,
                    TestMode::kIncognitoFirstParty,
                    TestMode::kIncognitoFirstPartyNonDefaultBucket,
                    TestMode::kIncognitoThirdParty,
                    TestMode::kIncognitoThirdPartyNonDefaultBucket));

TEST_P(ObfuscatedFileUtilTest, TestCreateAndDeleteFile) {
  FileSystemURL url = CreateURLFromUTF8("fake/file");
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
  bool created;

  base::File::Error result =
      ofu()->EnsureFileExists(context.get(), url, &created);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, result);

  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ofu()->DeleteFile(context.get(), url));

  url = CreateURLFromUTF8("test file");

  EXPECT_TRUE(change_observer()->HasNoChange());

  // Verify that file creation requires sufficient quota for the path.
  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(url.path()) - 1);
  result = ofu()->EnsureFileExists(context.get(), url, &created);
  ASSERT_EQ(base::File::FILE_ERROR_NO_SPACE, result);

  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(url.path()));
  created = false;
  result = ofu()->EnsureFileExists(context.get(), url, &created);
  ASSERT_EQ(base::File::FILE_OK, result);
  ASSERT_TRUE(created);
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());

  CheckFile(url);

  context = NewContext(nullptr);
  base::FilePath local_path;
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->GetLocalFilePath(context.get(), url, &local_path));
  EXPECT_NE(is_incognito(), base::PathExists(local_path));
  EXPECT_TRUE(PathExists(url));

  // Verify that deleting a file isn't stopped by zero quota, and that it frees
  // up quote from its path.
  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(0);
  EXPECT_EQ(base::File::FILE_OK, ofu()->DeleteFile(context.get(), url));
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_FALSE(PathExists(url));
  EXPECT_EQ(ObfuscatedFileUtil::ComputeFilePathCost(url.path()),
            context->allowed_bytes_growth());

  context = NewContext(nullptr);
  bool exclusive = true;
  bool recursive = true;
  FileSystemURL root_url = CreateURLFromUTF8("series");
  FileSystemURL intermediate_url = FileSystemURLAppendUTF8(root_url, "of");
  FileSystemURL directory_url =
      FileSystemURLAppendUTF8(intermediate_url, "directories");
  url = FileSystemURLAppendUTF8(directory_url, "file name");
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), directory_url, exclusive,
                                   recursive));
  // The operation created 3 directories recursively.
  EXPECT_THAT(
      change_observer()->get_changed_urls(),
      testing::UnorderedElementsAre(root_url, intermediate_url, directory_url));
  EXPECT_EQ(3, change_observer()->get_and_reset_create_directory_count());

  context = NewContext(nullptr);
  created = false;
  result = ofu()->EnsureFileExists(context.get(), url, &created);
  ASSERT_TRUE(created);
  ASSERT_EQ(base::File::FILE_OK, result);
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());

  CheckFile(url);

  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->GetLocalFilePath(context.get(), url, &local_path));
  EXPECT_NE(is_incognito(), base::PathExists(local_path));
  EXPECT_TRUE(PathExists(url));

  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK, ofu()->DeleteFile(context.get(), url));
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_FALSE(PathExists(url));

  // Make sure we have no unexpected changes.
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_P(ObfuscatedFileUtilTest, TestTruncate) {
  bool created = false;
  FileSystemURL url = CreateURLFromUTF8("file");
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);

  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ofu()->Truncate(context.get(), url, 4));

  context = NewContext(nullptr);
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_TRUE(created);
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());

  context = NewContext(nullptr);
  base::FilePath local_path;
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->GetLocalFilePath(context.get(), url, &local_path));
  CheckFileSize(url, local_path, 0);

  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK, ofu()->Truncate(context.get(), url, 10));
  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  CheckFileSize(url, local_path, 10);

  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK, ofu()->Truncate(context.get(), url, 1));
  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  CheckFileSize(url, local_path, 1);

  EXPECT_FALSE(DirectoryExists(url));
  EXPECT_TRUE(PathExists(url));

  // Make sure we have no unexpected changes.
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_P(ObfuscatedFileUtilTest, TestQuotaOnTruncation) {
  bool created = false;
  FileSystemURL url = CreateURLFromUTF8("file");

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(url))->context(), url, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->Truncate(AllowUsageIncrease(1020)->context(), url, 1020));
  ASSERT_EQ(1020, ComputeTotalFileSize());

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->Truncate(AllowUsageIncrease(-1020)->context(), url, 0));
  ASSERT_EQ(0, ComputeTotalFileSize());

  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE,
            ofu()->Truncate(DisallowUsageIncrease(1021)->context(), url, 1021));
  ASSERT_EQ(0, ComputeTotalFileSize());

  EXPECT_EQ(base::File::FILE_OK,
            ofu()->Truncate(AllowUsageIncrease(1020)->context(), url, 1020));
  ASSERT_EQ(1020, ComputeTotalFileSize());

  EXPECT_EQ(base::File::FILE_OK,
            ofu()->Truncate(AllowUsageIncrease(0)->context(), url, 1020));
  ASSERT_EQ(1020, ComputeTotalFileSize());

  // quota exceeded
  {
    std::unique_ptr<UsageVerifyHelper> helper = AllowUsageIncrease(-1);
    helper->context()->set_allowed_bytes_growth(
        helper->context()->allowed_bytes_growth() - 1);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->Truncate(helper->context(), url, 1019));
    ASSERT_EQ(1019, ComputeTotalFileSize());
  }

  if (!is_incognito()) {
    // Delete backing file to make following truncation fail.
    base::FilePath local_path;
    ASSERT_EQ(
        base::File::FILE_OK,
        ofu()->GetLocalFilePath(UnlimitedContext().get(), url, &local_path));
    ASSERT_FALSE(local_path.empty());
    ASSERT_TRUE(base::DeleteFile(local_path));

    EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
              ofu()->Truncate(LimitedContext(1234).get(), url, 1234));
  } else {
    std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK, ofu()->DeleteFile(context.get(), url));
  }
  ASSERT_EQ(0, ComputeTotalFileSize());
}

TEST_P(ObfuscatedFileUtilTest, TestEnsureFileExists) {
  FileSystemURL url = CreateURLFromUTF8("fake/file");
  bool created = false;
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Verify that file creation requires sufficient quota for the path.
  context = NewContext(nullptr);
  url = CreateURLFromUTF8("test file");
  created = false;
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(url.path()) - 1);
  ASSERT_EQ(base::File::FILE_ERROR_NO_SPACE,
            ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_FALSE(created);
  EXPECT_TRUE(change_observer()->HasNoChange());

  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(url.path()));
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_TRUE(created);
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());

  CheckFile(url);

  context = NewContext(nullptr);
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_FALSE(created);
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Also test in a subdirectory.
  url = CreateURLFromUTF8("path/to/file.txt");
  context = NewContext(nullptr);
  bool exclusive = true;
  bool recursive = true;
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), FileSystemURLDirName(url),
                                   exclusive, recursive));
  // 2 directories: path/ and path/to.
  EXPECT_EQ(2, change_observer()->get_and_reset_create_directory_count());

  context = NewContext(nullptr);
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_TRUE(created);
  EXPECT_FALSE(DirectoryExists(url));
  EXPECT_TRUE(PathExists(url));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_P(ObfuscatedFileUtilTest, TestDirectoryOps) {
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);

  bool exclusive = false;
  bool recursive = false;
  FileSystemURL url = CreateURLFromUTF8("foo/bar");
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ofu()->CreateDirectory(context.get(), url, exclusive, recursive));

  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ofu()->DeleteDirectory(context.get(), url));

  FileSystemURL root = CreateURLFromUTF8(std::string());
  EXPECT_FALSE(DirectoryExists(url));
  EXPECT_FALSE(PathExists(url));
  context = NewContext(nullptr);
  EXPECT_TRUE(ofu()->IsDirectoryEmpty(context.get(), root));

  context = NewContext(nullptr);
  exclusive = false;
  recursive = true;
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), url, exclusive, recursive));
  EXPECT_EQ(2, change_observer()->get_and_reset_create_directory_count());

  EXPECT_TRUE(DirectoryExists(url));
  EXPECT_TRUE(PathExists(url));

  context = NewContext(nullptr);
  EXPECT_FALSE(ofu()->IsDirectoryEmpty(context.get(), root));
  EXPECT_TRUE(DirectoryExists(FileSystemURLDirName(url)));

  context = NewContext(nullptr);
  EXPECT_FALSE(
      ofu()->IsDirectoryEmpty(context.get(), FileSystemURLDirName(url)));

  // Can't remove a non-empty directory.
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_EMPTY,
            ofu()->DeleteDirectory(context.get(), FileSystemURLDirName(url)));
  EXPECT_TRUE(change_observer()->HasNoChange());

  base::File::Info file_info;
  base::FilePath local_path;
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->GetFileInfo(context.get(), url, &file_info, &local_path));
  EXPECT_TRUE(local_path.empty());
  EXPECT_TRUE(file_info.is_directory);
  EXPECT_FALSE(file_info.is_symbolic_link);

  // Same create again should succeed, since exclusive is false.
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), url, exclusive, recursive));
  EXPECT_TRUE(change_observer()->HasNoChange());

  exclusive = true;
  recursive = true;
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS,
            ofu()->CreateDirectory(context.get(), url, exclusive, recursive));
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Verify that deleting a directory isn't stopped by zero quota, and that it
  // frees up quota from its path.
  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(0);
  EXPECT_EQ(base::File::FILE_OK, ofu()->DeleteDirectory(context.get(), url));
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(ObfuscatedFileUtil::ComputeFilePathCost(url.path()),
            context->allowed_bytes_growth());

  url = CreateURLFromUTF8("foo/bop");

  EXPECT_FALSE(DirectoryExists(url));
  EXPECT_FALSE(PathExists(url));

  context = NewContext(nullptr);
  EXPECT_TRUE(ofu()->IsDirectoryEmpty(context.get(), url));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ofu()->GetFileInfo(context.get(), url, &file_info, &local_path));

  // Verify that file creation requires sufficient quota for the path.
  exclusive = true;
  recursive = false;
  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(url.path()) - 1);
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE,
            ofu()->CreateDirectory(context.get(), url, exclusive, recursive));
  EXPECT_TRUE(change_observer()->HasNoChange());

  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(url.path()));
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), url, exclusive, recursive));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());

  EXPECT_TRUE(DirectoryExists(url));
  EXPECT_TRUE(PathExists(url));

  exclusive = true;
  recursive = false;
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS,
            ofu()->CreateDirectory(context.get(), url, exclusive, recursive));
  EXPECT_TRUE(change_observer()->HasNoChange());

  exclusive = true;
  recursive = false;
  url = CreateURLFromUTF8("foo");
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS,
            ofu()->CreateDirectory(context.get(), url, exclusive, recursive));
  EXPECT_TRUE(change_observer()->HasNoChange());

  url = CreateURLFromUTF8("blah");

  EXPECT_FALSE(DirectoryExists(url));
  EXPECT_FALSE(PathExists(url));

  exclusive = true;
  recursive = false;
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), url, exclusive, recursive));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());

  EXPECT_TRUE(DirectoryExists(url));
  EXPECT_TRUE(PathExists(url));

  exclusive = true;
  recursive = false;
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS,
            ofu()->CreateDirectory(context.get(), url, exclusive, recursive));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_P(ObfuscatedFileUtilTest, TestReadDirectory) {
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
  bool exclusive = true;
  bool recursive = true;
  FileSystemURL url = CreateURLFromUTF8("directory/to/use");
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), url, exclusive, recursive));
  TestReadDirectoryHelper(url);
}

TEST_P(ObfuscatedFileUtilTest, TestReadRootWithSlash) {
  TestReadDirectoryHelper(CreateURLFromUTF8(std::string()));
}

TEST_P(ObfuscatedFileUtilTest, TestReadRootWithEmptyString) {
  TestReadDirectoryHelper(CreateURLFromUTF8("/"));
}

TEST_P(ObfuscatedFileUtilTest, TestReadDirectoryOnFile) {
  FileSystemURL url = CreateURLFromUTF8("file");
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_TRUE(created);

  std::vector<filesystem::mojom::DirectoryEntry> entries;
  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_A_DIRECTORY,
      AsyncFileTestHelper::ReadDirectory(file_system_context(), url, &entries));

  EXPECT_TRUE(ofu()->IsDirectoryEmpty(context.get(), url));
}

// TODO(crbug.com/40511450): Remove this test once last_access_time has
// been removed after PPAPI has been deprecated. Fuchsia does not support touch,
// which breaks this test that relies on it. Since PPAPI is being deprecated,
// this test is excluded from the Fuchsia build.
// See https://crbug.com/1077456 for details.
#if !BUILDFLAG(IS_FUCHSIA)
TEST_P(ObfuscatedFileUtilTest, TestTouch) {
  FileSystemURL url = CreateURLFromUTF8("file");
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);

  base::Time last_access_time = base::Time::Now();
  base::Time last_modified_time = base::Time::Now();

  // It's not there yet.
  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      ofu()->Touch(context.get(), url, last_access_time, last_modified_time));

  // OK, now create it.
  context = NewContext(nullptr);
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_TRUE(created);
  TestTouchHelper(url, true);

  // Now test a directory:
  context = NewContext(nullptr);
  bool exclusive = true;
  bool recursive = false;
  url = CreateURLFromUTF8("dir");
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), url, exclusive, recursive));
  TestTouchHelper(url, false);
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

TEST_P(ObfuscatedFileUtilTest, TestPathQuotas) {
  FileSystemURL url = CreateURLFromUTF8("fake/file");
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);

  url = CreateURLFromUTF8("file name");
  context->set_allowed_bytes_growth(5);
  bool created = false;
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_FALSE(created);
  context->set_allowed_bytes_growth(1024);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_TRUE(created);
  int64_t path_cost = ObfuscatedFileUtil::ComputeFilePathCost(url.path());
  EXPECT_EQ(1024 - path_cost, context->allowed_bytes_growth());

  context->set_allowed_bytes_growth(1024);
  bool exclusive = true;
  bool recursive = true;
  url = CreateURLFromUTF8("directory/to/use");
  path_cost = 0;
  for (const auto& component : url.path().GetComponents()) {
    path_cost +=
        ObfuscatedFileUtil::ComputeFilePathCost(base::FilePath(component));
  }
  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(1024);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), url, exclusive, recursive));
  EXPECT_EQ(1024 - path_cost, context->allowed_bytes_growth());
}

TEST_P(ObfuscatedFileUtilTest, TestCopyOrMoveFileNotFound) {
  FileSystemURL source_url = CreateURLFromUTF8("path0.txt");
  FileSystemURL dest_url = CreateURLFromUTF8("path1.txt");
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);

  bool is_copy_not_move = false;
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ofu()->CopyOrMoveFile(context.get(), source_url, dest_url,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  is_copy_not_move));
  EXPECT_TRUE(change_observer()->HasNoChange());
  context = NewContext(nullptr);
  is_copy_not_move = true;
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ofu()->CopyOrMoveFile(context.get(), source_url, dest_url,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  is_copy_not_move));
  EXPECT_TRUE(change_observer()->HasNoChange());
  source_url = CreateURLFromUTF8("dir/dir/file");
  bool exclusive = true;
  bool recursive = true;
  context = NewContext(nullptr);
  ASSERT_EQ(
      base::File::FILE_OK,
      ofu()->CreateDirectory(context.get(), FileSystemURLDirName(source_url),
                             exclusive, recursive));
  EXPECT_EQ(2, change_observer()->get_and_reset_create_directory_count());
  is_copy_not_move = false;
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ofu()->CopyOrMoveFile(context.get(), source_url, dest_url,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  is_copy_not_move));
  EXPECT_TRUE(change_observer()->HasNoChange());
  context = NewContext(nullptr);
  is_copy_not_move = true;
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ofu()->CopyOrMoveFile(context.get(), source_url, dest_url,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  is_copy_not_move));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_P(ObfuscatedFileUtilTest, TestCopyOrMoveFileSuccess) {
  const int64_t kSourceLength = 5;
  const int64_t kDestLength = 50;

  for (size_t i = 0; i < std::size(kCopyMoveTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "kCopyMoveTestCase " << i);
    const CopyMoveTestCaseRecord& test_case = kCopyMoveTestCases[i];
    SCOPED_TRACE(testing::Message()
                 << "\t is_copy_not_move " << test_case.is_copy_not_move);
    SCOPED_TRACE(testing::Message()
                 << "\t source_path " << test_case.source_path);
    SCOPED_TRACE(testing::Message() << "\t dest_path " << test_case.dest_path);
    SCOPED_TRACE(testing::Message()
                 << "\t cause_overwrite " << test_case.cause_overwrite);
    std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);

    bool exclusive = false;
    bool recursive = true;
    FileSystemURL source_url = CreateURLFromUTF8(test_case.source_path);
    FileSystemURL dest_url = CreateURLFromUTF8(test_case.dest_path);

    context = NewContext(nullptr);
    ASSERT_EQ(
        base::File::FILE_OK,
        ofu()->CreateDirectory(context.get(), FileSystemURLDirName(source_url),
                               exclusive, recursive));
    context = NewContext(nullptr);
    ASSERT_EQ(
        base::File::FILE_OK,
        ofu()->CreateDirectory(context.get(), FileSystemURLDirName(dest_url),
                               exclusive, recursive));

    bool created = false;
    context = NewContext(nullptr);
    ASSERT_EQ(base::File::FILE_OK,
              ofu()->EnsureFileExists(context.get(), source_url, &created));
    ASSERT_TRUE(created);
    context = NewContext(nullptr);
    ASSERT_EQ(base::File::FILE_OK,
              ofu()->Truncate(context.get(), source_url, kSourceLength));

    if (test_case.cause_overwrite) {
      context = NewContext(nullptr);
      created = false;
      ASSERT_EQ(base::File::FILE_OK,
                ofu()->EnsureFileExists(context.get(), dest_url, &created));
      ASSERT_TRUE(created);
      context = NewContext(nullptr);
      ASSERT_EQ(base::File::FILE_OK,
                ofu()->Truncate(context.get(), dest_url, kDestLength));
    }

    context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->CopyOrMoveFile(context.get(), source_url, dest_url,
                                    FileSystemOperation::CopyOrMoveOptionSet(),
                                    test_case.is_copy_not_move));

    if (test_case.is_copy_not_move) {
      base::File::Info file_info;
      base::FilePath local_path;
      context = NewContext(nullptr);
      EXPECT_EQ(base::File::FILE_OK,
                ofu()->GetFileInfo(context.get(), source_url, &file_info,
                                   &local_path));
      EXPECT_EQ(kSourceLength, file_info.size);
      EXPECT_EQ(base::File::FILE_OK,
                ofu()->DeleteFile(context.get(), source_url));
    } else {
      base::File::Info file_info;
      base::FilePath local_path;
      context = NewContext(nullptr);
      EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
                ofu()->GetFileInfo(context.get(), source_url, &file_info,
                                   &local_path));
    }
    base::File::Info file_info;
    base::FilePath local_path;
    EXPECT_EQ(base::File::FILE_OK, ofu()->GetFileInfo(context.get(), dest_url,
                                                      &file_info, &local_path));
    EXPECT_EQ(kSourceLength, file_info.size);

    EXPECT_EQ(base::File::FILE_OK, ofu()->DeleteFile(context.get(), dest_url));
  }
}

TEST_P(ObfuscatedFileUtilTest, TestCopyPathQuotas) {
  FileSystemURL src_url = CreateURLFromUTF8("src path");
  FileSystemURL dest_url = CreateURLFromUTF8("destination path");
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), src_url, &created));

  bool is_copy = true;
  // Copy, no overwrite.
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(dest_url.path()) - 1);
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE,
            ofu()->CopyOrMoveFile(context.get(), src_url, dest_url,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  is_copy));
  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(dest_url.path()));
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CopyOrMoveFile(context.get(), src_url, dest_url,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  is_copy));

  // Copy, with overwrite.
  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(0);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CopyOrMoveFile(context.get(), src_url, dest_url,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  is_copy));
}

TEST_P(ObfuscatedFileUtilTest, TestMovePathQuotasWithRename) {
  FileSystemURL src_url = CreateURLFromUTF8("src path");
  FileSystemURL dest_url = CreateURLFromUTF8("destination path");
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), src_url, &created));

  bool is_copy = false;
  // Move, rename, no overwrite.
  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(dest_url.path()) -
      ObfuscatedFileUtil::ComputeFilePathCost(src_url.path()) - 1);
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE,
            ofu()->CopyOrMoveFile(context.get(), src_url, dest_url,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  is_copy));
  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(dest_url.path()) -
      ObfuscatedFileUtil::ComputeFilePathCost(src_url.path()));
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CopyOrMoveFile(context.get(), src_url, dest_url,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  is_copy));

  context = NewContext(nullptr);
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), src_url, &created));

  // Move, rename, with overwrite.
  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(0);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CopyOrMoveFile(context.get(), src_url, dest_url,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  is_copy));
}

TEST_P(ObfuscatedFileUtilTest, TestMovePathQuotasWithoutRename) {
  FileSystemURL src_url = CreateURLFromUTF8("src path");
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), src_url, &created));

  bool exclusive = true;
  bool recursive = false;
  FileSystemURL dir_url = CreateURLFromUTF8("directory path");
  context = NewContext(nullptr);
  ASSERT_EQ(base::File::FILE_OK, ofu()->CreateDirectory(context.get(), dir_url,
                                                        exclusive, recursive));

  FileSystemURL dest_url = FileSystemURLAppend(dir_url, src_url.path().value());

  bool is_copy = false;
  int64_t allowed_bytes_growth = -1000;  // Over quota, this should still work.
  // Move, no rename, no overwrite.
  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(allowed_bytes_growth);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CopyOrMoveFile(context.get(), src_url, dest_url,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  is_copy));
  EXPECT_EQ(allowed_bytes_growth, context->allowed_bytes_growth());

  // Move, no rename, with overwrite.
  context = NewContext(nullptr);
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), src_url, &created));
  context = NewContext(nullptr);
  context->set_allowed_bytes_growth(allowed_bytes_growth);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CopyOrMoveFile(context.get(), src_url, dest_url,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  is_copy));
  EXPECT_EQ(allowed_bytes_growth +
                ObfuscatedFileUtil::ComputeFilePathCost(src_url.path()),
            context->allowed_bytes_growth());
}

TEST_P(ObfuscatedFileUtilTest, TestCopyInForeignFile) {
  TestCopyInForeignFileHelper(false /* overwrite */);
  TestCopyInForeignFileHelper(true /* overwrite */);
}

TEST_P(ObfuscatedFileUtilTest, TestEnumerator) {
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
  FileSystemURL src_url = CreateURLFromUTF8("source dir");
  bool exclusive = true;
  bool recursive = false;
  ASSERT_EQ(base::File::FILE_OK, ofu()->CreateDirectory(context.get(), src_url,
                                                        exclusive, recursive));

  std::set<base::FilePath::StringType> files;
  std::set<base::FilePath::StringType> directories;
  FillTestDirectory(src_url, &files, &directories);

  FileSystemURL dest_url = CreateURLFromUTF8("destination dir");

  EXPECT_FALSE(DirectoryExists(dest_url));
  ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::Copy(
                                     file_system_context(), src_url, dest_url));

  ValidateTestDirectory(dest_url, files, directories);
  EXPECT_TRUE(DirectoryExists(src_url));
  EXPECT_TRUE(DirectoryExists(dest_url));
  recursive = true;
  ASSERT_EQ(
      base::File::FILE_OK,
      AsyncFileTestHelper::Remove(file_system_context(), dest_url, recursive));
  EXPECT_FALSE(DirectoryExists(dest_url));
}

TEST_P(ObfuscatedFileUtilTest, TestStorageKeyEnumerator) {
  std::unique_ptr<ObfuscatedFileUtil::AbstractStorageKeyEnumerator> enumerator =
      ofu()->CreateStorageKeyEnumerator();
  // The test helper starts out with a single filesystem.
  EXPECT_TRUE(enumerator.get());
  // This test is not relevant for third-party or non-default buckets code paths
  // because these paths do not add to the OriginDatabase, the structure that
  // populates the enumerator being tested. So in a test environment, this
  // enumerator should not have any additional StorageKeys to access via Next().
  if (is_third_party_context() || is_non_default_bucket()) {
    EXPECT_EQ(std::nullopt, enumerator->Next());
    return;
  }
  EXPECT_EQ(storage_key(), enumerator->Next());
  ASSERT_TRUE(type() == kFileSystemTypeTemporary);
  EXPECT_TRUE(HasFileSystemType(enumerator.get(), kFileSystemTypeTemporary));
  EXPECT_FALSE(HasFileSystemType(enumerator.get(), kFileSystemTypePersistent));
  EXPECT_FALSE(enumerator->Next());
  EXPECT_FALSE(HasFileSystemType(enumerator.get(), kFileSystemTypeTemporary));
  EXPECT_FALSE(HasFileSystemType(enumerator.get(), kFileSystemTypePersistent));

  std::set<blink::StorageKey> storage_keys_expected;
  storage_keys_expected.insert(storage_key());

  for (size_t i = 0; i < std::size(kOriginEnumerationTestRecords); ++i) {
    SCOPED_TRACE(testing::Message()
                 << "Validating kOriginEnumerationTestRecords " << i);
    const OriginEnumerationTestRecord& record =
        kOriginEnumerationTestRecords[i];
    blink::StorageKey storage_key =
        blink::StorageKey::CreateFromStringForTesting(record.origin_url);
    storage_keys_expected.insert(storage_key);
    if (record.has_temporary) {
      std::unique_ptr<SandboxFileSystemTestHelper> file_system =
          NewFileSystem(storage_key, kFileSystemTypeTemporary);
      std::unique_ptr<FileSystemOperationContext> context(
          NewContext(file_system.get()));
      bool created = false;
      ASSERT_EQ(
          base::File::FILE_OK,
          ofu()->EnsureFileExists(
              context.get(), file_system->CreateURLFromUTF8("file"), &created));
      EXPECT_TRUE(created);
    }
    if (record.has_persistent) {
      std::unique_ptr<SandboxFileSystemTestHelper> file_system =
          NewFileSystem(storage_key, kFileSystemTypePersistent);
      std::unique_ptr<FileSystemOperationContext> context(
          NewContext(file_system.get()));
      bool created = false;
      ASSERT_EQ(
          base::File::FILE_OK,
          ofu()->EnsureFileExists(
              context.get(), file_system->CreateURLFromUTF8("file"), &created));
      EXPECT_TRUE(created);
    }
  }
  enumerator = ofu()->CreateStorageKeyEnumerator();
  EXPECT_TRUE(enumerator.get());
  std::set<blink::StorageKey> storage_keys_found;
  std::optional<blink::StorageKey> enumerator_storage_key;
  while ((enumerator_storage_key = enumerator->Next()).has_value()) {
    storage_keys_found.insert(enumerator_storage_key.value());
    SCOPED_TRACE(testing::Message()
                 << "Handling "
                 << enumerator_storage_key->origin().Serialize());
    bool found = false;
    for (const auto& record : kOriginEnumerationTestRecords) {
      if (enumerator_storage_key->origin().GetURL() != record.origin_url)
        continue;
      found = true;
      EXPECT_EQ(record.has_temporary,
                HasFileSystemType(enumerator.get(), kFileSystemTypeTemporary));
      EXPECT_EQ(record.has_persistent,
                HasFileSystemType(enumerator.get(), kFileSystemTypePersistent));
    }
    // Deal with the default filesystem created by the test helper.
    if (!found && enumerator_storage_key == storage_key()) {
      ASSERT_TRUE(type() == kFileSystemTypeTemporary);
      EXPECT_TRUE(
          HasFileSystemType(enumerator.get(), kFileSystemTypeTemporary));
      EXPECT_FALSE(
          HasFileSystemType(enumerator.get(), kFileSystemTypePersistent));
      found = true;
    }
    EXPECT_TRUE(found);
  }

  std::set<blink::StorageKey> diff;
  std::set_symmetric_difference(
      storage_keys_expected.begin(), storage_keys_expected.end(),
      storage_keys_found.begin(), storage_keys_found.end(),
      inserter(diff, diff.begin()));
  EXPECT_TRUE(diff.empty());
}

TEST_P(ObfuscatedFileUtilTest, TestRevokeUsageCache) {
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);

  int64_t expected_quota = 0;

  for (size_t i = 0; i < kRegularFileSystemTestCaseSize; ++i) {
    SCOPED_TRACE(testing::Message() << "Creating kRegularTestCase " << i);
    const FileSystemTestCaseRecord& test_case = kRegularFileSystemTestCases[i];
    base::FilePath file_path(test_case.path);
    expected_quota += ObfuscatedFileUtil::ComputeFilePathCost(file_path);
    if (test_case.is_directory) {
      bool exclusive = true;
      bool recursive = false;
      ASSERT_EQ(base::File::FILE_OK,
                ofu()->CreateDirectory(context.get(), CreateURL(file_path),
                                       exclusive, recursive));
    } else {
      bool created = false;
      ASSERT_EQ(base::File::FILE_OK,
                ofu()->EnsureFileExists(context.get(), CreateURL(file_path),
                                        &created));
      ASSERT_TRUE(created);
      ASSERT_EQ(base::File::FILE_OK,
                ofu()->Truncate(context.get(), CreateURL(file_path),
                                test_case.data_file_size));
      expected_quota += test_case.data_file_size;
    }
  }

  // Usually raw size in usage cache and the usage returned by QuotaUtil
  // should be same.
  EXPECT_EQ(expected_quota, SizeInUsageFile());
  EXPECT_EQ(expected_quota, SizeByQuotaUtil());

  RevokeUsageCache();
  EXPECT_EQ(-1, SizeInUsageFile());
  EXPECT_EQ(expected_quota, SizeByQuotaUtil());

  // This should reconstruct the cache.
  GetUsageFromQuotaManager();
  EXPECT_EQ(expected_quota, SizeInUsageFile());
  EXPECT_EQ(expected_quota, SizeByQuotaUtil());
  EXPECT_EQ(expected_quota, usage());
}

TEST_P(ObfuscatedFileUtilTest, TestInconsistency) {
  const FileSystemURL kPath1 = CreateURLFromUTF8("hoge");
  const FileSystemURL kPath2 = CreateURLFromUTF8("fuga");

  std::unique_ptr<FileSystemOperationContext> context;
  base::File::Info file_info;
  base::FilePath data_path;
  bool created = false;

  // Create a non-empty file.
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), kPath1, &created));
  EXPECT_TRUE(created);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK, ofu()->Truncate(context.get(), kPath1, 10));
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->GetFileInfo(context.get(), kPath1, &file_info, &data_path));
  EXPECT_EQ(10, file_info.size);

  // Destroy database to make inconsistency between database and filesystem.
  ofu()->DestroyDirectoryDatabaseForBucket(
      is_non_default_bucket() ? custom_bucket_ : default_bucket_, type());

  // Try to get file info of broken file.
  EXPECT_FALSE(PathExists(kPath1));
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), kPath1, &created));
  EXPECT_TRUE(created);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->GetFileInfo(context.get(), kPath1, &file_info, &data_path));
  EXPECT_EQ(0, file_info.size);

  // Make another broken file to `kPath2`.
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), kPath2, &created));
  EXPECT_TRUE(created);

  // Destroy again.
  ofu()->DestroyDirectoryDatabaseForBucket(
      is_non_default_bucket() ? custom_bucket_ : default_bucket_, type());

  // Repair broken `kPath1`.
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ofu()->Touch(context.get(), kPath1, base::Time::Now(),
                         base::Time::Now()));
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), kPath1, &created));
  EXPECT_TRUE(created);

  // Copy from sound `kPath1` to broken `kPath2`.
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CopyOrMoveFile(context.get(), kPath1, kPath2,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  true /* copy */));

  ofu()->DestroyDirectoryDatabaseForBucket(
      is_non_default_bucket() ? custom_bucket_ : default_bucket_, type());
  context = NewContext(nullptr);
  created = false;
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), kPath1, &created));
  EXPECT_TRUE(created);

  EXPECT_EQ(0, GetPathSize(kPath1));
}

TEST_P(ObfuscatedFileUtilTest, TestIncompleteDirectoryReading) {
  const FileSystemURL kPath[] = {CreateURLFromUTF8("foo"),
                                 CreateURLFromUTF8("bar"),
                                 CreateURLFromUTF8("baz")};
  const FileSystemURL empty_path = CreateURL(base::FilePath());
  std::unique_ptr<FileSystemOperationContext> context;

  for (const auto& path : kPath) {
    bool created = false;
    context = NewContext(nullptr);
    EXPECT_EQ(base::File::FILE_OK,
              ofu()->EnsureFileExists(context.get(), path, &created));
    EXPECT_TRUE(created);
  }

  std::vector<filesystem::mojom::DirectoryEntry> entries;
  EXPECT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::ReadDirectory(file_system_context(),
                                               empty_path, &entries));
  EXPECT_EQ(3u, entries.size());

  EXPECT_EQ(base::File::FILE_OK, ofu()->DeleteFile(context.get(), kPath[0]));

  entries.clear();
  EXPECT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::ReadDirectory(file_system_context(),
                                               empty_path, &entries));
  EXPECT_EQ(std::size(kPath) - 1, entries.size());
}

TEST_P(ObfuscatedFileUtilTest, TestDirectoryTimestampForCreation) {
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
  const FileSystemURL dir_url = CreateURLFromUTF8("foo_dir");

  // Create working directory.
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), dir_url, false, false));

  // EnsureFileExists, create case.
  FileSystemURL url(FileSystemURLAppendUTF8(dir_url, "EnsureFileExists_file"));
  bool created = false;
  ClearTimestamp(dir_url);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_TRUE(created);
  EXPECT_NE(base::Time(), GetModifiedTime(dir_url));

  // non create case.
  created = true;
  ClearTimestamp(dir_url);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_FALSE(created);
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // fail case.
  url = FileSystemURLAppendUTF8(dir_url, "EnsureFileExists_dir");
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), url, false, false));

  ClearTimestamp(dir_url);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_FILE,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // CreateOrOpen, create case.
  url = FileSystemURLAppendUTF8(dir_url, "CreateOrOpen_file");
  ClearTimestamp(dir_url);
  context = NewContext(nullptr);
  created = false;
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_TRUE(created);
  EXPECT_NE(base::Time(), GetModifiedTime(dir_url));

  // open case.
  ClearTimestamp(dir_url);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_FALSE(created);
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // fail case
  ClearTimestamp(dir_url);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_FALSE(created);
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // CreateDirectory, create case.
  // Creating CreateDirectory_dir and CreateDirectory_dir/subdir.
  url = FileSystemURLAppendUTF8(dir_url, "CreateDirectory_dir");
  FileSystemURL subdir_url(FileSystemURLAppendUTF8(url, "subdir"));
  ClearTimestamp(dir_url);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), subdir_url,
                                   true /* exclusive */, true /* recursive */));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_url));

  // create subdir case.
  // Creating CreateDirectory_dir/subdir2.
  subdir_url = FileSystemURLAppendUTF8(url, "subdir2");
  ClearTimestamp(dir_url);
  ClearTimestamp(url);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), subdir_url,
                                   true /* exclusive */, true /* recursive */));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));
  EXPECT_NE(base::Time(), GetModifiedTime(url));

  // fail case.
  url = FileSystemURLAppendUTF8(dir_url, "CreateDirectory_dir");
  ClearTimestamp(dir_url);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS,
            ofu()->CreateDirectory(context.get(), url, true /* exclusive */,
                                   true /* recursive */));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // CopyInForeignFile, create case.
  url = FileSystemURLAppendUTF8(dir_url, "CopyInForeignFile_file");
  base::ScopedTempDir foreign_source_dir;
  ASSERT_TRUE(foreign_source_dir.CreateUniqueTempDir());
  base::FilePath foreign_src_file_path =
      foreign_source_dir.GetPath().AppendASCII("file_name");

  EXPECT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(foreign_src_file_path, &created));
  EXPECT_TRUE(created);

  ClearTimestamp(dir_url);
  context = NewContext(nullptr);
  EXPECT_EQ(
      base::File::FILE_OK,
      ofu()->CopyInForeignFile(context.get(), foreign_src_file_path, url));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_url));
}

TEST_P(ObfuscatedFileUtilTest, TestDirectoryTimestampForDeletion) {
  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
  const FileSystemURL dir_url = CreateURLFromUTF8("foo_dir");

  // Create working directory.
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), dir_url, false, false));

  // DeleteFile, delete case.
  FileSystemURL url = FileSystemURLAppendUTF8(dir_url, "DeleteFile_file");
  bool created = false;
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_TRUE(created);

  ClearTimestamp(dir_url);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK, ofu()->DeleteFile(context.get(), url));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_url));

  // fail case.
  ClearTimestamp(dir_url);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ofu()->DeleteFile(context.get(), url));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // DeleteDirectory, fail case.
  url = FileSystemURLAppendUTF8(dir_url, "DeleteDirectory_dir");
  FileSystemURL file_path(FileSystemURLAppendUTF8(url, "pakeratta"));
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), url, true, true));
  created = false;
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), file_path, &created));
  EXPECT_TRUE(created);

  ClearTimestamp(dir_url);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_EMPTY,
            ofu()->DeleteDirectory(context.get(), url));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // delete case.
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK, ofu()->DeleteFile(context.get(), file_path));

  ClearTimestamp(dir_url);
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK, ofu()->DeleteDirectory(context.get(), url));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_url));
}

TEST_P(ObfuscatedFileUtilTest, TestDirectoryTimestampForCopyAndMove) {
  TestDirectoryTimestampHelper(CreateURLFromUTF8("copy overwrite"), true, true);
  TestDirectoryTimestampHelper(CreateURLFromUTF8("copy non-overwrite"), true,
                               false);
  TestDirectoryTimestampHelper(CreateURLFromUTF8("move overwrite"), false,
                               true);
  TestDirectoryTimestampHelper(CreateURLFromUTF8("move non-overwrite"), false,
                               false);
}

TEST_P(ObfuscatedFileUtilTest, TestFileEnumeratorTimestamp) {
  FileSystemURL dir = CreateURLFromUTF8("foo");
  FileSystemURL url1 = FileSystemURLAppendUTF8(dir, "bar");
  FileSystemURL url2 = FileSystemURLAppendUTF8(dir, "baz");

  std::unique_ptr<FileSystemOperationContext> context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), dir, false, false));

  bool created = false;
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(context.get(), url1, &created));
  EXPECT_TRUE(created);

  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(context.get(), url2, false, false));

  base::FilePath file_path;
  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->GetLocalFilePath(context.get(), url1, &file_path));
  EXPECT_FALSE(file_path.empty());

  context = NewContext(nullptr);
  EXPECT_EQ(base::File::FILE_OK,
            ofu()->Touch(context.get(), url1,
                         base::Time::Now() + base::Hours(1), base::Time()));

  context = NewContext(nullptr);
  std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum =
      ofu()->CreateFileEnumerator(context.get(), dir, false);

  int count = 0;
  base::FilePath file_path_each;
  while (!(file_path_each = file_enum->Next()).empty()) {
    context = NewContext(nullptr);
    base::File::Info file_info;
    FileSystemURL new_url = FileSystemURL::CreateForTest(
        dir.storage_key(), dir.mount_type(), file_path_each);
    if (dir.bucket().has_value())
      new_url.SetBucket(dir.bucket().value());
    EXPECT_EQ(base::File::FILE_OK, ofu()->GetFileInfo(context.get(), new_url,
                                                      &file_info, &file_path));
    EXPECT_EQ(file_info.is_directory, file_enum->IsDirectory());
    EXPECT_EQ(file_info.last_modified, file_enum->LastModifiedTime());
    EXPECT_EQ(file_info.size, file_enum->Size());
    ++count;
  }
  EXPECT_EQ(2, count);
}

TEST_P(ObfuscatedFileUtilTest, TestQuotaOnCopyFile) {
  FileSystemURL from_file(CreateURLFromUTF8("fromfile"));
  FileSystemURL obstacle_file(CreateURLFromUTF8("obstaclefile"));
  FileSystemURL to_file1(CreateURLFromUTF8("tofile1"));
  FileSystemURL to_file2(CreateURLFromUTF8("tofile2"));
  bool created;

  int64_t expected_total_file_size = 0;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(from_file))->context(), from_file,
                &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(obstacle_file))->context(),
                obstacle_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64_t from_file_size = 1020;
  expected_total_file_size += from_file_size;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->Truncate(AllowUsageIncrease(from_file_size)->context(),
                            from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64_t obstacle_file_size = 1;
  expected_total_file_size += obstacle_file_size;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->Truncate(AllowUsageIncrease(obstacle_file_size)->context(),
                            obstacle_file, obstacle_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64_t to_file1_size = from_file_size;
  expected_total_file_size += to_file1_size;
  ASSERT_EQ(
      base::File::FILE_OK,
      ofu()->CopyOrMoveFile(
          AllowUsageIncrease(PathCost(to_file1) + to_file1_size)->context(),
          from_file, to_file1, FileSystemOperation::CopyOrMoveOptionSet(),
          true /* copy */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(
      base::File::FILE_ERROR_NO_SPACE,
      ofu()->CopyOrMoveFile(
          DisallowUsageIncrease(PathCost(to_file2) + from_file_size)->context(),
          from_file, to_file2, FileSystemOperation::CopyOrMoveOptionSet(),
          true /* copy */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64_t old_obstacle_file_size = obstacle_file_size;
  obstacle_file_size = from_file_size;
  expected_total_file_size += obstacle_file_size - old_obstacle_file_size;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->CopyOrMoveFile(
                AllowUsageIncrease(obstacle_file_size - old_obstacle_file_size)
                    ->context(),
                from_file, obstacle_file,
                FileSystemOperation::CopyOrMoveOptionSet(), true /* copy */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64_t old_from_file_size = from_file_size;
  from_file_size = old_from_file_size - 1;
  expected_total_file_size += from_file_size - old_from_file_size;
  ASSERT_EQ(
      base::File::FILE_OK,
      ofu()->Truncate(
          AllowUsageIncrease(from_file_size - old_from_file_size)->context(),
          from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  // quota exceeded
  {
    old_obstacle_file_size = obstacle_file_size;
    obstacle_file_size = from_file_size;
    expected_total_file_size += obstacle_file_size - old_obstacle_file_size;
    std::unique_ptr<UsageVerifyHelper> helper =
        AllowUsageIncrease(obstacle_file_size - old_obstacle_file_size);
    helper->context()->set_allowed_bytes_growth(
        helper->context()->allowed_bytes_growth() - 1);
    ASSERT_EQ(base::File::FILE_OK,
              ofu()->CopyOrMoveFile(helper->context(), from_file, obstacle_file,
                                    FileSystemOperation::CopyOrMoveOptionSet(),
                                    true /* copy */));
    ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());
  }
}

TEST_P(ObfuscatedFileUtilTest, TestQuotaOnMoveFile) {
  FileSystemURL from_file(CreateURLFromUTF8("fromfile"));
  FileSystemURL obstacle_file(CreateURLFromUTF8("obstaclefile"));
  FileSystemURL to_file(CreateURLFromUTF8("tofile"));
  bool created;

  int64_t expected_total_file_size = 0;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(from_file))->context(), from_file,
                &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64_t from_file_size = 1020;
  expected_total_file_size += from_file_size;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->Truncate(AllowUsageIncrease(from_file_size)->context(),
                            from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  from_file_size = 0;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->CopyOrMoveFile(
                AllowUsageIncrease(-PathCost(from_file) + PathCost(to_file))
                    ->context(),
                from_file, to_file, FileSystemOperation::CopyOrMoveOptionSet(),
                false /* move */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(from_file))->context(), from_file,
                &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(obstacle_file))->context(),
                obstacle_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  from_file_size = 1020;
  expected_total_file_size += from_file_size;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->Truncate(AllowUsageIncrease(from_file_size)->context(),
                            from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64_t obstacle_file_size = 1;
  expected_total_file_size += obstacle_file_size;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->Truncate(AllowUsageIncrease(1)->context(), obstacle_file,
                            obstacle_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64_t old_obstacle_file_size = obstacle_file_size;
  obstacle_file_size = from_file_size;
  from_file_size = 0;
  expected_total_file_size -= old_obstacle_file_size;
  ASSERT_EQ(
      base::File::FILE_OK,
      ofu()->CopyOrMoveFile(
          AllowUsageIncrease(-old_obstacle_file_size - PathCost(from_file))
              ->context(),
          from_file, obstacle_file, FileSystemOperation::CopyOrMoveOptionSet(),
          false /* move */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(from_file))->context(), from_file,
                &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  from_file_size = 10;
  expected_total_file_size += from_file_size;
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->Truncate(AllowUsageIncrease(from_file_size)->context(),
                            from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  // quota exceeded even after operation
  old_obstacle_file_size = obstacle_file_size;
  obstacle_file_size = from_file_size;
  from_file_size = 0;
  expected_total_file_size -= old_obstacle_file_size;
  std::unique_ptr<FileSystemOperationContext> context =
      LimitedContext(-old_obstacle_file_size - PathCost(from_file) - 1);
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->CopyOrMoveFile(context.get(), from_file, obstacle_file,
                                  FileSystemOperation::CopyOrMoveOptionSet(),
                                  false /* move */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());
  context.reset();
}

TEST_P(ObfuscatedFileUtilTest, TestQuotaOnRemove) {
  FileSystemURL dir(CreateURLFromUTF8("dir"));
  FileSystemURL file(CreateURLFromUTF8("file"));
  FileSystemURL dfile1(CreateURLFromUTF8("dir/dfile1"));
  FileSystemURL dfile2(CreateURLFromUTF8("dir/dfile2"));
  bool created;

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(file))->context(), file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->CreateDirectory(AllowUsageIncrease(PathCost(dir))->context(),
                                   dir, false, false));
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(
      base::File::FILE_OK,
      ofu()->EnsureFileExists(AllowUsageIncrease(PathCost(dfile1))->context(),
                              dfile1, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(
      base::File::FILE_OK,
      ofu()->EnsureFileExists(AllowUsageIncrease(PathCost(dfile2))->context(),
                              dfile2, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->Truncate(AllowUsageIncrease(340)->context(), file, 340));
  ASSERT_EQ(340, ComputeTotalFileSize());

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->Truncate(AllowUsageIncrease(1020)->context(), dfile1, 1020));
  ASSERT_EQ(1360, ComputeTotalFileSize());

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->Truncate(AllowUsageIncrease(120)->context(), dfile2, 120));
  ASSERT_EQ(1480, ComputeTotalFileSize());

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->DeleteFile(
                AllowUsageIncrease(-PathCost(file) - 340)->context(), file));
  ASSERT_EQ(1140, ComputeTotalFileSize());

  ASSERT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::Remove(file_system_context(), dir,
                                        true /* recursive */));
  ASSERT_EQ(0, ComputeTotalFileSize());
}

TEST_P(ObfuscatedFileUtilTest, TestQuotaOnOpen) {
  FileSystemURL url(CreateURLFromUTF8("file"));

  bool created;
  // Creating a file.
  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(url))->context(), url, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  // Opening it, which shouldn't change the usage.
  ASSERT_EQ(
      base::File::FILE_OK,
      ofu()->EnsureFileExists(AllowUsageIncrease(0)->context(), url, &created));
  ASSERT_EQ(0, ComputeTotalFileSize());

  const int length = 33;
  ASSERT_EQ(
      base::File::FILE_OK,
      ofu()->Truncate(AllowUsageIncrease(length)->context(), url, length));
  ASSERT_EQ(length, ComputeTotalFileSize());

  // TODO(crbug.com/41444071): After CreateOrOpen is modified to return
  // file error instead of file, the in-memory test can proceed through the next
  // steps.
  if (is_incognito())
    return;

  // Opening it with CREATE_ALWAYS flag, which should truncate the file size.
  base::File file = ofu()->CreateOrOpen(
      AllowUsageIncrease(-length)->context(), url,
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  ASSERT_TRUE(file.IsValid());
  ASSERT_EQ(0, ComputeTotalFileSize());
  file.Close();

  // Extending the file again.
  ASSERT_EQ(
      base::File::FILE_OK,
      ofu()->Truncate(AllowUsageIncrease(length)->context(), url, length));
  ASSERT_EQ(length, ComputeTotalFileSize());

  // Opening it with TRUNCATED flag, which should truncate the file size.
  file = ofu()->CreateOrOpen(
      AllowUsageIncrease(-length)->context(), url,
      base::File::FLAG_OPEN_TRUNCATED | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  ASSERT_EQ(0, ComputeTotalFileSize());
  file.Close();
}

TEST_P(ObfuscatedFileUtilTest, MaybeDropDatabasesAliveCase) {
  MaybeDropDatabasesAliveCaseTestBody();
}

TEST_P(ObfuscatedFileUtilTest, MaybeDropDatabasesAlreadyDeletedCase) {
  MaybeDropDatabasesAlreadyDeletedCaseTestBody();
}

TEST_P(ObfuscatedFileUtilTest, DestroyDirectoryDatabase_Isolated) {
  DestroyDirectoryDatabase_IsolatedTestBody();
}

TEST_P(ObfuscatedFileUtilTest, GetDirectoryDatabase_Isolated) {
  GetDirectoryDatabase_IsolatedTestBody();
}

TEST_P(ObfuscatedFileUtilTest, OpenPathInNonDirectory) {
  FileSystemURL url(CreateURLFromUTF8("file"));
  FileSystemURL path_in_file(CreateURLFromUTF8("file/file"));
  bool created;

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(UnlimitedContext().get(), url, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::File::FILE_ERROR_NOT_A_DIRECTORY,
            ofu()->EnsureFileExists(UnlimitedContext().get(), path_in_file,
                                    &created));

  ASSERT_EQ(
      base::File::FILE_ERROR_NOT_A_DIRECTORY,
      ofu()->CreateDirectory(UnlimitedContext().get(), path_in_file,
                             false /* exclusive */, false /* recursive */));
}

TEST_P(ObfuscatedFileUtilTest, CreateDirectory_NotADirectoryInRecursive) {
  FileSystemURL file(CreateURLFromUTF8("file"));
  FileSystemURL path_in_file(CreateURLFromUTF8("file/child"));
  FileSystemURL path_in_file_in_file(
      CreateURLFromUTF8("file/child/grandchild"));
  bool created;

  ASSERT_EQ(base::File::FILE_OK,
            ofu()->EnsureFileExists(UnlimitedContext().get(), file, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(
      base::File::FILE_ERROR_NOT_A_DIRECTORY,
      ofu()->CreateDirectory(UnlimitedContext().get(), path_in_file,
                             false /* exclusive */, true /* recursive */));
  ASSERT_EQ(
      base::File::FILE_ERROR_NOT_A_DIRECTORY,
      ofu()->CreateDirectory(UnlimitedContext().get(), path_in_file_in_file,
                             false /* exclusive */, true /* recursive */));
}

TEST_P(ObfuscatedFileUtilTest, DeleteDirectoryForBucketAndType) {
  // Create directories.
  std::unique_ptr<SandboxFileSystemTestHelper> fs1 =
      NewFileSystem(default_bucket_, kFileSystemTypeTemporary);
  std::unique_ptr<SandboxFileSystemTestHelper> fs2 =
      NewFileSystem(default_bucket_, kFileSystemTypePersistent);
  std::unique_ptr<SandboxFileSystemTestHelper> fs3 =
      NewFileSystem(custom_bucket_, kFileSystemTypeTemporary);
  std::unique_ptr<SandboxFileSystemTestHelper> fs4 =
      NewFileSystem(custom_bucket_, kFileSystemTypePersistent);

  // Make sure directories for default_bucket_ exist.
  ASSERT_TRUE(ofu()
                  ->GetDirectoryForBucketAndType(default_bucket_,
                                                 kFileSystemTypeTemporary,
                                                 /*create=*/false)
                  .has_value());
  ASSERT_TRUE(ofu()
                  ->GetDirectoryForBucketAndType(default_bucket_,
                                                 kFileSystemTypePersistent,
                                                 /*create=*/false)
                  .has_value());

  // Make sure directories for custom_bucket_ exist.
  ASSERT_TRUE(ofu()
                  ->GetDirectoryForBucketAndType(custom_bucket_,
                                                 kFileSystemTypeTemporary,
                                                 /*create=*/false)
                  .has_value());
  ASSERT_TRUE(ofu()
                  ->GetDirectoryForBucketAndType(custom_bucket_,
                                                 kFileSystemTypePersistent,
                                                 /*create=*/false)
                  .has_value());

  // Delete a directory for default_bucket_'s persistent filesystem.
  ASSERT_TRUE(ofu()->DeleteDirectoryForBucketAndType(
      default_bucket_, kFileSystemTypePersistent));

  // The directory for default_bucket_'s temporary filesystem should not be
  // removed.
  ASSERT_TRUE(ofu()
                  ->GetDirectoryForBucketAndType(default_bucket_,
                                                 kFileSystemTypeTemporary,
                                                 /*create=*/false)
                  .has_value());

  // The directory for default_bucket_'s persistent filesystem should be
  // removed.
  ASSERT_THAT(ofu()->GetDirectoryForBucketAndType(default_bucket_,
                                                  kFileSystemTypePersistent,
                                                  /*create=*/false),
              base::test::ErrorIs(base::File::FILE_ERROR_NOT_FOUND));

  // The directories for custom_bucket_ should not be removed.
  ASSERT_TRUE(ofu()
                  ->GetDirectoryForBucketAndType(custom_bucket_,
                                                 kFileSystemTypeTemporary,
                                                 /*create=*/false)
                  .has_value());
  ASSERT_TRUE(ofu()
                  ->GetDirectoryForBucketAndType(custom_bucket_,
                                                 kFileSystemTypePersistent,
                                                 /*create=*/false)
                  .has_value());

  // Deleting directories which don't exist is not an error.
  ASSERT_TRUE(ofu()->DeleteDirectoryForBucketAndType(
      alternate_custom_bucket_, kFileSystemTypePersistent));
}

TEST_P(ObfuscatedFileUtilTest, DeleteDirectoryForBucketAndType_DeleteAll) {
  // Create origin directories.
  std::unique_ptr<SandboxFileSystemTestHelper> fs1 =
      NewFileSystem(default_bucket_, kFileSystemTypeTemporary);
  std::unique_ptr<SandboxFileSystemTestHelper> fs2 =
      NewFileSystem(default_bucket_, kFileSystemTypePersistent);
  std::unique_ptr<SandboxFileSystemTestHelper> fs3 =
      NewFileSystem(custom_bucket_, kFileSystemTypeTemporary);
  std::unique_ptr<SandboxFileSystemTestHelper> fs4 =
      NewFileSystem(custom_bucket_, kFileSystemTypePersistent);

  // Make sure directories for default_bucket_ exist.
  ASSERT_TRUE(ofu()
                  ->GetDirectoryForBucketAndType(default_bucket_,
                                                 kFileSystemTypeTemporary,
                                                 /*create=*/false)
                  .has_value());
  ASSERT_TRUE(ofu()
                  ->GetDirectoryForBucketAndType(default_bucket_,
                                                 kFileSystemTypePersistent,
                                                 /*create=*/false)
                  .has_value());

  // Make sure directories for custom_bucket_ exist.
  ASSERT_TRUE(ofu()
                  ->GetDirectoryForBucketAndType(custom_bucket_,
                                                 kFileSystemTypeTemporary,
                                                 /*create=*/false)
                  .has_value());
  ASSERT_TRUE(ofu()
                  ->GetDirectoryForBucketAndType(custom_bucket_,
                                                 kFileSystemTypePersistent,
                                                 /*create=*/false)
                  .has_value());

  // Delete all directories for default_bucket_.
  ofu()->DeleteDirectoryForBucketAndType(default_bucket_, std::nullopt);

  // The directories for default_bucket_ should be removed.
  ASSERT_THAT(ofu()->GetDirectoryForBucketAndType(default_bucket_,
                                                  kFileSystemTypeTemporary,
                                                  /*create=*/false),
              base::test::ErrorIs(base::File::FILE_ERROR_NOT_FOUND));
  ASSERT_THAT(ofu()->GetDirectoryForBucketAndType(default_bucket_,
                                                  kFileSystemTypePersistent,
                                                  /*create=*/false),
              base::test::ErrorIs(base::File::FILE_ERROR_NOT_FOUND));

  // The directories for custom_bucket_ should not be removed.
  ASSERT_TRUE(ofu()
                  ->GetDirectoryForBucketAndType(custom_bucket_,
                                                 kFileSystemTypeTemporary,
                                                 /*create=*/false)
                  .has_value());
  ASSERT_TRUE(ofu()
                  ->GetDirectoryForBucketAndType(custom_bucket_,
                                                 kFileSystemTypePersistent,
                                                 /*create=*/false)
                  .has_value());
}

}  // namespace storage
