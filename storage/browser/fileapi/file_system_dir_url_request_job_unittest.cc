// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_dir_url_request_job.h"
#include "storage/browser/fileapi/file_system_file_util.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

using storage::FileSystemContext;
using storage::FileSystemOperationContext;
using storage::FileSystemURL;

namespace content {
namespace {

// We always use the TEMPORARY FileSystem in this test.
const char kFileSystemURLPrefix[] = "filesystem:http://remote/temporary/";

const char kValidExternalMountPoint[] = "mnt_name";

// An auto mounter that will try to mount anything for |storage_domain| =
// "automount", but will only succeed for the mount point "mnt_name".
bool TestAutoMountForURLRequest(
    const storage::FileSystemRequestInfo& request_info,
    const storage::FileSystemURL& filesystem_url,
    base::OnceCallback<void(base::File::Error result)> callback) {
  if (request_info.storage_domain != "automount")
    return false;

  std::vector<base::FilePath::StringType> components;
  filesystem_url.path().GetComponents(&components);
  std::string mount_point = base::FilePath(components[0]).AsUTF8Unsafe();

  if (mount_point == kValidExternalMountPoint) {
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        kValidExternalMountPoint,
        storage::kFileSystemTypeTest,
        storage::FileSystemMountOption(),
        base::FilePath());
    std::move(callback).Run(base::File::FILE_OK);
  } else {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
  }
  return true;
}

class FileSystemDirURLRequestJobFactory : public net::URLRequestJobFactory {
 public:
  FileSystemDirURLRequestJobFactory(const std::string& storage_domain,
                                    FileSystemContext* context)
      : storage_domain_(storage_domain), file_system_context_(context) {
  }

  net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new storage::FileSystemDirURLRequestJob(
        request, network_delegate, storage_domain_, file_system_context_);
  }

  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) const override {
    return nullptr;
  }

  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return nullptr;
  }

  bool IsHandledProtocol(const std::string& scheme) const override {
    return true;
  }

  bool IsSafeRedirectTarget(const GURL& location) const override {
    return false;
  }

 private:
  std::string storage_domain_;
  FileSystemContext* file_system_context_;
};


}  // namespace

class FileSystemDirURLRequestJobTest : public testing::Test {
 protected:
  FileSystemDirURLRequestJobTest()
    : weak_factory_(this) {
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    special_storage_policy_ = new MockSpecialStoragePolicy;
    file_system_context_ =
        CreateFileSystemContextForTesting(nullptr, temp_dir_.GetPath());

    file_system_context_->OpenFileSystem(
        GURL("http://remote/"), storage::kFileSystemTypeTemporary,
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce(&FileSystemDirURLRequestJobTest::OnOpenFileSystem,
                       weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // NOTE: order matters, request must die before delegate
    request_.reset(nullptr);
    delegate_.reset(nullptr);
  }

  void SetUpAutoMountContext(base::FilePath* mnt_point) {
    *mnt_point = temp_dir_.GetPath().AppendASCII("auto_mount_dir");
    ASSERT_TRUE(base::CreateDirectory(*mnt_point));

    std::vector<std::unique_ptr<storage::FileSystemBackend>>
        additional_providers;
    additional_providers.emplace_back(std::make_unique<TestFileSystemBackend>(
        base::ThreadTaskRunnerHandle::Get().get(), *mnt_point));

    std::vector<storage::URLRequestAutoMountHandler> handlers;
    handlers.emplace_back(base::BindRepeating(&TestAutoMountForURLRequest));

    file_system_context_ = CreateFileSystemContextWithAutoMountersForTesting(
        nullptr, std::move(additional_providers), handlers,
        temp_dir_.GetPath());
  }

  void OnOpenFileSystem(const GURL& root_url,
                        const std::string& name,
                        base::File::Error result) {
    ASSERT_EQ(base::File::FILE_OK, result);
  }

  void TestRequestHelper(const GURL& url, bool run_to_completion,
                         FileSystemContext* file_system_context) {
    delegate_.reset(new net::TestDelegate());
    job_factory_.reset(new FileSystemDirURLRequestJobFactory(
        url.GetOrigin().host(), file_system_context));
    empty_context_.set_job_factory(job_factory_.get());

    request_ = empty_context_.CreateRequest(url, net::DEFAULT_PRIORITY,
                                            delegate_.get(),
                                            TRAFFIC_ANNOTATION_FOR_TESTS);
    request_->Start();
    ASSERT_TRUE(request_->is_pending());  // verify that we're starting async
    if (run_to_completion)
      delegate_->RunUntilComplete();
  }

  void TestRequest(const GURL& url) {
    TestRequestHelper(url, true, file_system_context_.get());
  }

  void TestRequestWithContext(const GURL& url,
                              FileSystemContext* file_system_context) {
    TestRequestHelper(url, true, file_system_context);
  }

  void TestRequestNoRun(const GURL& url) {
    TestRequestHelper(url, false, file_system_context_.get());
  }

  FileSystemURL CreateURL(const base::FilePath& file_path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://remote"), storage::kFileSystemTypeTemporary, file_path);
  }

  FileSystemOperationContext* NewOperationContext() {
    FileSystemOperationContext* context(
        new FileSystemOperationContext(file_system_context_.get()));
    context->set_allowed_bytes_growth(1024);
    return context;
  }

  void CreateDirectory(const base::StringPiece& dir_name) {
    base::FilePath path = base::FilePath().AppendASCII(dir_name);
    std::unique_ptr<FileSystemOperationContext> context(NewOperationContext());
    ASSERT_EQ(base::File::FILE_OK, file_util()->CreateDirectory(
        context.get(),
        CreateURL(path),
        false /* exclusive */,
        false /* recursive */));
  }

  void EnsureFileExists(const base::StringPiece file_name) {
    base::FilePath path = base::FilePath().AppendASCII(file_name);
    std::unique_ptr<FileSystemOperationContext> context(NewOperationContext());
    ASSERT_EQ(
        base::File::FILE_OK,
        file_util()->EnsureFileExists(context.get(), CreateURL(path), nullptr));
  }

  void TruncateFile(const base::StringPiece file_name, int64_t length) {
    base::FilePath path = base::FilePath().AppendASCII(file_name);
    std::unique_ptr<FileSystemOperationContext> context(NewOperationContext());
    ASSERT_EQ(base::File::FILE_OK, file_util()->Truncate(
        context.get(), CreateURL(path), length));
  }

  base::File::Error GetFileInfo(const base::FilePath& path,
                                base::File::Info* file_info,
                                base::FilePath* platform_file_path) {
    std::unique_ptr<FileSystemOperationContext> context(NewOperationContext());
    return file_util()->GetFileInfo(context.get(),
                                    CreateURL(path),
                                    file_info, platform_file_path);
  }

  // If |size| is negative, the reported size is ignored.
  void VerifyListingEntry(const std::string& entry_line,
                          const std::string& name,
                          const std::string& url,
                          bool is_directory,
                          int64_t size) {
#define NUMBER "([0-9-]*)"
#define STR "([^\"]*)"
    icu::UnicodeString pattern("^<script>addRow\\(\"" STR "\",\"" STR
        "\",(0|1)," NUMBER ",\"" STR "\"," NUMBER ",\"" STR "\"\\);</script>");
#undef NUMBER
#undef STR
    icu::UnicodeString input(entry_line.c_str());

    UErrorCode status = U_ZERO_ERROR;
    icu::RegexMatcher match(pattern, input, 0, status);

    EXPECT_TRUE(match.find());
    EXPECT_EQ(7, match.groupCount());
    EXPECT_EQ(icu::UnicodeString(name.c_str()), match.group(1, status));
    EXPECT_EQ(icu::UnicodeString(url.c_str()), match.group(2, status));
    EXPECT_EQ(icu::UnicodeString(is_directory ? "1" : "0"),
              match.group(3, status));
    if (size >= 0) {
      icu::UnicodeString size_string(
          base::FormatBytesUnlocalized(size).c_str());
      EXPECT_EQ(size_string, match.group(5, status));
    }

    icu::UnicodeString date_ustr(match.group(7, status));
    std::unique_ptr<icu::DateFormat> formatter(
        icu::DateFormat::createDateTimeInstance(icu::DateFormat::kShort));
    UErrorCode parse_status = U_ZERO_ERROR;
    UDate udate = formatter->parse(date_ustr, parse_status);
    EXPECT_TRUE(U_SUCCESS(parse_status));
    base::Time date = base::Time::FromJsTime(udate);
    EXPECT_FALSE(date.is_null());
  }

  GURL CreateFileSystemURL(const std::string& path) {
    return GURL(kFileSystemURLPrefix + path);
  }

  storage::FileSystemFileUtil* file_util() {
    return file_system_context_->sandbox_delegate()->sync_file_util();
  }

  // Put the message loop at the top, so that it's the last thing deleted.
  // Delete all task runner objects before the MessageLoop, to help prevent
  // leaks caused by tasks posted during shutdown.
  base::MessageLoopForIO message_loop_;

  base::ScopedTempDir temp_dir_;
  net::URLRequestContext empty_context_;
  std::unique_ptr<net::TestDelegate> delegate_;
  std::unique_ptr<net::URLRequest> request_;
  std::unique_ptr<FileSystemDirURLRequestJobFactory> job_factory_;
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<FileSystemContext> file_system_context_;
  base::WeakPtrFactory<FileSystemDirURLRequestJobTest> weak_factory_;
};

namespace {

TEST_F(FileSystemDirURLRequestJobTest, DirectoryListing) {
  CreateDirectory("foo");
  CreateDirectory("foo/bar");
  CreateDirectory("foo/bar/baz");

  EnsureFileExists("foo/bar/hoge");
  TruncateFile("foo/bar/hoge", 10);

  TestRequest(CreateFileSystemURL("foo/bar/"));

  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(1, delegate_->response_started_count());
  EXPECT_FALSE(delegate_->received_data_before_response());
  EXPECT_GT(delegate_->bytes_received(), 0);

  std::istringstream in(delegate_->data_received());
  std::string line;
  EXPECT_TRUE(std::getline(in, line));

#if defined(OS_WIN)
  EXPECT_EQ("<script>start(\"foo\\\\bar\");</script>", line);
#elif defined(OS_POSIX)
  EXPECT_EQ("<script>start(\"/foo/bar\");</script>", line);
#endif

  EXPECT_TRUE(std::getline(in, line));
  VerifyListingEntry(line, "hoge", "hoge", false, 10);

  EXPECT_TRUE(std::getline(in, line));
  VerifyListingEntry(line, "baz", "baz", true, 0);
  EXPECT_FALSE(!!std::getline(in, line));
}

TEST_F(FileSystemDirURLRequestJobTest, InvalidURL) {
  TestRequest(GURL("filesystem:/foo/bar/baz"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_INVALID_URL, delegate_->request_status());
}

TEST_F(FileSystemDirURLRequestJobTest, NoSuchRoot) {
  TestRequest(GURL("filesystem:http://remote/persistent/somedir/"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, delegate_->request_status());
}

TEST_F(FileSystemDirURLRequestJobTest, NoSuchDirectory) {
  TestRequest(CreateFileSystemURL("somedir/"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, delegate_->request_status());
}

TEST_F(FileSystemDirURLRequestJobTest, Cancel) {
  CreateDirectory("foo");
  TestRequestNoRun(CreateFileSystemURL("foo/"));
  // Run StartAsync() and only StartAsync().
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                  request_.release());
  base::RunLoop().RunUntilIdle();
  // If we get here, success! we didn't crash!
}

TEST_F(FileSystemDirURLRequestJobTest, Incognito) {
  CreateDirectory("foo");

  scoped_refptr<FileSystemContext> file_system_context =
      CreateIncognitoFileSystemContextForTesting(nullptr, temp_dir_.GetPath());

  TestRequestWithContext(CreateFileSystemURL("/"),
                         file_system_context.get());
  ASSERT_FALSE(request_->is_pending());

  std::istringstream in(delegate_->data_received());
  std::string line;
  EXPECT_TRUE(std::getline(in, line));
  EXPECT_FALSE(!!std::getline(in, line));

  TestRequestWithContext(CreateFileSystemURL("foo"),
                         file_system_context.get());
  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, delegate_->request_status());
}

TEST_F(FileSystemDirURLRequestJobTest, AutoMountDirectoryListing) {
  base::FilePath mnt_point;
  SetUpAutoMountContext(&mnt_point);
  ASSERT_TRUE(base::CreateDirectory(mnt_point));
  ASSERT_TRUE(base::CreateDirectory(mnt_point.AppendASCII("foo")));
  ASSERT_EQ(10,
            base::WriteFile(mnt_point.AppendASCII("bar"), "1234567890", 10));

  TestRequest(GURL("filesystem:http://automount/external/mnt_name"));

  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(1, delegate_->response_started_count());
  EXPECT_FALSE(delegate_->received_data_before_response());
  EXPECT_GT(delegate_->bytes_received(), 0);

  std::istringstream in(delegate_->data_received());
  std::string line;
  EXPECT_TRUE(std::getline(in, line));  // |line| contains the temp dir path.

  // Result order is not guaranteed, so sort the results.
  std::vector<std::string> listing_entries;
  while (!!std::getline(in, line))
    listing_entries.push_back(line);

  ASSERT_EQ(2U, listing_entries.size());
  std::sort(listing_entries.begin(), listing_entries.end());
  VerifyListingEntry(listing_entries[0], "bar", "bar", false, 10);
  VerifyListingEntry(listing_entries[1], "foo", "foo", true, -1);

  ASSERT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          kValidExternalMountPoint));
}

TEST_F(FileSystemDirURLRequestJobTest, AutoMountInvalidRoot) {
  base::FilePath mnt_point;
  SetUpAutoMountContext(&mnt_point);
  TestRequest(GURL("filesystem:http://automount/external/invalid"));

  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, delegate_->request_status());

  ASSERT_FALSE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          "invalid"));
}

TEST_F(FileSystemDirURLRequestJobTest, AutoMountNoHandler) {
  base::FilePath mnt_point;
  SetUpAutoMountContext(&mnt_point);
  TestRequest(GURL("filesystem:http://noauto/external/mnt_name"));

  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, delegate_->request_status());

  ASSERT_FALSE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          kValidExternalMountPoint));
}

}  // namespace
}  // namespace content
