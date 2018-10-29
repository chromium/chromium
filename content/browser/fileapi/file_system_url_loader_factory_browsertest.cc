// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/unicodestring.h"
#include "base/rand_util.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/fileapi/file_system_url_loader_factory.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/base/mime_util.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_file_util.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/browser/test/test_file_system_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

using content::AsyncFileTestHelper;
using network::mojom::URLLoaderFactory;
using storage::FileSystemContext;
using storage::FileSystemOperationContext;
using storage::FileSystemURL;

namespace content {
namespace {

// We always use the TEMPORARY FileSystem in these tests.
const char kFileSystemURLPrefix[] = "filesystem:http://remote/temporary/";

const char kValidExternalMountPoint[] = "mnt_name";

const char kTestFileData[] = "0123456789";

void FillBuffer(char* buffer, size_t len) {
  base::RandBytes(buffer, len);
}

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
        kValidExternalMountPoint, storage::kFileSystemTypeTest,
        storage::FileSystemMountOption(), base::FilePath());
    std::move(callback).Run(base::File::FILE_OK);
  } else {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
  }
  return true;
}

void ReadDataPipeInternal(mojo::DataPipeConsumerHandle handle,
                          std::string* result,
                          base::OnceClosure quit_closure) {
  while (true) {
    uint32_t num_bytes;
    const void* buffer = nullptr;
    MojoResult rv =
        handle.BeginReadData(&buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
    switch (rv) {
      case MOJO_RESULT_BUSY:
      case MOJO_RESULT_INVALID_ARGUMENT:
        NOTREACHED();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        std::move(quit_closure).Run();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&ReadDataPipeInternal, handle, result,
                                      std::move(quit_closure)));
        return;
      case MOJO_RESULT_OK:
        EXPECT_NE(nullptr, buffer);
        EXPECT_GT(num_bytes, 0u);
        uint32_t before_size = result->size();
        result->append(static_cast<const char*>(buffer), num_bytes);
        uint32_t read_size = result->size() - before_size;
        EXPECT_EQ(num_bytes, read_size);
        rv = handle.EndReadData(read_size);
        EXPECT_EQ(MOJO_RESULT_OK, rv);
        break;
    }
  }
  NOTREACHED();
  return;
}

std::string ReadDataPipe(mojo::ScopedDataPipeConsumerHandle handle) {
  EXPECT_TRUE(handle.is_valid());
  if (!handle.is_valid())
    return "";
  std::string result;
  base::RunLoop loop;
  ReadDataPipeInternal(handle.get(), &result, loop.QuitClosure());
  loop.Run();
  return result;
}

// Directory listings can have a HTML header in the file to format the response.
// This function determines if a single line in the response is for a directory
// entry.
bool IsDirectoryListingLine(const std::string& line) {
  return line.find("<script>addRow(\"") == 0;
}

// Is the line a title inserted by net::GetDirectoryListingHeader?
bool IsDirectoryListingTitle(const std::string& line) {
  return line.find("<script>start(\"") == 0;
}

void ShutdownFileSystemContextOnIOThread(
    scoped_refptr<FileSystemContext> file_system_context) {
  if (!file_system_context)
    return;
  file_system_context->Shutdown();
  file_system_context = nullptr;
}

}  // namespace

class FileSystemURLLoaderFactoryTest : public ContentBrowserTest {
 protected:
  FileSystemURLLoaderFactoryTest() {}
  ~FileSystemURLLoaderFactoryTest() override = default;

  void SetUpOnMainThread() override {
    feature_list_.InitAndEnableFeature(network::features::kNetworkService);

    special_storage_policy_ = new MockSpecialStoragePolicy;

    // We use a test FileSystemContext which runs on the main thread, so we
    // can work with it synchronously.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_system_context_ =
        CreateFileSystemContextForTesting(nullptr, temp_dir_.GetPath());
    base::RunLoop run_loop;
    file_system_context_->OpenFileSystem(
        GURL("http://remote/"), storage::kFileSystemTypeTemporary,
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce(&FileSystemURLLoaderFactoryTest::OnOpenFileSystem,
                       run_loop.QuitWhenIdleClosure()));
    run_loop.Run();

    ContentBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    loader_.reset();
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&ShutdownFileSystemContextOnIOThread,
                       std::move(file_system_context_)));
    special_storage_policy_ = nullptr;
    // FileReader posts a task to close the file in destructor.
    base::RunLoop().RunUntilIdle();
    ContentBrowserTest::TearDownOnMainThread();
  }

  void SetUpAutoMountContext(base::FilePath* mnt_point) {
    *mnt_point = temp_dir_.GetPath().AppendASCII("auto_mount_dir");
    ASSERT_TRUE(base::CreateDirectory(*mnt_point));

    std::vector<std::unique_ptr<storage::FileSystemBackend>>
        additional_providers;
    additional_providers.push_back(std::make_unique<TestFileSystemBackend>(
        base::ThreadTaskRunnerHandle::Get().get(), *mnt_point));

    std::vector<storage::URLRequestAutoMountHandler> handlers = {
        base::BindRepeating(&TestAutoMountForURLRequest)};

    file_system_context_ = CreateFileSystemContextWithAutoMountersForTesting(
        nullptr, std::move(additional_providers), handlers,
        temp_dir_.GetPath());
  }

  void SetFileUpAutoMountContext() {
    base::FilePath mnt_point;
    SetUpAutoMountContext(&mnt_point);

    ASSERT_EQ(static_cast<int>(sizeof(kTestFileData)) - 1,
              base::WriteFile(mnt_point.AppendASCII("foo"), kTestFileData,
                              sizeof(kTestFileData) - 1));
  }

  FileSystemURL CreateURL(const base::FilePath& file_path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://remote"), storage::kFileSystemTypeTemporary, file_path);
  }

  void CreateDirectory(const base::StringPiece& dir_name) {
    base::FilePath path = base::FilePath().AppendASCII(dir_name);
    std::unique_ptr<FileSystemOperationContext> context(NewOperationContext());
    ASSERT_EQ(base::File::FILE_OK,
              file_util()->CreateDirectory(context.get(), CreateURL(path),
                                           false /* exclusive */,
                                           false /* recursive */));
  }

  void WriteFile(const base::StringPiece& file_name,
                 const char* buf,
                 int buf_size) {
    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://remote"), storage::kFileSystemTypeTemporary,
        base::FilePath().AppendASCII(file_name));
    ASSERT_EQ(base::File::FILE_OK,
              AsyncFileTestHelper::CreateFileWithData(
                  file_system_context_.get(), url, buf, buf_size));
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
    ASSERT_EQ(base::File::FILE_OK,
              file_util()->Truncate(context.get(), CreateURL(path), length));
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
                               "\",(0|1)," NUMBER ",\"" STR "\"," NUMBER
                               ",\"" STR "\"\\);</script>");
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

  std::unique_ptr<network::TestURLLoaderClient> TestLoad(const GURL& url) {
    auto client = TestLoadHelper(url, /*extra_headers=*/nullptr,
                                 file_system_context_.get());
    client->RunUntilComplete();
    return client;
  }

  std::unique_ptr<network::TestURLLoaderClient> TestLoadWithContext(
      const GURL& url,
      FileSystemContext* file_system_context) {
    auto client =
        TestLoadHelper(url, /*extra_headers=*/nullptr, file_system_context);
    client->RunUntilComplete();
    return client;
  }

  std::unique_ptr<network::TestURLLoaderClient> TestLoadWithHeaders(
      const GURL& url,
      const net::HttpRequestHeaders* extra_headers) {
    auto client =
        TestLoadHelper(url, extra_headers, file_system_context_.get());
    client->RunUntilComplete();
    return client;
  }

  std::unique_ptr<network::TestURLLoaderClient> TestLoadNoRun(const GURL& url) {
    return TestLoadHelper(url, /*extra_headers=*/nullptr,
                          file_system_context_.get());
  }

  // |temp_dir_| must be deleted last.
  base::ScopedTempDir temp_dir_;
  network::mojom::URLLoaderPtr loader_;

 private:
  static void OnOpenFileSystem(base::OnceClosure done_closure,
                               const GURL& root_url,
                               const std::string& name,
                               base::File::Error result) {
    ASSERT_EQ(base::File::FILE_OK, result);
    std::move(done_closure).Run();
  }

  storage::FileSystemFileUtil* file_util() {
    return file_system_context_->sandbox_delegate()->sync_file_util();
  }

  FileSystemOperationContext* NewOperationContext() {
    FileSystemOperationContext* context(
        new FileSystemOperationContext(file_system_context_.get()));
    context->set_allowed_bytes_growth(1024);
    return context;
  }

  RenderFrameHost* render_frame_host() const {
    return shell()->web_contents()->GetMainFrame();
  }

  // Starts |request| using |loader_factory| and sets |out_loader| and
  // |out_loader_client| to the resulting URLLoader and its URLLoaderClient. The
  // caller can then use functions like client.RunUntilComplete() to wait for
  // completion.
  void StartRequest(
      URLLoaderFactory* loader_factory,
      const network::ResourceRequest& request,
      network::mojom::URLLoaderPtr* out_loader,
      std::unique_ptr<network::TestURLLoaderClient>* out_loader_client) {
    *out_loader_client = std::make_unique<network::TestURLLoaderClient>();
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(out_loader), 0, 0, network::mojom::kURLLoadOptionNone,
        request, (*out_loader_client)->CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  content::WebContents* GetWebContents() { return shell()->web_contents(); }

  std::unique_ptr<network::TestURLLoaderClient> TestLoadHelper(
      const GURL& url,
      const net::HttpRequestHeaders* extra_headers,
      FileSystemContext* file_system_context) {
    network::ResourceRequest request;
    request.url = url;
    if (extra_headers)
      request.headers.MergeFrom(*extra_headers);
    const std::string storage_domain = url.GetOrigin().host();

    auto factory = content::CreateFileSystemURLLoaderFactory(
        render_frame_host(), /*is_navigation=*/false, file_system_context,
        storage_domain);
    std::unique_ptr<network::TestURLLoaderClient> client;
    StartRequest(factory.get(), request, &loader_, &client);
    return client;
  }

  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<FileSystemContext> file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemURLLoaderFactoryTest);
};

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, DirectoryListing) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CreateDirectory("foo");
  CreateDirectory("foo/bar");
  CreateDirectory("foo/bar/baz");

  EnsureFileExists("foo/bar/hoge");
  TruncateFile("foo/bar/hoge", 10);

  auto client = TestLoad(CreateFileSystemURL("foo/bar/"));

  ASSERT_TRUE(client->has_received_response());
  ASSERT_TRUE(client->has_received_completion());

  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_GT(response_text.size(), 0ul);

  std::istringstream in(response_text);
  std::string line;

  std::string listing_header;
  std::vector<std::string> listing_entries;
  while (!!std::getline(in, line)) {
    if (listing_header.empty() && IsDirectoryListingTitle(line)) {
      listing_header = line;
      continue;
    }
    if (IsDirectoryListingLine(line))
      listing_entries.push_back(line);
  }

#if defined(OS_WIN)
  EXPECT_EQ("<script>start(\"foo\\\\bar\");</script>", listing_header);
#elif defined(OS_POSIX)
  EXPECT_EQ("<script>start(\"/foo/bar\");</script>", listing_header);
#endif

  ASSERT_EQ(2U, listing_entries.size());
  std::sort(listing_entries.begin(), listing_entries.end());
  VerifyListingEntry(listing_entries[0], "baz", "baz", true, 0);
  VerifyListingEntry(listing_entries[1], "hoge", "hoge", false, 10);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, InvalidURL) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto client = TestLoad(GURL("filesystem:/foo/bar/baz"));
  ASSERT_FALSE(client->has_received_response());
  ASSERT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_URL, client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, NoSuchRoot) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto client = TestLoad(GURL("filesystem:http://remote/persistent/somedir/"));
  ASSERT_FALSE(client->has_received_response());
  ASSERT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, NoSuchDirectory) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto client = TestLoad(CreateFileSystemURL("somedir/"));
  ASSERT_FALSE(client->has_received_response());
  ASSERT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, Cancel) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CreateDirectory("foo");
  auto client = TestLoadNoRun(CreateFileSystemURL("foo/"));
  ASSERT_FALSE(client->has_received_response());
  ASSERT_FALSE(client->has_received_completion());

  client.reset();
  loader_.reset();
  base::RunLoop().RunUntilIdle();
  // If we get here, success! we didn't crash!
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, Incognito) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CreateDirectory("foo");

  scoped_refptr<FileSystemContext> file_system_context =
      CreateIncognitoFileSystemContextForTesting(nullptr, temp_dir_.GetPath());

  auto client =
      TestLoadWithContext(CreateFileSystemURL("/"), file_system_context.get());
  ASSERT_TRUE(client->has_received_response());
  ASSERT_TRUE(client->has_received_completion());

  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_GT(response_text.size(), 0ul);

  std::istringstream in(response_text);

  int num_entries = 0;
  std::string line;
  while (!!std::getline(in, line)) {
    if (IsDirectoryListingLine(line))
      num_entries++;
  }

  EXPECT_EQ(0, num_entries);

  client = TestLoadWithContext(CreateFileSystemURL("foo"),
                               file_system_context.get());
  ASSERT_FALSE(client->has_received_response());
  ASSERT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest,
                       AutoMountDirectoryListing) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath mnt_point;
  SetUpAutoMountContext(&mnt_point);
  EXPECT_TRUE(base::CreateDirectory(mnt_point));
  EXPECT_TRUE(base::CreateDirectory(mnt_point.AppendASCII("foo")));
  EXPECT_EQ(10,
            base::WriteFile(mnt_point.AppendASCII("bar"), "1234567890", 10));

  auto client =
      TestLoad(GURL("filesystem:http://automount/external/mnt_name/"));

  ASSERT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());

  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_GT(response_text.size(), 0ul);

  std::istringstream in(response_text);
  std::string line;
  EXPECT_TRUE(std::getline(in, line));  // |line| contains the temp dir path.

  // Result order is not guaranteed, so sort the results.
  std::vector<std::string> listing_entries;
  while (!!std::getline(in, line)) {
    if (IsDirectoryListingLine(line))
      listing_entries.push_back(line);
  }

  ASSERT_EQ(2U, listing_entries.size());
  std::sort(listing_entries.begin(), listing_entries.end());
  VerifyListingEntry(listing_entries[0], "bar", "bar", false, 10);
  VerifyListingEntry(listing_entries[1], "foo", "foo", true, -1);

  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          kValidExternalMountPoint));
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, AutoMountInvalidRoot) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath mnt_point;
  SetUpAutoMountContext(&mnt_point);
  auto client = TestLoad(GURL("filesystem:http://automount/external/invalid"));

  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);

  EXPECT_FALSE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          "invalid"));
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, AutoMountNoHandler) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath mnt_point;
  SetUpAutoMountContext(&mnt_point);
  auto client = TestLoad(GURL("filesystem:http://noauto/external/mnt_name"));

  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);

  EXPECT_FALSE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          kValidExternalMountPoint));
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, FileTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile("file1.dat", kTestFileData, base::size(kTestFileData) - 1);
  auto client = TestLoad(CreateFileSystemURL("file1.dat"));

  EXPECT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_EQ(kTestFileData, response_text);
  ASSERT_TRUE(client->response_head().headers) << "No response headers";
  EXPECT_EQ(200, client->response_head().headers->response_code());
  std::string cache_control;
  EXPECT_TRUE(client->response_head().headers->GetNormalizedHeader(
      "cache-control", &cache_control));
  EXPECT_EQ("no-cache", cache_control);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest,
                       FileTestFullSpecifiedRange) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const size_t buffer_size = 4000;
  std::unique_ptr<char[]> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);
  WriteFile("bigfile", buffer.get(), buffer_size);

  const size_t first_byte_position = 500;
  const size_t last_byte_position = buffer_size - first_byte_position;
  std::string partial_buffer_string(buffer.get() + first_byte_position,
                                    buffer.get() + last_byte_position + 1);

  net::HttpRequestHeaders headers;
  headers.SetHeader(
      net::HttpRequestHeaders::kRange,
      net::HttpByteRange::Bounded(first_byte_position, last_byte_position)
          .GetHeaderValue());
  auto client = TestLoadWithHeaders(CreateFileSystemURL("bigfile"), &headers);

  ASSERT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_TRUE(partial_buffer_string == response_text);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest,
                       FileTestHalfSpecifiedRange) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const size_t buffer_size = 4000;
  std::unique_ptr<char[]> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);
  WriteFile("bigfile", buffer.get(), buffer_size);

  const size_t first_byte_position = 500;
  std::string partial_buffer_string(buffer.get() + first_byte_position,
                                    buffer.get() + buffer_size);

  net::HttpRequestHeaders headers;
  headers.SetHeader(
      net::HttpRequestHeaders::kRange,
      net::HttpByteRange::RightUnbounded(first_byte_position).GetHeaderValue());
  auto client = TestLoadWithHeaders(CreateFileSystemURL("bigfile"), &headers);
  ASSERT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  // Don't use EXPECT_EQ, it will print out a lot of garbage if check failed.
  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_TRUE(partial_buffer_string == response_text);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest,
                       FileTestMultipleRangesNotSupported) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile("file1.dat", kTestFileData, base::size(kTestFileData) - 1);
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kRange,
                    "bytes=0-5,10-200,200-300");
  auto client = TestLoadWithHeaders(CreateFileSystemURL("file1.dat"), &headers);
  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, FileRangeOutOfBounds) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile("file1.dat", kTestFileData, base::size(kTestFileData) - 1);
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kRange,
                    net::HttpByteRange::Bounded(500, 1000).GetHeaderValue());
  auto client = TestLoadWithHeaders(CreateFileSystemURL("file1.dat"), &headers);

  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, FileDirRedirect) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CreateDirectory("dir");
  auto client = TestLoad(CreateFileSystemURL("dir"));

  EXPECT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_TRUE(client->has_received_redirect());
  EXPECT_EQ(301, client->redirect_info().status_code);
  EXPECT_EQ(CreateFileSystemURL("dir/"), client->redirect_info().new_url);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, FileNoSuchRoot) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto client = TestLoad(GURL("filesystem:http://remote/persistent/somefile"));
  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, NoSuchFile) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto client = TestLoad(CreateFileSystemURL("somefile"));
  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, FileCancel) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile("file1.dat", kTestFileData, base::size(kTestFileData) - 1);
  auto client = TestLoadNoRun(CreateFileSystemURL("file1.dat"));

  // client.reset();
  base::RunLoop().RunUntilIdle();
  // If we get here, success! we didn't crash!
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, FileGetMimeType) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string file_data =
      "<!DOCTYPE HTML><html><head>test</head>"
      "<body>foo</body></html>";
  const char kFilename[] = "hoge.html";
  WriteFile(kFilename, file_data.data(), file_data.size());

  std::string mime_type_direct;
  base::FilePath::StringType extension =
      base::FilePath().AppendASCII(kFilename).Extension();
  if (!extension.empty())
    extension = extension.substr(1);
  EXPECT_TRUE(
      net::GetWellKnownMimeTypeFromExtension(extension, &mime_type_direct));

  auto client = TestLoad(CreateFileSystemURL(kFilename));
  EXPECT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());

  EXPECT_EQ(mime_type_direct, client->response_head().mime_type);
  EXPECT_TRUE(client->response_head().did_mime_sniff);
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, FileIncognito) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile("file", kTestFileData, base::size(kTestFileData) - 1);

  // Creates a new filesystem context for incognito mode.
  scoped_refptr<FileSystemContext> file_system_context =
      CreateIncognitoFileSystemContextForTesting(nullptr, temp_dir_.GetPath());

  // The request should return NOT_FOUND error if it's in incognito mode.
  auto client = TestLoadWithContext(CreateFileSystemURL("file"),
                                    file_system_context.get());
  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);

  // Make sure it returns success with regular (non-incognito) context.
  client = TestLoad(CreateFileSystemURL("file"));
  ASSERT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_EQ(kTestFileData, response_text);
  EXPECT_EQ(200, client->response_head().headers->response_code());
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, FileAutoMountFileTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SetFileUpAutoMountContext();
  auto client =
      TestLoad(GURL("filesystem:http://automount/external/mnt_name/foo"));

  ASSERT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_EQ(kTestFileData, response_text);
  EXPECT_EQ(200, client->response_head().headers->response_code());

  std::string cache_control;
  EXPECT_TRUE(client->response_head().headers->GetNormalizedHeader(
      "cache-control", &cache_control));
  EXPECT_EQ("no-cache", cache_control);

  ASSERT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          kValidExternalMountPoint));
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest,
                       FileAutoMountInvalidRoot) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SetFileUpAutoMountContext();
  auto client =
      TestLoad(GURL("filesystem:http://automount/external/invalid/foo"));

  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);

  ASSERT_FALSE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          "invalid"));
}

IN_PROC_BROWSER_TEST_F(FileSystemURLLoaderFactoryTest, FileAutoMountNoHandler) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SetFileUpAutoMountContext();
  auto client =
      TestLoad(GURL("filesystem:http://noauto/external/mnt_name/foo"));

  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);

  ASSERT_FALSE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          kValidExternalMountPoint));
}

}  // namespace content
