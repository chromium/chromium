// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_grant.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/test/test_web_contents.h"
#include "file_system_access_directory_handle_impl.h"
#include "mock_file_system_access_permission_context.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/content_uri_utils.h"
#include "base/android/path_utils.h"
#include "base/strings/escape.h"
#include "base/test/android/content_uri_test_utils.h"
#endif

namespace content {

using blink::mojom::FileSystemAccessStatus;
using blink::mojom::PermissionStatus;
using storage::FileSystemURL;
using testing::_;
using testing::FieldsAre;

namespace {
struct WriteModeTestParams {
  const char* test_name_suffix;
  bool is_feature_enabled;
};

constexpr WriteModeTestParams kTestParams[] = {
    {"WriteModeDisabled", false},
    {"WriteModeEnabled", true},
};
}  // namespace

// A matcher to check if a `RequestPermission()` call is successful and
// returns the expected permission status.
MATCHER_P(IsOkAndPermissionStatus, status, "") {
  if (arg.first->status != FileSystemAccessStatus::kOk) {
    *result_listener << "FileSystemAccessStatus is " << arg.first->status;
    return false;
  }
  if (arg.second != status) {
    *result_listener << "PermissionStatus is " << arg.second;
    return false;
  }
  return true;
}

// A matcher to check the result of a `CreateFileWriter()` call.
MATCHER_P(FileWriterCreationIs, expected_status, "") {
  const auto& result = arg.first;
  const auto& writer = arg.second;

  if (result->status != expected_status) {
    *result_listener << "FileSystemAccessStatus is " << result->status
                     << ", expected " << expected_status;
    return false;
  }

  bool should_be_valid = (expected_status == FileSystemAccessStatus::kOk);
  if (writer.is_valid() != should_be_valid) {
    *result_listener << "writer validity is " << writer.is_valid()
                     << ", expected " << should_be_valid;
    return false;
  }
  return true;
}

// Test fixture for FileSystemAccessFileHandleImpl.
//
// The permission context in this fixture is always null, so the relevant
// permission checks are skipped.
//
// Tests with mock permission context are in
// FileSystemAccessFileHandleImplMovePermissionsTest.
class FileSystemAccessFileHandleImplTestBase : public testing::Test {
 public:
  FileSystemAccessFileHandleImplTestBase() = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    SetupHelper(storage::kFileSystemTypeTest, /*is_incognito=*/false,
                /*use_content_uri=*/false);
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  std::string ReadFile(const FileSystemURL& url) {
    std::unique_ptr<storage::FileStreamReader> reader =
        file_system_context_->CreateFileStreamReader(
            url, 0, std::numeric_limits<int64_t>::max(), base::Time());
    std::string result;
    while (true) {
      auto buf = base::MakeRefCounted<net::IOBufferWithSize>(4096);
      net::TestCompletionCallback callback;
      int rv = reader->Read(buf.get(), buf->size(), callback.callback());
      if (rv == net::ERR_IO_PENDING) {
        rv = callback.WaitForResult();
      }
      EXPECT_GE(rv, 0);
      if (rv < 0) {
        return "(read failure)";
      }
      if (rv == 0) {
        return result;
      }
      result.append(buf->data(), rv);
    }
  }

  std::unique_ptr<FileSystemAccessFileHandleImpl> GetHandleWithPermissions(
      const base::FilePath& path,
      scoped_refptr<FixedFileSystemAccessPermissionGrant> read_grant,
      scoped_refptr<FixedFileSystemAccessPermissionGrant> write_grant) {
    auto url = manager_->CreateFileSystemURLFromPath(PathInfo(path));
    auto handle = std::make_unique<FileSystemAccessFileHandleImpl>(
        manager_.get(),
        FileSystemAccessManagerImpl::BindingContext(
            test_src_storage_key_, test_src_url_,
            web_contents_->GetPrimaryMainFrame()->GetGlobalId()),
        url, path.BaseName().AsUTF8Unsafe(),
        FileSystemAccessManagerImpl::SharedHandleState(std::move(read_grant),
                                                       std::move(write_grant)));
    return handle;
  }

  std::unique_ptr<FileSystemAccessDirectoryHandleImpl>
  GetDirectoryHandleWithPermissions(
      const base::FilePath& path,
      scoped_refptr<FixedFileSystemAccessPermissionGrant> read_grant,
      scoped_refptr<FixedFileSystemAccessPermissionGrant> write_grant) {
    auto url = manager_->CreateFileSystemURLFromPath(PathInfo(path));
    auto handle = std::make_unique<FileSystemAccessDirectoryHandleImpl>(
        manager_.get(),
        FileSystemAccessManagerImpl::BindingContext(
            test_src_storage_key_, test_src_url_,
            web_contents_->GetPrimaryMainFrame()->GetGlobalId()),
        url,
        FileSystemAccessManagerImpl::SharedHandleState(std::move(read_grant),
                                                       std::move(write_grant)));
    return handle;
  }

  storage::QuotaErrorOr<storage::BucketLocator> CreateBucketForTesting() {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>>
        bucket_future;
    quota_manager_proxy_->CreateBucketForTesting(
        test_src_storage_key_, "custom_bucket",
        base::SequencedTaskRunner::GetCurrentDefault(),
        bucket_future.GetCallback());
    return bucket_future.Take().transform(
        &storage::BucketInfo::ToBucketLocator);
  }

 protected:
  void SetupHelper(storage::FileSystemType type,
                   bool is_incognito,
                   bool use_content_uri = false) {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
#if BUILDFLAG(IS_WIN)
    // Convert path to long format to avoid mixing long and 8.3 formats in test.
    ASSERT_TRUE(dir_.Set(base::MakeLongFilePath(dir_.Take())));
#endif  // BUILDFLAG(IS_WIN)

    web_contents_ = web_contents_factory_.CreateWebContents(&browser_context_);
    static_cast<TestWebContents*>(web_contents_)
        ->NavigateAndCommit(test_src_url_);

    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        is_incognito, dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>());
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    if (is_incognito) {
      file_system_context_ =
          storage::CreateIncognitoFileSystemContextForTesting(
              base::SingleThreadTaskRunner::GetCurrentDefault(),
              base::SingleThreadTaskRunner::GetCurrentDefault(),
              quota_manager_proxy_.get(), dir_.GetPath());
    } else {
      file_system_context_ = storage::CreateFileSystemContextForTesting(
          quota_manager_proxy_.get(), dir_.GetPath());
    }

    // Use an absolute path for local files, or a relative path otherwise,
    auto test_file_path = type == storage::kFileSystemTypeLocal
                              ? dir_.GetPath().AppendASCII("test")
                              : base::FilePath::FromUTF8Unsafe("test");
#if BUILDFLAG(IS_ANDROID)
    if (use_content_uri) {
      base::FilePath parent =
          *base::test::android::GetInMemoryContentTreeUriFromCacheDirDirectory(
              dir_.GetPath());
      base::FilePath content_uri = base::ContentUriGetChildDocumentOrQuery(
          parent, test_file_path.BaseName().value(), "text/plain",
          /*is_directory=*/false,
          /*create=*/true);
      ASSERT_TRUE(base::ContentUriIsCreateChildDocumentQuery(content_uri));
      test_file_path = content_uri;
    }
#endif
    test_file_url_ = file_system_context_->CreateCrackedFileSystemURL(
        test_src_storage_key_, type, test_file_path);
    if (type == storage::kFileSystemTypeTemporary) {
      ASSERT_OK_AND_ASSIGN(auto bucket, CreateBucketForTesting());
      test_file_url_.SetBucket(std::move(bucket));
    }

    ASSERT_EQ(base::File::FILE_OK,
              storage::AsyncFileTestHelper::CreateFile(
                  file_system_context_.get(), test_file_url_));

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/nullptr,
        /*off_the_record=*/is_incognito);

    handle_ = std::make_unique<FileSystemAccessFileHandleImpl>(
        manager_.get(),
        FileSystemAccessManagerImpl::BindingContext(
            test_src_storage_key_, test_src_url_,
            web_contents_->GetPrimaryMainFrame()->GetGlobalId()),
        test_file_url_, "test",
        FileSystemAccessManagerImpl::SharedHandleState(allow_grant_,
                                                       allow_grant_));
  }

  const GURL test_src_url_ = GURL("http://example.com/foo");
  const blink::StorageKey test_src_storage_key_ =
      blink::StorageKey::CreateFromStringForTesting("http://example.com/foo");

  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  base::ScopedTempDir dir_;

  TestBrowserContext browser_context_;
  TestWebContentsFactory web_contents_factory_;

  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  testing::NiceMock<MockFileSystemAccessPermissionContext> permission_context_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;

  raw_ptr<WebContents> web_contents_ = nullptr;

  FileSystemURL test_file_url_;

  scoped_refptr<FixedFileSystemAccessPermissionGrant> allow_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
          PathInfo());
  scoped_refptr<FixedFileSystemAccessPermissionGrant> ask_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::ASK,
          PathInfo());
  scoped_refptr<FixedFileSystemAccessPermissionGrant> deny_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::DENIED,
          PathInfo());
  std::unique_ptr<FileSystemAccessFileHandleImpl> handle_;

  base::test::ScopedFeatureList scoped_feature_list;
};

class FileSystemAccessAccessHandleTest
    : public FileSystemAccessFileHandleImplTestBase {
 public:
  void SetUp() override {
    // AccessHandles are only allowed for temporary file systems.
    SetupHelper(storage::kFileSystemTypeTemporary, /*is_incognito=*/false,
                /*use_content_uri=*/false);
  }

  std::tuple<
      blink::mojom::FileSystemAccessErrorPtr,
      blink::mojom::FileSystemAccessAccessHandleFilePtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>>
  OpenAccessHandle(
      blink::mojom::FileSystemAccessAccessHandleLockMode lock_mode) {
    base::test::TestFuture<
        blink::mojom::FileSystemAccessErrorPtr,
        blink::mojom::FileSystemAccessAccessHandleFilePtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>>
        future;
    handle_->OpenAccessHandle(lock_mode, future.GetCallback());
    return future.Take();
  }

  FileSystemAccessStatus TestLockMode(
      blink::mojom::FileSystemAccessAccessHandleLockMode lock_mode) {
    return std::get<blink::mojom::FileSystemAccessErrorPtr>(
               OpenAccessHandle(lock_mode))
        ->status;
  }
};

class FileSystemAccessAccessHandleIncognitoTest
    : public FileSystemAccessAccessHandleTest {
  void SetUp() override {
    // AccessHandles are only allowed for temporary file systems.
    SetupHelper(storage::kFileSystemTypeTemporary, /*is_incognito=*/true,
                /*use_content_uri=*/false);
  }
};

#if BUILDFLAG(IS_ANDROID)
class FileSystemAccessAccessHandleContentUriTest
    : public FileSystemAccessAccessHandleTest {
  void SetUp() override {
    // AccessHandles are only allowed for temporary file systems.
    SetupHelper(storage::kFileSystemTypeLocal, /*is_incognito=*/false,
                /*use_content_uri=*/true);
  }
};
#endif

class FileSystemAccessFileHandleImplCreateFileWriterTest
    : public FileSystemAccessFileHandleImplTestBase {
 public:
  // Creates a file writer and waits for the operation to complete. Returns a
  // pair containing the error and the writer remote.
  std::pair<blink::mojom::FileSystemAccessErrorPtr,
            mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
  CreateFileWriter(
      bool keep_existing_data,
      bool auto_close,
      blink::mojom::FileSystemAccessWritableFileStreamLockMode lock_mode) {
    base::test::TestFuture<
        blink::mojom::FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
        future;
    handle_->CreateFileWriter(keep_existing_data, auto_close, lock_mode,
                              future.GetCallback());
    return future.Take();
  }
};

TEST_F(FileSystemAccessFileHandleImplCreateFileWriterTest, OverLimitNotOK) {
  int max_files = 5;
  handle_->set_max_swap_files_for_testing(max_files);

  const FileSystemURL base_swap_url =
      file_system_context_->CreateCrackedFileSystemURL(
          test_src_storage_key_, storage::kFileSystemTypeTest,
          base::FilePath::FromUTF8Unsafe("test.crswap"));

  std::vector<mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      writers;
  for (int i = 0; i < max_files; i++) {
    FileSystemURL swap_url;
    if (i == 0) {
      swap_url = base_swap_url;
    } else {
      swap_url = file_system_context_->CreateCrackedFileSystemURL(
          test_src_storage_key_, storage::kFileSystemTypeTest,
          base::FilePath::FromUTF8Unsafe(
              base::StringPrintf("test.%d.crswap", i)));
    }

    auto writer_pair = CreateFileWriter(
        /*keep_existing_data=*/false,
        /*auto_close=*/false,
        blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed);
    ASSERT_THAT(writer_pair, FileWriterCreationIs(FileSystemAccessStatus::kOk));
    EXPECT_EQ("", ReadFile(swap_url));
    writers.push_back(std::move(writer_pair.second));
  }

  EXPECT_THAT(
      CreateFileWriter(
          /*keep_existing_data=*/false,
          /*auto_close=*/false,
          blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed),
      FileWriterCreationIs(FileSystemAccessStatus::kOperationFailed));
}

TEST_F(FileSystemAccessFileHandleImplCreateFileWriterTest, SiloedMode) {
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer;

  // Keep the writer alive for the duration of the test.
  auto writer_pair = CreateFileWriter(
      /*keep_existing_data=*/false,
      /*auto_close=*/false,
      blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed);
  ASSERT_THAT(writer_pair, FileWriterCreationIs(FileSystemAccessStatus::kOk));
  writer = std::move(writer_pair.second);

  // If file writer in siloed mode exists for a file handle, can create a writer
  // in siloed mode for the file handle.
  EXPECT_THAT(
      CreateFileWriter(
          /*keep_existing_data=*/false,
          /*auto_close=*/false,
          blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed),
      FileWriterCreationIs(FileSystemAccessStatus::kOk));

  // If file writer in siloed mode exists for a file handle, can't create a
  // writer in exclusive mode for the file handle.
  EXPECT_THAT(
      CreateFileWriter(
          /*keep_existing_data=*/false,
          /*auto_close=*/false,
          blink::mojom::FileSystemAccessWritableFileStreamLockMode::kExclusive),
      FileWriterCreationIs(
          FileSystemAccessStatus::kNoModificationAllowedError));
}

TEST_F(FileSystemAccessFileHandleImplCreateFileWriterTest, ExclusiveMode) {
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer;
  auto writer_pair = CreateFileWriter(
      /*keep_existing_data=*/false,
      /*auto_close=*/false,
      blink::mojom::FileSystemAccessWritableFileStreamLockMode::kExclusive);
  ASSERT_THAT(writer_pair, FileWriterCreationIs(FileSystemAccessStatus::kOk));
  writer = std::move(writer_pair.second);

  // If file writer in exclusive mode exists for a file handle, can't create a
  // writer in siloed mode for the file handle.
  EXPECT_THAT(
      CreateFileWriter(
          /*keep_existing_data=*/false,
          /*auto_close=*/false,
          blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed),
      FileWriterCreationIs(
          FileSystemAccessStatus::kNoModificationAllowedError));

  // If file writer in exclusive mode exists for a file handle, can't create a
  // writer in exclusive mode for the file handle.
  EXPECT_THAT(
      CreateFileWriter(
          /*keep_existing_data=*/false,
          /*auto_close=*/false,
          blink::mojom::FileSystemAccessWritableFileStreamLockMode::kExclusive),
      FileWriterCreationIs(
          FileSystemAccessStatus::kNoModificationAllowedError));
}

TEST_F(FileSystemAccessFileHandleImplCreateFileWriterTest,
       WithExistingSwapFile) {
  const FileSystemURL swap_url =
      file_system_context_->CreateCrackedFileSystemURL(
          test_src_storage_key_, storage::kFileSystemTypeTest,
          base::FilePath::FromUTF8Unsafe("test.crswap"));

  // Create pre-existing swap file.
  ASSERT_EQ(base::File::FILE_OK, storage::AsyncFileTestHelper::CreateFile(
                                     file_system_context_.get(), swap_url));

  // Creating the writer still succeeds.
  EXPECT_THAT(
      CreateFileWriter(
          /*keep_existing_data=*/true,
          /*auto_close=*/false,
          blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed),
      FileWriterCreationIs(FileSystemAccessStatus::kOk));
}

// Verifies that `auto_close` is plumbed through to the FileSystemAccessManager.
TEST_F(FileSystemAccessFileHandleImplCreateFileWriterTest, WithAutoClose) {
  EXPECT_THAT(
      CreateFileWriter(
          /*keep_existing_data=*/true,
          /*auto_close=*/true,
          blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed),
      FileWriterCreationIs(FileSystemAccessStatus::kOk));
}

// TODO(crbug.com/40276567): Add test to cover that swap file is truncated when
// `keep_existing_data` is false.

#if BUILDFLAG(IS_ANDROID)
TEST_F(FileSystemAccessAccessHandleContentUriTest, CreateFileWriter) {
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      future;
  handle_->CreateFileWriter(
      /*keep_existing_data=*/false,
      /*auto_close=*/false,
      blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed,
      future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer_remote;
  std::tie(result, writer_remote) = future.Take();
  EXPECT_EQ(result->status, FileSystemAccessStatus::kOk);
  EXPECT_TRUE(writer_remote.is_valid());

  // Swap file should be created in cache dir.
  base::FilePath cache_dir;
  EXPECT_TRUE(base::android::GetCacheDirectory(&cache_dir));
  auto escaped = base::EscapeAllExceptUnreserved(test_file_url_.path().value());
  base::FilePath swap = cache_dir.Append("FileSystemAPISwap")
                            .Append(escaped)
                            .AddExtension(".crswap");
  EXPECT_TRUE(base::PathExists(swap));
}
#endif

class FileSystemAccessFileHandleImplRemoveTest
    : public FileSystemAccessFileHandleImplTestBase {};

TEST_F(FileSystemAccessFileHandleImplRemoveTest, NoWriteAccess) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));

  auto handle = GetHandleWithPermissions(file, /*read_grant=*/allow_grant_,
                                         /*write_grant=*/deny_grant_);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Remove(future.GetCallback());
  EXPECT_EQ(future.Get()->status, FileSystemAccessStatus::kPermissionDenied);
  EXPECT_TRUE(base::PathExists(file));
}

TEST_F(FileSystemAccessFileHandleImplRemoveTest, HasWriteAccess) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));

  auto handle = GetHandleWithPermissions(file, /*read_grant=*/allow_grant_,
                                         /*write_grant=*/allow_grant_);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Remove(future.GetCallback());
  EXPECT_EQ(future.Get()->status, FileSystemAccessStatus::kOk);
  EXPECT_FALSE(base::PathExists(file));
}

TEST_F(FileSystemAccessAccessHandleTest, OpenAccessHandle) {
  blink::mojom::FileSystemAccessErrorPtr result;
  blink::mojom::FileSystemAccessAccessHandleFilePtr file;
  mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
      access_handle_remote;
  std::tie(result, file, access_handle_remote) = OpenAccessHandle(
      blink::mojom::FileSystemAccessAccessHandleLockMode::kReadwrite);
  EXPECT_EQ(result->status, FileSystemAccessStatus::kOk);
  // File should be valid and no incognito remote is needed.
  EXPECT_TRUE(file->is_regular_file());
  blink::mojom::FileSystemAccessRegularFilePtr regular_file =
      std::move(file->get_regular_file());
  EXPECT_TRUE(regular_file->os_file.IsValid());
  EXPECT_EQ(regular_file->file_size, 0);
  EXPECT_TRUE(regular_file->file_modification_host.is_valid());
  EXPECT_TRUE(access_handle_remote.is_valid());
}

TEST_F(FileSystemAccessAccessHandleIncognitoTest, OpenAccessHandle) {
  blink::mojom::FileSystemAccessErrorPtr result;
  blink::mojom::FileSystemAccessAccessHandleFilePtr file;
  mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
      access_handle_remote;
  std::tie(result, file, access_handle_remote) = OpenAccessHandle(
      blink::mojom::FileSystemAccessAccessHandleLockMode::kReadwrite);
  EXPECT_EQ(result->status, FileSystemAccessStatus::kOk);
  // Incognito remote should be valid and no file is needed.
  EXPECT_TRUE(file->is_incognito_file_delegate());
  EXPECT_TRUE(file->get_incognito_file_delegate().is_valid());
  EXPECT_TRUE(access_handle_remote.is_valid());
}

TEST_F(FileSystemAccessAccessHandleTest, OpenAccessHandleLockModes_Readwrite) {
  // For a file with an open access handle in READWRITE mode, no other access
  // handles can be open.
  mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
      access_handle_remote;

  blink::mojom::FileSystemAccessErrorPtr result;
  blink::mojom::FileSystemAccessAccessHandleFilePtr file;
  std::tie(result, file, access_handle_remote) = OpenAccessHandle(
      blink::mojom::FileSystemAccessAccessHandleLockMode::kReadwrite);
  EXPECT_EQ(result->status, FileSystemAccessStatus::kOk);

  // Cannot open another access handle in READWRITE mode.
  EXPECT_EQ(TestLockMode(
                blink::mojom::FileSystemAccessAccessHandleLockMode::kReadwrite),
            FileSystemAccessStatus::kNoModificationAllowedError);

  // Cannot open another access handle in a different mode.
  EXPECT_EQ(TestLockMode(
                blink::mojom::FileSystemAccessAccessHandleLockMode::kReadOnly),
            FileSystemAccessStatus::kNoModificationAllowedError);
  EXPECT_EQ(
      TestLockMode(
          blink::mojom::FileSystemAccessAccessHandleLockMode::kReadwriteUnsafe),
      FileSystemAccessStatus::kNoModificationAllowedError);
}

TEST_F(FileSystemAccessAccessHandleTest, OpenAccessHandleLockModes_ReadOnly) {
  // For a file with an open access handle in READ_ONLY mode, only access
  // handles in READ_ONLY mode may be open.
  mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
      access_handle_remote;

  blink::mojom::FileSystemAccessErrorPtr result;
  blink::mojom::FileSystemAccessAccessHandleFilePtr file;
  std::tie(result, file, access_handle_remote) = OpenAccessHandle(
      blink::mojom::FileSystemAccessAccessHandleLockMode::kReadOnly);
  EXPECT_EQ(result->status, FileSystemAccessStatus::kOk);

  // Can open another access handle in READ_ONLY mode.
  EXPECT_EQ(TestLockMode(
                blink::mojom::FileSystemAccessAccessHandleLockMode::kReadOnly),
            FileSystemAccessStatus::kOk);

  // Cannot open another access handle in a different mode.
  EXPECT_EQ(TestLockMode(
                blink::mojom::FileSystemAccessAccessHandleLockMode::kReadwrite),
            FileSystemAccessStatus::kNoModificationAllowedError);
  EXPECT_EQ(
      TestLockMode(
          blink::mojom::FileSystemAccessAccessHandleLockMode::kReadwriteUnsafe),
      FileSystemAccessStatus::kNoModificationAllowedError);
}

TEST_F(FileSystemAccessAccessHandleTest,
       OpenAccessHandleLockModes_ReadwriteUnsafe) {
  // For a file with an open access handle in READWRITE_UNSAFE mode, only access
  // handles in READWRITE_UNSAFE mode may be open.
  mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
      access_handle_remote;

  blink::mojom::FileSystemAccessErrorPtr result;
  blink::mojom::FileSystemAccessAccessHandleFilePtr file;
  std::tie(result, file, access_handle_remote) = OpenAccessHandle(
      blink::mojom::FileSystemAccessAccessHandleLockMode::kReadwriteUnsafe);
  EXPECT_EQ(result->status, FileSystemAccessStatus::kOk);

  // Can open another access handle in READ_ONLY mode.
  EXPECT_EQ(
      TestLockMode(
          blink::mojom::FileSystemAccessAccessHandleLockMode::kReadwriteUnsafe),
      FileSystemAccessStatus::kOk);

  // Cannot open another access handle in a different mode.
  EXPECT_EQ(TestLockMode(
                blink::mojom::FileSystemAccessAccessHandleLockMode::kReadwrite),
            FileSystemAccessStatus::kNoModificationAllowedError);
  EXPECT_EQ(TestLockMode(
                blink::mojom::FileSystemAccessAccessHandleLockMode::kReadOnly),
            FileSystemAccessStatus::kNoModificationAllowedError);
}

class FileSystemAccessFileHandleImplRenameTest
    : public FileSystemAccessFileHandleImplTestBase {};

TEST_F(FileSystemAccessFileHandleImplRenameTest, NoWriteAccess) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));
  base::FilePath renamed_file = file.DirName().AppendASCII("new_name.txt");

  auto handle = GetHandleWithPermissions(file, /*read_grant=*/allow_grant_,
                                         /*write_grant=*/ask_grant_);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Rename(renamed_file.BaseName().AsUTF8Unsafe(), future.GetCallback());
  EXPECT_EQ(future.Get()->status, FileSystemAccessStatus::kPermissionDenied);
  EXPECT_TRUE(base::PathExists(file));
  EXPECT_FALSE(base::PathExists(renamed_file));
}

TEST_F(FileSystemAccessFileHandleImplRenameTest, HasWriteAccess) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));
  base::FilePath renamed_file = file.DirName().AppendASCII("new_name.txt");

  auto handle = GetHandleWithPermissions(file, /*read_grant=*/allow_grant_,
                                         /*write_grant=*/allow_grant_);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Rename(renamed_file.BaseName().AsUTF8Unsafe(), future.GetCallback());
  EXPECT_EQ(future.Get()->status, FileSystemAccessStatus::kOk);
  EXPECT_FALSE(base::PathExists(file));
  EXPECT_TRUE(base::PathExists(renamed_file));
}

class FileSystemAccessFileHandleImplMoveTest
    : public FileSystemAccessFileHandleImplTestBase {};

TEST_F(FileSystemAccessFileHandleImplMoveTest, NoWriteAccess) {
  base::FilePath dest_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      dir_.GetPath(), FILE_PATH_LITERAL("dest"), &dest_dir));
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));
  base::FilePath renamed_file = dest_dir.AppendASCII("new_name.txt");

  auto dest_dir_handle =
      GetDirectoryHandleWithPermissions(dest_dir, /*read_grant=*/allow_grant_,
                                        /*write_grant=*/allow_grant_);
  auto handle = GetHandleWithPermissions(file, /*read_grant=*/allow_grant_,
                                         /*write_grant=*/deny_grant_);

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> dir_remote;
  manager_->CreateTransferToken(*dest_dir_handle,
                                dir_remote.InitWithNewPipeAndPassReceiver());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Move(std::move(dir_remote), renamed_file.BaseName().AsUTF8Unsafe(),
               future.GetCallback());
  EXPECT_EQ(future.Get()->status, FileSystemAccessStatus::kPermissionDenied);
  EXPECT_TRUE(base::PathExists(file));
  EXPECT_FALSE(base::PathExists(renamed_file));
}

TEST_F(FileSystemAccessFileHandleImplMoveTest, NoDestWriteAccess) {
  base::FilePath dest_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      dir_.GetPath(), FILE_PATH_LITERAL("dest"), &dest_dir));
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));
  base::FilePath renamed_file = dest_dir.AppendASCII("new_name.txt");

  auto dest_dir_handle =
      GetDirectoryHandleWithPermissions(dest_dir, /*read_grant=*/allow_grant_,
                                        /*write_grant=*/deny_grant_);
  auto handle = GetHandleWithPermissions(file, /*read_grant=*/allow_grant_,
                                         /*write_grant=*/allow_grant_);

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> dir_remote;
  manager_->CreateTransferToken(*dest_dir_handle,
                                dir_remote.InitWithNewPipeAndPassReceiver());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Move(std::move(dir_remote), renamed_file.BaseName().AsUTF8Unsafe(),
               future.GetCallback());
  EXPECT_EQ(future.Get()->status, FileSystemAccessStatus::kPermissionDenied);
  EXPECT_TRUE(base::PathExists(file));
  EXPECT_FALSE(base::PathExists(renamed_file));
}

TEST_F(FileSystemAccessFileHandleImplMoveTest, HasDestWriteAccess) {
  base::FilePath dest_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      dir_.GetPath(), FILE_PATH_LITERAL("dest"), &dest_dir));
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));
  base::FilePath renamed_file = dest_dir.AppendASCII("new_name.txt");

  auto dest_dir_handle =
      GetDirectoryHandleWithPermissions(dest_dir, /*read_grant=*/allow_grant_,
                                        /*write_grant=*/allow_grant_);
  auto handle = GetHandleWithPermissions(file, /*read_grant=*/allow_grant_,
                                         /*write_grant=*/allow_grant_);

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> dir_remote;
  manager_->CreateTransferToken(*dest_dir_handle,
                                dir_remote.InitWithNewPipeAndPassReceiver());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Move(std::move(dir_remote), renamed_file.BaseName().AsUTF8Unsafe(),
               future.GetCallback());
  EXPECT_EQ(future.Get()->status, FileSystemAccessStatus::kOk);
  EXPECT_FALSE(base::PathExists(file));
  EXPECT_TRUE(base::PathExists(renamed_file));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(FileSystemAccessFileHandleImplMoveTest,
       ContentUriRenameMoveNotSupported) {
  base::FilePath dest_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      dir_.GetPath(), FILE_PATH_LITERAL("dest"), &dest_dir));
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));
  base::FilePath renamed_file = file.DirName().AppendASCII("new_name.txt");
  base::FilePath moved_file = dest_dir.AppendASCII("new_name.txt");

  base::FilePath content_uri_dest_dir =
      *base::test::android::GetContentUriFromCacheDirFilePath(dest_dir);
  base::FilePath content_uri_file =
      *base::test::android::GetContentUriFromCacheDirFilePath(file);

  auto dest_dir_handle = GetDirectoryHandleWithPermissions(
      content_uri_dest_dir, /*read_grant=*/allow_grant_,
      /*write_grant=*/allow_grant_);
  auto handle =
      GetHandleWithPermissions(content_uri_file, /*read_grant=*/allow_grant_,
                               /*write_grant=*/allow_grant_);

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> dir_remote;
  manager_->CreateTransferToken(*dest_dir_handle,
                                dir_remote.InitWithNewPipeAndPassReceiver());

  // Rename is not supported.
  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> rename_future;
  handle->Rename(renamed_file.BaseName().AsUTF8Unsafe(),
                 rename_future.GetCallback());
  EXPECT_EQ(rename_future.Get()->status,
            FileSystemAccessStatus::kInvalidModificationError);
  EXPECT_TRUE(base::PathExists(file));
  EXPECT_FALSE(base::PathExists(renamed_file));

  // Move is not supported.
  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> move_future;
  handle->Move(std::move(dir_remote), moved_file.BaseName().AsUTF8Unsafe(),
               move_future.GetCallback());
  EXPECT_EQ(move_future.Get()->status,
            FileSystemAccessStatus::kInvalidModificationError);
  EXPECT_TRUE(base::PathExists(file));
  EXPECT_FALSE(base::PathExists(moved_file));
}
#endif

#if BUILDFLAG(IS_MAC)
// Tests that swap file cloning (i.e. creating a swap file using underlying
// platform support for copy-on-write files) behaves as expected. Swap file
// cloning requires storage::kFileSystemTypeLocal.
class FileSystemAccessFileHandleSwapFileCloningTest
    : public FileSystemAccessFileHandleImplTestBase {
 public:
  enum class CloneFileResult {
    kDidNotAttempt,
    kAttemptedAndAborted,
    kAttemptedAndCompletedUnexpectedly,
    kAttemptedAndCompletedAsExpected
  };

  void SetUp() override {
    SetupHelper(storage::kFileSystemTypeLocal, /*is_incognito=*/false,
                /*use_content_uri=*/false);
  }

  CloneFileResult GetCloneFileResult(
      const std::unique_ptr<FileSystemAccessFileHandleImpl>& handle) {
    auto maybe_clone_result = handle->get_swap_file_clone_result_for_testing();

    if (!maybe_clone_result.has_value()) {
      return CloneFileResult::kDidNotAttempt;
    }

    if (maybe_clone_result.value() == base::File::Error::FILE_ERROR_ABORT) {
      return CloneFileResult::kAttemptedAndAborted;
    }

    // We should not attempt to clone the file if the swap file exists. Other
    // errors are okay.
    if (maybe_clone_result.value() == base::File::Error::FILE_ERROR_EXISTS) {
      return CloneFileResult::kAttemptedAndCompletedUnexpectedly;
    }

    // Ideally we could just check that the result is FILE_OK, but
    // clonefile() may spuriously fail. See https://crbug.com/1439179. For the
    // purposes of these tests, we'll consider these spurious errors as
    // "expected".
    return CloneFileResult::kAttemptedAndCompletedAsExpected;
  }
};

TEST_F(FileSystemAccessFileHandleSwapFileCloningTest, BasicClone) {
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      future;
  handle_->CreateFileWriter(
      /*keep_existing_data=*/true,
      /*auto_close=*/false,
      blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed,
      future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer_remote;
  std::tie(result, writer_remote) = future.Take();
  EXPECT_EQ(result->status, FileSystemAccessStatus::kOk);
  EXPECT_TRUE(writer_remote.is_valid());
  EXPECT_EQ(GetCloneFileResult(handle_),
            CloneFileResult::kAttemptedAndCompletedAsExpected);
}

TEST_F(FileSystemAccessFileHandleSwapFileCloningTest,
       IgnoringExistingDataDoesNotClone) {
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      future;
  handle_->CreateFileWriter(
      /*keep_existing_data=*/false,
      /*auto_close=*/false,
      blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed,
      future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer_remote;
  std::tie(result, writer_remote) = future.Take();
  EXPECT_EQ(result->status, FileSystemAccessStatus::kOk);
  EXPECT_TRUE(writer_remote.is_valid());
  EXPECT_EQ(GetCloneFileResult(handle_), CloneFileResult::kDidNotAttempt);
}

TEST_F(FileSystemAccessFileHandleSwapFileCloningTest, HandleExistingSwapFile) {
  const FileSystemURL swap_url =
      file_system_context_->CreateCrackedFileSystemURL(
          test_src_storage_key_, storage::kFileSystemTypeLocal,
          dir_.GetPath().AppendASCII("test.crswap"));

  // Create pre-existing swap file.
  ASSERT_EQ(base::File::FILE_OK, storage::AsyncFileTestHelper::CreateFile(
                                     file_system_context_.get(), swap_url));

  // Creating the writer still succeeds, even though clonefile() will fail if
  // the destination file already exists.
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      future;
  handle_->CreateFileWriter(
      /*keep_existing_data=*/true,
      /*auto_close=*/false,
      blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed,
      future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer_remote;
  std::tie(result, writer_remote) = future.Take();
  EXPECT_EQ(result->status, FileSystemAccessStatus::kOk);
  EXPECT_TRUE(writer_remote.is_valid());
  EXPECT_EQ(GetCloneFileResult(handle_),
            CloneFileResult::kAttemptedAndCompletedAsExpected);
}

TEST_F(FileSystemAccessFileHandleSwapFileCloningTest, HandleCloneFailure) {
  handle_->set_swap_file_cloning_will_fail_for_testing();

  // Creating the writer still succeeds, even if cloning fails.
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      future;
  handle_->CreateFileWriter(
      /*keep_existing_data=*/true,
      /*auto_close=*/false,
      blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed,
      future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer_remote;
  std::tie(result, writer_remote) = future.Take();
  EXPECT_EQ(result->status, FileSystemAccessStatus::kOk);
  EXPECT_TRUE(writer_remote.is_valid());
  EXPECT_EQ(GetCloneFileResult(handle_), CloneFileResult::kAttemptedAndAborted);
}
#endif  // BUILDFLAG(IS_MAC)

struct MovePermissionsTestCase {
  // Is there a file to be overwritten?
  bool target_present;
  // Does the site have user activation?
  bool gesture_present;
};

// Uses a mock permission context to ensure the correct permission grant for the
// target file (and parent, for renames) is used, since moves retrieve the
// target's permission grant via GetSharedHandleStateForNonSandboxedPath() which
// always returns GRANTED for tests without a permission context.
//
// Moves do not call GetSharedHandleStateForNonSandboxedPath() on the
// destination directory, so the above tests are sufficient to test moves
// without access to the destination directory. These tests always grant write
// access to the destination directory.
class FileSystemAccessFileHandleImplMovePermissionsTest
    : public FileSystemAccessFileHandleImplTestBase,
      public testing::WithParamInterface<MovePermissionsTestCase> {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    SetupHelper(storage::kFileSystemTypeLocal, /*is_incognito=*/false,
                /*use_content_uri=*/false);
    manager_->SetPermissionContextForTesting(&permission_context_);
  }

  bool target_present() const { return GetParam().target_present; }
  bool gesture_present() const { return GetParam().gesture_present; }

  std::pair<base::FilePath, base::FilePath> CreateSourceAndMaybeTarget() {
    base::FilePath source;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &source));

    base::FilePath target;
    if (target_present()) {
      EXPECT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &target));
    } else {
      target = dir_.GetPath().AppendASCII("new_name.txt");
    }

    return {source, target};
  }

  void ExpectFileRenameSuccess(
      const base::FilePath& source,
      const base::FilePath& target,
      scoped_refptr<FixedFileSystemAccessPermissionGrant> target_grant) {
    auto source_handle =
        GetHandleWithPermissions(source, /*read_grant=*/allow_grant_,
                                 /*write_grant=*/allow_grant_);
    auto target_handle =
        GetHandleWithPermissions(target, /*read_grant=*/target_grant,
                                 /*write_grant=*/target_grant);
    auto origin = test_src_storage_key_.origin();
    auto target_basename = target.BaseName();

    EXPECT_CALL(permission_context_,
                IsFileTypeDangerous_(target_basename, origin))
        .WillOnce(testing::Return(false));
    EXPECT_CALL(permission_context_,
                GetReadPermissionGrant(
                    origin, content::PathInfo(target),
                    FileSystemAccessPermissionContext::HandleType::kFile,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(target_grant));
    EXPECT_CALL(permission_context_,
                GetWritePermissionGrant(
                    origin, content::PathInfo(target),
                    FileSystemAccessPermissionContext::HandleType::kFile,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(target_grant));
    EXPECT_CALL(
        permission_context_,
        ConfirmSensitiveEntryAccess_(
            origin, content::PathInfo(target),
            FileSystemAccessPermissionContext::HandleType::kFile,
            FileSystemAccessPermissionContext::UserAction::kSave,
            web_contents_->GetPrimaryMainFrame()->GetGlobalId(), testing::_))
        .WillOnce(base::test::RunOnceCallback<5>(
            FileSystemAccessPermissionContext::SensitiveEntryResult::kAllowed));

    // These checks should only be called if the file is successfully moved.

    if (source != target) {
      // On Windows, CreateTemporaryFileInDir() creates files with the '.tmp'
      // extension. Safe Browsing checks are not run on same-file-system moves
      // in which the extension does not change. For more context, see
      // FileSystemAccessSafeMoveHelper::RequireAfterWriteChecks().
      if (source.Extension() != FILE_PATH_LITERAL(".tmp") ||
          target.Extension() != FILE_PATH_LITERAL(".tmp")) {
        EXPECT_CALL(
            permission_context_,
            PerformAfterWriteChecks_(testing::_, testing::_, testing::_))
            .WillOnce(base::test::RunOnceCallback<2>(
                FileSystemAccessPermissionContext::AfterWriteCheckResult::
                    kAllow));
      }
    }
    EXPECT_CALL(permission_context_,
                NotifyEntryMoved(origin, PathInfo(source), PathInfo(target)));

    if (gesture_present()) {
      static_cast<TestRenderFrameHost*>(web_contents_->GetPrimaryMainFrame())
          ->SimulateUserActivation();
    }

    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    source_handle->Rename(target_basename.AsUTF8Unsafe(), future.GetCallback());
    EXPECT_EQ(future.Get()->status, FileSystemAccessStatus::kOk);
    if (source != target) {
      EXPECT_FALSE(base::PathExists(source));
    }
    EXPECT_TRUE(base::PathExists(target));
  }

  void ExpectFileRenameFailure(
      const base::FilePath& source,
      const base::FilePath& target,
      scoped_refptr<FixedFileSystemAccessPermissionGrant> target_grant,
      FileSystemAccessStatus result,
      bool expects_safe_name = true,
      std::optional<FileSystemAccessPermissionContext::SensitiveEntryResult>
          expected_sensitive_entry_result = std::nullopt) {
    auto source_handle =
        GetHandleWithPermissions(source, /*read_grant=*/allow_grant_,
                                 /*write_grant=*/allow_grant_);
    auto target_handle =
        GetHandleWithPermissions(target, /*read_grant=*/target_grant,
                                 /*write_grant=*/target_grant);
    auto origin = test_src_storage_key_.origin();
    auto target_basename = target.BaseName();

    EXPECT_CALL(permission_context_,
                IsFileTypeDangerous_(target_basename, origin))
        .WillRepeatedly(testing::Return(!expects_safe_name));
    if (expected_sensitive_entry_result.has_value()) {
      EXPECT_CALL(
          permission_context_,
          ConfirmSensitiveEntryAccess_(
              origin, content::PathInfo(target),
              FileSystemAccessPermissionContext::HandleType::kFile,
              FileSystemAccessPermissionContext::UserAction::kSave,
              web_contents_->GetPrimaryMainFrame()->GetGlobalId(), testing::_))
          .WillOnce(
              base::test::RunOnceCallback<5>(*expected_sensitive_entry_result));
    }
    if (expects_safe_name) {
      // If safe name check has passed, it should be expected to get grants.
      EXPECT_CALL(permission_context_,
                  GetReadPermissionGrant(
                      origin, content::PathInfo(target),
                      FileSystemAccessPermissionContext::HandleType::kFile,
                      FileSystemAccessPermissionContext::UserAction::kNone))
          .WillOnce(testing::Return(target_grant));
      EXPECT_CALL(permission_context_,
                  GetWritePermissionGrant(
                      origin, content::PathInfo(target),
                      FileSystemAccessPermissionContext::HandleType::kFile,
                      FileSystemAccessPermissionContext::UserAction::kNone))
          .WillOnce(testing::Return(target_grant));
    }

    // No after-write checks needed since the file should not have been moved.

    if (gesture_present()) {
      static_cast<TestRenderFrameHost*>(web_contents_->GetPrimaryMainFrame())
          ->SimulateUserActivation();
    }

    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    source_handle->Rename(target_basename.AsUTF8Unsafe(), future.GetCallback());
    EXPECT_EQ(future.Get()->status, result);

    // The source file should not have been removed.
    EXPECT_TRUE(base::PathExists(source));

    // The target file should remain untouched.
    EXPECT_EQ(target_present(), base::PathExists(target));
  }

  void ExpectFileMoveSuccess(const base::FilePath& source,
                             const base::FilePath& target) {
    base::FilePath target_parent = target.DirName();
    // File move cannot succeed if the site does not have write access to the
    // destination directory.
    auto dest_dir_handle = GetDirectoryHandleWithPermissions(
        target_parent, /*read_grant=*/allow_grant_,
        /*write_grant=*/allow_grant_);
    auto handle = GetHandleWithPermissions(source, /*read_grant=*/allow_grant_,
                                           /*write_grant=*/allow_grant_);
    auto origin = test_src_storage_key_.origin();
    auto target_basename = target.BaseName();

    EXPECT_CALL(permission_context_,
                IsFileTypeDangerous_(target_basename, origin))
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(
        permission_context_,
        ConfirmSensitiveEntryAccess_(
            origin, content::PathInfo(target),
            FileSystemAccessPermissionContext::HandleType::kFile,
            FileSystemAccessPermissionContext::UserAction::kSave,
            web_contents_->GetPrimaryMainFrame()->GetGlobalId(), testing::_))
        .WillOnce(base::test::RunOnceCallback<5>(
            FileSystemAccessPermissionContext::SensitiveEntryResult::kAllowed));

    // These checks should only be called if the file is successfully moved.
    if (source != target) {
      // On Windows, CreateTemporaryFileInDir() creates files with the '.tmp'
      // extension. Safe Browsing checks are not run on same-file-system moves
      // in which the extension does not change. For more context, see
      // FileSystemAccessSafeMoveHelper::RequireAfterWriteChecks().
      if (source.Extension() != FILE_PATH_LITERAL(".tmp") ||
          target.Extension() != FILE_PATH_LITERAL(".tmp")) {
        EXPECT_CALL(
            permission_context_,
            PerformAfterWriteChecks_(testing::_, testing::_, testing::_))
            .WillOnce(base::test::RunOnceCallback<2>(
                FileSystemAccessPermissionContext::AfterWriteCheckResult::
                    kAllow));
      }
    }
    EXPECT_CALL(permission_context_,
                NotifyEntryMoved(origin, PathInfo(source), PathInfo(target)));

    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> dir_remote;
    manager_->CreateTransferToken(*dest_dir_handle,
                                  dir_remote.InitWithNewPipeAndPassReceiver());

    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    handle->Move(std::move(dir_remote), target_basename.AsUTF8Unsafe(),
                 future.GetCallback());
    EXPECT_EQ(future.Get()->status, FileSystemAccessStatus::kOk);
    if (source != target) {
      EXPECT_FALSE(base::PathExists(source));
    }
    EXPECT_TRUE(base::PathExists(target));
  }

  void ExpectFileMoveFailure(
      const base::FilePath& source,
      const base::FilePath& target,
      scoped_refptr<FixedFileSystemAccessPermissionGrant> target_grant,
      FileSystemAccessStatus result,
      bool expects_safe_name = true,
      std::optional<FileSystemAccessPermissionContext::SensitiveEntryResult>
          expected_sensitive_entry_result = std::nullopt) {
    base::FilePath target_parent = target.DirName();
    // The site has write access to the destination directory.
    auto dest_dir_handle = GetDirectoryHandleWithPermissions(
        target_parent, /*read_grant=*/target_grant,
        /*write_grant=*/target_grant);
    auto handle = GetHandleWithPermissions(source, /*read_grant=*/allow_grant_,
                                           /*write_grant=*/allow_grant_);
    auto origin = test_src_storage_key_.origin();
    auto target_basename = target.BaseName();

    EXPECT_CALL(permission_context_,
                IsFileTypeDangerous_(target_basename, origin))
        .WillRepeatedly(testing::Return(!expects_safe_name));
    if (expected_sensitive_entry_result.has_value()) {
      EXPECT_CALL(
          permission_context_,
          ConfirmSensitiveEntryAccess_(
              origin, content::PathInfo(target),
              FileSystemAccessPermissionContext::HandleType::kFile,
              FileSystemAccessPermissionContext::UserAction::kSave,
              web_contents_->GetPrimaryMainFrame()->GetGlobalId(), testing::_))
          .WillOnce(
              base::test::RunOnceCallback<5>(*expected_sensitive_entry_result));
    }

    // No after-write checks needed since the file should not have been moved.

    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> dir_remote;
    manager_->CreateTransferToken(*dest_dir_handle,
                                  dir_remote.InitWithNewPipeAndPassReceiver());

    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    handle->Move(std::move(dir_remote), target_basename.AsUTF8Unsafe(),
                 future.GetCallback());
    EXPECT_EQ(future.Get()->status, result);

    // The source file should not have been removed.
    EXPECT_TRUE(base::PathExists(source));
  }

 protected:
  testing::StrictMock<MockFileSystemAccessPermissionContext>
      permission_context_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessFileHandleImplMovePermissionsTest,
    ::testing::Values(MovePermissionsTestCase{.target_present = false,
                                              .gesture_present = false},
                      MovePermissionsTestCase{.target_present = false,
                                              .gesture_present = true},
                      MovePermissionsTestCase{.target_present = true,
                                              .gesture_present = false},
                      MovePermissionsTestCase{.target_present = true,
                                              .gesture_present = true}),
    [](const testing::TestParamInfo<
        FileSystemAccessFileHandleImplMovePermissionsTest::ParamType>& info) {
      return base::StrCat(
          {(info.param.target_present ? "" : "No"), "TargetPresent_",
           (info.param.gesture_present ? "" : "No"), "GesturePresent"});
    });

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest,
       Rename_HasTargetWriteAccess) {
  auto [source, target] = CreateSourceAndMaybeTarget();
  ExpectFileRenameSuccess(source, target, /*target_grant=*/allow_grant_);
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest,
       Rename_AskTargetWriteAccess) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  // The site has not yet asked for access to the target entry.
  auto target_grant = ask_grant_;

  if (!gesture_present()) {
    // The rename should fail because write access to the target is not granted.
    ExpectFileRenameFailure(
        source, target, target_grant, FileSystemAccessStatus::kPermissionDenied
        // No user activation, so the sensitive entry access check is not
        // performed.
    );
  } else if (target_present()) {
    // The rename should fail because write access to the target is not granted.
    ExpectFileRenameFailure(
        source, target, target_grant,
        FileSystemAccessStatus::kInvalidModificationError,
        /*expects_safe_name=*/true,
        // With user activation, the sensitive entry access check should be
        // performed and allowed.
        FileSystemAccessPermissionContext::SensitiveEntryResult::kAllowed);
  } else {
    ExpectFileRenameSuccess(source, target, target_grant);
  }
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest, Rename_SameFile) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  target = source;

  ExpectFileRenameSuccess(source, target, /*target_grant=*/allow_grant_);
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest, Rename_UnsafeName) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  ExpectFileRenameFailure(
      source, target, /*target_grant=*/allow_grant_,
      FileSystemAccessStatus::kInvalidArgument,
      /*expects_safe_name=*/false
      // The access is already granted to the target, so the sensitive entry
      // access check should not be performed.
  );
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest,
       Rename_SensitiveName) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  ExpectFileRenameFailure(
      source, target, /*target_grant=*/allow_grant_,
      FileSystemAccessStatus::kInvalidArgument,
      /*expects_safe_name=*/true,
      // Let the test fail because the sensitive entry access check is aborted.
      FileSystemAccessPermissionContext::SensitiveEntryResult::kAbort);
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest, Move) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  ExpectFileMoveSuccess(source, target);
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest,
       Move_NoTargetWriteAccessFails) {
  auto [source, target] = CreateSourceAndMaybeTarget();
  ExpectFileMoveFailure(source, target, /*target_grant=*/ask_grant_,
                        FileSystemAccessStatus::kPermissionDenied);
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest, Move_SameFile) {
  auto [source, unused] = CreateSourceAndMaybeTarget();

  ExpectFileMoveSuccess(source, source);
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest, Move_UnsafeName) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  ExpectFileMoveFailure(source, target, /*target_grant=*/allow_grant_,
                        FileSystemAccessStatus::kInvalidArgument,
                        /*expects_safe_name=*/false
                        // The access is already granted to the target, so the
                        // sensitive entry access check should not be performed.
  );
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest, Move_SensitiveName) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  ExpectFileMoveFailure(
      source, target, /*target_grant=*/allow_grant_,
      FileSystemAccessStatus::kInvalidArgument,
      /*expects_safe_name=*/true,
      // Let the test fail because the sensitive entry access check is aborted.
      FileSystemAccessPermissionContext::SensitiveEntryResult::kAbort);
}

// Base class for file handle tests that require mock permission grants.
class FileSystemAccessFileHandleImplMockGrantTestBase
    : public FileSystemAccessFileHandleImplTestBase {
 public:
  void SetUp() override {
    FileSystemAccessFileHandleImplTestBase::SetUp();
    mock_read_grant_ = base::MakeRefCounted<
        testing::StrictMock<MockFileSystemAccessPermissionGrant>>();
    mock_write_grant_ = base::MakeRefCounted<
        testing::StrictMock<MockFileSystemAccessPermissionGrant>>();
  }

 protected:
  // Creates a handle to a file named "test_file" in the test directory.
  std::unique_ptr<FileSystemAccessFileHandleImpl> CreateHandle() {
    auto test_path = dir_.GetPath().AppendASCII("test_file");
    EXPECT_TRUE(base::WriteFile(test_path, ""));
    auto url = manager_->CreateFileSystemURLFromPath(PathInfo(test_path));
    return std::make_unique<FileSystemAccessFileHandleImpl>(
        manager_.get(), kFrameBindingContext, url, "test_file",
        FileSystemAccessManagerImpl::SharedHandleState(mock_read_grant_,
                                                       mock_write_grant_));
  }

  // Calls GetPermissionStatus() on the given `handle` and waits for the result.
  void GetPermissionStatus(FileSystemAccessFileHandleImpl* handle,
                           blink::mojom::FileSystemAccessPermissionMode mode,
                           blink::mojom::PermissionStatus* out_status) {
    base::test::TestFuture<blink::mojom::PermissionStatus> future;
    handle->GetPermissionStatus(mode, future.GetCallback());
    *out_status = future.Get();
  }

  // Calls RequestPermission() on the given `handle` and waits for the result.
  std::pair<blink::mojom::FileSystemAccessErrorPtr,
            blink::mojom::PermissionStatus>
  RequestPermission(FileSystemAccessFileHandleImpl* handle,
                    blink::mojom::FileSystemAccessPermissionMode mode) {
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                           blink::mojom::PermissionStatus>
        future;
    handle->RequestPermission(mode, future.GetCallback());
    return future.Take();
  }

  // Sets up expectations for a call to `RequestPermission()` on a mock grant.
  void SetUpGrantExpectations(
      testing::StrictMock<MockFileSystemAccessPermissionGrant>& grant,
      PermissionStatus new_status,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome outcome) {
    EXPECT_CALL(grant, GetStatus())
        .WillRepeatedly(testing::Return(PermissionStatus::ASK));
    EXPECT_CALL(
        grant,
        RequestPermission_(
            kFrameId,
            FileSystemAccessPermissionGrant::UserActivationState::kRequired, _))
        .WillOnce(
            testing::DoAll(testing::InvokeWithoutArgs([&grant, new_status]() {
                             EXPECT_CALL(grant, GetStatus())
                                 .WillRepeatedly(testing::Return(new_status));
                           }),
                           base::test::RunOnceCallback<2>(outcome)));
  }

  const int kProcessId = 1;
  const int kFrameRoutingId = 2;
  const GlobalRenderFrameHostId kFrameId{kProcessId, kFrameRoutingId};
  const FileSystemAccessManagerImpl::BindingContext kFrameBindingContext = {
      test_src_storage_key_, test_src_url_, kFrameId};

  scoped_refptr<testing::StrictMock<MockFileSystemAccessPermissionGrant>>
      mock_read_grant_;
  scoped_refptr<testing::StrictMock<MockFileSystemAccessPermissionGrant>>
      mock_write_grant_;
};

class FileSystemAccessFileHandleImplGetPermissionStatusTest
    : public FileSystemAccessFileHandleImplMockGrantTestBase {};

TEST_F(FileSystemAccessFileHandleImplGetPermissionStatusTest, ReadHandle) {
  auto test_path = dir_.GetPath().AppendASCII("test_file");
  ASSERT_TRUE(base::WriteFile(test_path, ""));

  auto read_only_handle =
      GetHandleWithPermissions(test_path, allow_grant_, deny_grant_);

  blink::mojom::PermissionStatus status;
  GetPermissionStatus(read_only_handle.get(),
                      blink::mojom::FileSystemAccessPermissionMode::kRead,
                      &status);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::GRANTED);

  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
    GetPermissionStatus(read_only_handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kWrite,
                        &status);
    EXPECT_EQ(status, blink::mojom::PermissionStatus::DENIED);
  }

  GetPermissionStatus(read_only_handle.get(),
                      blink::mojom::FileSystemAccessPermissionMode::kReadWrite,
                      &status);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::DENIED);
}

TEST_F(FileSystemAccessFileHandleImplGetPermissionStatusTest, WriteHandle) {
  auto test_path = dir_.GetPath().AppendASCII("test_file");
  ASSERT_TRUE(base::WriteFile(test_path, ""));

  auto write_handle =
      GetHandleWithPermissions(test_path, deny_grant_, allow_grant_);

  blink::mojom::PermissionStatus status;
  GetPermissionStatus(write_handle.get(),
                      blink::mojom::FileSystemAccessPermissionMode::kRead,
                      &status);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::DENIED);

  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
    GetPermissionStatus(write_handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kWrite,
                        &status);
    EXPECT_EQ(status, blink::mojom::PermissionStatus::GRANTED);
  }

  GetPermissionStatus(write_handle.get(),
                      blink::mojom::FileSystemAccessPermissionMode::kReadWrite,
                      &status);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::DENIED);
}

TEST_F(FileSystemAccessFileHandleImplGetPermissionStatusTest, ReadWriteHandle) {
  auto test_path = dir_.GetPath().AppendASCII("test_file");
  ASSERT_TRUE(base::WriteFile(test_path, ""));

  auto read_write_handle =
      GetHandleWithPermissions(test_path, allow_grant_, allow_grant_);

  blink::mojom::PermissionStatus status;
  GetPermissionStatus(read_write_handle.get(),
                      blink::mojom::FileSystemAccessPermissionMode::kRead,
                      &status);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::GRANTED);

  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
    GetPermissionStatus(read_write_handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kWrite,
                        &status);
    EXPECT_EQ(status, blink::mojom::PermissionStatus::GRANTED);
  }

  GetPermissionStatus(read_write_handle.get(),
                      blink::mojom::FileSystemAccessPermissionMode::kReadWrite,
                      &status);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::GRANTED);
}

class FileSystemAccessFileHandleImplRequestPermissionTest
    : public FileSystemAccessFileHandleImplMockGrantTestBase {};

TEST_F(FileSystemAccessFileHandleImplRequestPermissionTest,
       RequestRead_Granted) {
  auto handle = CreateHandle();

  SetUpGrantExpectations(
      *mock_read_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);

  EXPECT_THAT(
      RequestPermission(handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kRead),
      IsOkAndPermissionStatus(PermissionStatus::GRANTED));
}

TEST_F(FileSystemAccessFileHandleImplRequestPermissionTest,
       RequestRead_Denied) {
  auto handle = CreateHandle();

  SetUpGrantExpectations(
      *mock_read_grant_, PermissionStatus::DENIED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied);

  EXPECT_THAT(
      RequestPermission(handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kRead),
      IsOkAndPermissionStatus(PermissionStatus::DENIED));
}

TEST_F(FileSystemAccessFileHandleImplRequestPermissionTest,
       RequestReadWrite_Granted) {
  auto handle = CreateHandle();

  SetUpGrantExpectations(
      *mock_read_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);
  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);

  EXPECT_THAT(RequestPermission(
                  handle.get(),
                  blink::mojom::FileSystemAccessPermissionMode::kReadWrite),
              IsOkAndPermissionStatus(PermissionStatus::GRANTED));
}

TEST_F(FileSystemAccessFileHandleImplRequestPermissionTest,
       RequestReadWrite_ReadDenied) {
  auto handle = CreateHandle();

  SetUpGrantExpectations(
      *mock_read_grant_, PermissionStatus::DENIED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied);
  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);

  EXPECT_THAT(RequestPermission(
                  handle.get(),
                  blink::mojom::FileSystemAccessPermissionMode::kReadWrite),
              IsOkAndPermissionStatus(PermissionStatus::DENIED));
}

TEST_F(FileSystemAccessFileHandleImplRequestPermissionTest,
       RequestReadWrite_WriteDenied) {
  auto handle = CreateHandle();

  EXPECT_CALL(*mock_read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::GRANTED));
  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::DENIED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied);

  EXPECT_THAT(RequestPermission(
                  handle.get(),
                  blink::mojom::FileSystemAccessPermissionMode::kReadWrite),
              IsOkAndPermissionStatus(PermissionStatus::DENIED));
}

TEST_F(FileSystemAccessFileHandleImplRequestPermissionTest,
       RequestWrite_Granted) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateHandle();

  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);

  EXPECT_THAT(
      RequestPermission(handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kWrite),
      IsOkAndPermissionStatus(PermissionStatus::GRANTED));
}

TEST_F(FileSystemAccessFileHandleImplRequestPermissionTest,
       RequestWrite_Denied) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateHandle();

  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::DENIED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied);

  EXPECT_THAT(
      RequestPermission(handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kWrite),
      IsOkAndPermissionStatus(PermissionStatus::DENIED));
}

class FileSystemAccessFileHandleImplRemoveWriteModeTest
    : public FileSystemAccessFileHandleImplMockGrantTestBase,
      public testing::WithParamInterface<WriteModeTestParams> {
 public:
  FileSystemAccessFileHandleImplRemoveWriteModeTest() {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kFileSystemAccessWriteMode,
        GetParam().is_feature_enabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that `Remove()` requests the correct permissions before removing a
// file. When `kFileSystemAccessWriteMode` is
// - disabled: it should request both read and write permissions.
// - enabled: it should only request write permission.
TEST_P(FileSystemAccessFileHandleImplRemoveWriteModeTest,
       RequestsCorrectPermissions) {
  auto handle = CreateHandle();
  if (!GetParam().is_feature_enabled) {
    SetUpGrantExpectations(*mock_read_grant_, PermissionStatus::GRANTED,
                           FileSystemAccessPermissionGrant::
                               PermissionRequestOutcome::kUserGranted);
  }
  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Remove(future.GetCallback());
  EXPECT_EQ(future.Get()->status, FileSystemAccessStatus::kOk);
  EXPECT_FALSE(base::PathExists(dir_.GetPath().AppendASCII("test_file")));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessFileHandleImplRemoveWriteModeTest,
    testing::ValuesIn(kTestParams),
    [](const testing::TestParamInfo<WriteModeTestParams>& info) {
      return info.param.test_name_suffix;
    });

// Tests for the rename() method on the file handle, parameterized to run
// with the kFileSystemAccessWriteMode feature enabled and disabled.
class FileSystemAccessFileHandleImplRenameWriteModeTest
    : public FileSystemAccessFileHandleImplMockGrantTestBase,
      public testing::WithParamInterface<WriteModeTestParams> {
 public:
  FileSystemAccessFileHandleImplRenameWriteModeTest() {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kFileSystemAccessWriteMode,
        GetParam().is_feature_enabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that rename() on a file handle requests the correct permission
// mode depending on whether the kFileSystemAccessWriteMode feature is enabled.
TEST_P(FileSystemAccessFileHandleImplRenameWriteModeTest,
       RequestsCorrectPermissions) {
  auto handle = CreateHandle();
  if (!GetParam().is_feature_enabled) {
    SetUpGrantExpectations(*mock_read_grant_, PermissionStatus::GRANTED,
                           FileSystemAccessPermissionGrant::
                               PermissionRequestOutcome::kUserGranted);
  }
  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Rename("new-name", future.GetCallback());
  EXPECT_EQ(future.Get()->status, FileSystemAccessStatus::kOk);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessFileHandleImplRenameWriteModeTest,
    testing::ValuesIn(kTestParams),
    [](const testing::TestParamInfo<WriteModeTestParams>& info) {
      return info.param.test_name_suffix;
    });

// Tests for the move() method on the file handle, parameterized to run
// with the kFileSystemAccessWriteMode feature enabled and disabled.
class FileSystemAccessFileHandleImplMoveWriteModeTest
    : public FileSystemAccessFileHandleImplMockGrantTestBase,
      public testing::WithParamInterface<WriteModeTestParams> {
 public:
  FileSystemAccessFileHandleImplMoveWriteModeTest() {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kFileSystemAccessWriteMode,
        GetParam().is_feature_enabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that move() on a file handle requests the correct permission
// mode depending on whether the kFileSystemAccessWriteMode feature is enabled.
TEST_P(FileSystemAccessFileHandleImplMoveWriteModeTest,
       RequestsCorrectPermissions) {
  auto handle = CreateHandle();
  auto dest_handle = GetDirectoryHandleWithPermissions(
      dir_.GetPath(), allow_grant_, allow_grant_);

  if (!GetParam().is_feature_enabled) {
    SetUpGrantExpectations(*mock_read_grant_, PermissionStatus::GRANTED,
                           FileSystemAccessPermissionGrant::
                               PermissionRequestOutcome::kUserGranted);
  }
  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> dest_token;
  manager_->CreateTransferToken(*dest_handle,
                                dest_token.InitWithNewPipeAndPassReceiver());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Move(std::move(dest_token), "new-name", future.GetCallback());
  EXPECT_EQ(future.Get()->status, FileSystemAccessStatus::kOk);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessFileHandleImplMoveWriteModeTest,
    testing::ValuesIn(kTestParams),
    [](const testing::TestParamInfo<WriteModeTestParams>& info) {
      return info.param.test_name_suffix;
    });

}  // namespace content
